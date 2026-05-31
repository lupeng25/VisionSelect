#include "license/LicenseManager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSysInfo>

#include <cstring>

#ifdef Q_OS_WIN
#include <windows.h>
#include <bcrypt.h>
#endif

namespace {
const char *kProductId = "VisionSelect";
const char *kSettingsKey = "license/key";

QString compactKey(QString key)
{
    key.remove(QChar::Space);
    key.remove(QChar::Tabulation);
    key.remove(QChar::LineFeed);
    key.remove(QChar::CarriageReturn);
    return key.trimmed();
}

QString groupedHex(const QByteArray &digest)
{
    const QString hex = QString::fromLatin1(digest.toHex().left(16)).toUpper();
    QStringList groups;
    for (int i = 0; i < hex.size(); i += 4)
        groups << hex.mid(i, 4);
    return groups.join(QLatin1Char('-'));
}

QStringList machineSeeds()
{
    QStringList seeds;
    const QByteArray uniqueId = QSysInfo::machineUniqueId();
    if (!uniqueId.isEmpty())
        seeds << QString::fromLatin1(uniqueId.toHex());

#ifdef Q_OS_WIN
    QSettings registry(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography"), QSettings::NativeFormat);
    const QString machineGuid = registry.value(QStringLiteral("MachineGuid")).toString().trimmed();
    if (!machineGuid.isEmpty())
        seeds << machineGuid;
#endif

    const QByteArray computerName = qgetenv("COMPUTERNAME");
    if (seeds.isEmpty() && !computerName.isEmpty())
        seeds << QString::fromLocal8Bit(computerName);
    return seeds;
}

QDate jsonDate(const QJsonObject &object, const QString &key)
{
    return QDate::fromString(object.value(key).toString(), Qt::ISODate);
}

#ifdef Q_OS_WIN
QByteArray rsaPublicBlob(const QByteArray &modulus, const QByteArray &exponent)
{
    BCRYPT_RSAKEY_BLOB header;
    header.Magic = BCRYPT_RSAPUBLIC_MAGIC;
    header.BitLength = static_cast<ULONG>(modulus.size() * 8);
    header.cbPublicExp = static_cast<ULONG>(exponent.size());
    header.cbModulus = static_cast<ULONG>(modulus.size());
    header.cbPrime1 = 0;
    header.cbPrime2 = 0;

    QByteArray blob;
    blob.resize(sizeof(BCRYPT_RSAKEY_BLOB));
    memcpy(blob.data(), &header, sizeof(BCRYPT_RSAKEY_BLOB));
    blob.append(exponent);
    blob.append(modulus);
    return blob;
}
#endif
}

LicenseManager::LicenseManager()
{
}

QString LicenseManager::machineCode() const
{
    return machineCodeForSeeds(machineSeeds());
}

LicenseStatus LicenseManager::currentStatus() const
{
    const QString key = storedLicenseKey();
    if (key.trimmed().isEmpty())
        return status(LicenseStatusCode::Missing, QCoreApplication::translate("LicenseManager", "No license key has been registered."));
    return validateKey(key);
}

LicenseStatus LicenseManager::validateKey(const QString &licenseKey) const
{
    return validateKeyForMachine(licenseKey, machineCode(), QDate::currentDate());
}

LicenseStatus LicenseManager::validateKeyForMachine(const QString &licenseKey,
                                                    const QString &expectedMachineCode,
                                                    const QDate &today) const
{
    QByteArray signedPayload;
    QByteArray payload;
    QByteArray signature;
    LicenseInfo info;
    QString error;
    if (!parseKey(licenseKey, &signedPayload, &payload, &signature, &info, &error))
        return status(LicenseStatusCode::InvalidFormat, error);

    if (!verifySignature(signedPayload, signature))
        return status(LicenseStatusCode::BadSignature, QCoreApplication::translate("LicenseManager", "The license signature is invalid."), info);
    if (info.productId != QLatin1String(kProductId))
        return status(LicenseStatusCode::ProductMismatch, QCoreApplication::translate("LicenseManager", "This license is not for VisionSelect."), info);
    if (info.machineCode.compare(expectedMachineCode, Qt::CaseInsensitive) != 0)
        return status(LicenseStatusCode::MachineMismatch, QCoreApplication::translate("LicenseManager", "This license is not bound to this machine."), info);
    if (info.expiresAt.isValid() && info.expiresAt < today)
        return status(LicenseStatusCode::Expired, QCoreApplication::translate("LicenseManager", "This license has expired."), info);
    return status(LicenseStatusCode::Valid, QCoreApplication::translate("LicenseManager", "License is valid."), info);
}

bool LicenseManager::saveLicenseKey(const QString &licenseKey, QString *errorMessage) const
{
    const LicenseStatus checked = validateKey(licenseKey);
    if (!checked.isValid()) {
        if (errorMessage)
            *errorMessage = checked.message;
        return false;
    }
    QSettings settings;
    settings.setValue(QString::fromLatin1(kSettingsKey), compactKey(licenseKey));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("LicenseManager", "Unable to save the license key.");
        return false;
    }
    return true;
}

QString LicenseManager::storedLicenseKey() const
{
    QSettings settings;
    return settings.value(QString::fromLatin1(kSettingsKey)).toString();
}

void LicenseManager::clearStoredLicense() const
{
    QSettings settings;
    settings.remove(QString::fromLatin1(kSettingsKey));
}

QString LicenseManager::machineCodeForSeeds(const QStringList &seeds)
{
    QStringList normalized;
    for (const QString &seed : seeds) {
        const QString trimmed = seed.trimmed();
        if (!trimmed.isEmpty())
            normalized << trimmed;
    }
    if (normalized.isEmpty())
        normalized << QStringLiteral("unknown-machine");
    normalized.sort(Qt::CaseInsensitive);
    const QByteArray material = normalized.join(QLatin1Char('|')).toUtf8()
        + QByteArray("|VisionSelect-License-v1");
    return groupedHex(QCryptographicHash::hash(material, QCryptographicHash::Sha256));
}

void LicenseManager::setPublicKeyForTesting(const QByteArray &modulus, const QByteArray &exponent)
{
    m_publicModulusOverride = modulus;
    m_publicExponentOverride = exponent;
}

bool LicenseManager::parseKey(const QString &licenseKey,
                              QByteArray *signedPayload,
                              QByteArray *payload,
                              QByteArray *signature,
                              LicenseInfo *info,
                              QString *errorMessage) const
{
    const QString key = compactKey(licenseKey);
    const QStringList parts = key.split(QLatin1Char('-'));
    if (parts.size() != 3 || parts.at(0) != QLatin1String("VS1")) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("LicenseManager", "The license key format is invalid.");
        return false;
    }

    const QByteArray payloadBase64 = parts.at(1).toLatin1();
    const QByteArray payloadBytes = QByteArray::fromBase64(payloadBase64);
    const QByteArray signatureBytes = QByteArray::fromBase64(parts.at(2).toLatin1());
    if (payloadBytes.isEmpty() || signatureBytes.isEmpty()) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("LicenseManager", "The license payload or signature is empty.");
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payloadBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("LicenseManager", "The license payload is not valid JSON.");
        return false;
    }

    const QJsonObject object = document.object();
    LicenseInfo parsed;
    parsed.productId = object.value(QStringLiteral("productId")).toString();
    parsed.licensee = object.value(QStringLiteral("licensee")).toString();
    parsed.serial = object.value(QStringLiteral("serial")).toString();
    parsed.machineCode = object.value(QStringLiteral("machineCode")).toString().toUpper();
    parsed.issuedAt = jsonDate(object, QStringLiteral("issuedAt"));
    parsed.expiresAt = jsonDate(object, QStringLiteral("expiresAt"));
    const QJsonArray features = object.value(QStringLiteral("features")).toArray();
    for (const QJsonValue &feature : features)
        parsed.features << feature.toString();

    if (parsed.productId.isEmpty() || parsed.licensee.isEmpty() || parsed.serial.isEmpty()
        || parsed.machineCode.isEmpty() || !parsed.expiresAt.isValid()) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("LicenseManager", "The license payload is missing required fields.");
        return false;
    }

    if (signedPayload)
        *signedPayload = payloadBase64;
    if (payload)
        *payload = payloadBytes;
    if (signature)
        *signature = signatureBytes;
    if (info)
        *info = parsed;
    return true;
}

bool LicenseManager::verifySignature(const QByteArray &signedPayload, const QByteArray &signature) const
{
    QByteArray modulus;
    QByteArray exponent;
    if (!publicKey(&modulus, &exponent) || modulus.isEmpty() || exponent.isEmpty())
        return false;

#ifdef Q_OS_WIN
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_KEY_HANDLE key = nullptr;
    bool ok = false;

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_RSA_ALGORITHM, nullptr, 0) == 0) {
        const QByteArray blob = rsaPublicBlob(modulus, exponent);
        if (BCryptImportKeyPair(algorithm, nullptr, BCRYPT_RSAPUBLIC_BLOB, &key,
                                reinterpret_cast<PUCHAR>(const_cast<char *>(blob.constData())),
                                static_cast<ULONG>(blob.size()), 0) == 0) {
            const QByteArray digest = QCryptographicHash::hash(signedPayload, QCryptographicHash::Sha256);
            BCRYPT_PKCS1_PADDING_INFO paddingInfo;
            paddingInfo.pszAlgId = BCRYPT_SHA256_ALGORITHM;
            ok = BCryptVerifySignature(key, &paddingInfo,
                                       reinterpret_cast<PUCHAR>(const_cast<char *>(digest.constData())),
                                       static_cast<ULONG>(digest.size()),
                                       reinterpret_cast<PUCHAR>(const_cast<char *>(signature.constData())),
                                       static_cast<ULONG>(signature.size()),
                                       BCRYPT_PAD_PKCS1) == 0;
            BCryptDestroyKey(key);
        }
        BCryptCloseAlgorithmProvider(algorithm, 0);
    }
    return ok;
#else
    Q_UNUSED(signedPayload)
    Q_UNUSED(signature)
    return false;
#endif
}

bool LicenseManager::publicKey(QByteArray *modulus, QByteArray *exponent) const
{
    if (!m_publicModulusOverride.isEmpty() && !m_publicExponentOverride.isEmpty()) {
        if (modulus)
            *modulus = m_publicModulusOverride;
        if (exponent)
            *exponent = m_publicExponentOverride;
        return true;
    }

    QFile file(QStringLiteral(":/license/public_key.json"));
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return false;
    const QJsonObject object = document.object();
    if (modulus)
        *modulus = QByteArray::fromBase64(object.value(QStringLiteral("modulus")).toString().toLatin1());
    if (exponent)
        *exponent = QByteArray::fromBase64(object.value(QStringLiteral("exponent")).toString().toLatin1());
    return true;
}

LicenseStatus LicenseManager::status(LicenseStatusCode code, const QString &message, const LicenseInfo &info)
{
    LicenseStatus result;
    result.code = code;
    result.message = message;
    result.info = info;
    return result;
}
