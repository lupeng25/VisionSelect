#include "license/LicenseManager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSysInfo>

#include <cstring>

#ifdef Q_OS_WIN
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#endif

namespace {
const char *kProductId = "VisionSelect";
const char *kSettingsKey = "license/key";
const char *kLastSeenDateKey = "license/lastSeenDate";

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

QByteArray protectClockValue(const QByteArray &plainText)
{
#ifdef Q_OS_WIN
    DATA_BLOB input;
    input.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(plainText.constData()));
    input.cbData = static_cast<DWORD>(plainText.size());
    const QByteArray entropyBytes("VisionSelect-LicenseClock-v1");
    DATA_BLOB entropy;
    entropy.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(entropyBytes.constData()));
    entropy.cbData = static_cast<DWORD>(entropyBytes.size());
    DATA_BLOB output = {0, nullptr};
    if (!CryptProtectData(&input, L"VisionSelect license clock", &entropy, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &output))
        return QByteArray();
    const QByteArray protectedValue(reinterpret_cast<const char *>(output.pbData), static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    return protectedValue;
#else
    const QByteArray proof = QCryptographicHash::hash(
        plainText + QByteArray("|VisionSelect-LicenseClock-v1"), QCryptographicHash::Sha256).toHex();
    return plainText.toBase64() + QByteArray(".") + proof;
#endif
}

QByteArray unprotectClockValue(const QByteArray &protectedValue)
{
#ifdef Q_OS_WIN
    DATA_BLOB input;
    input.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(protectedValue.constData()));
    input.cbData = static_cast<DWORD>(protectedValue.size());
    const QByteArray entropyBytes("VisionSelect-LicenseClock-v1");
    DATA_BLOB entropy;
    entropy.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(entropyBytes.constData()));
    entropy.cbData = static_cast<DWORD>(entropyBytes.size());
    DATA_BLOB output = {0, nullptr};
    if (!CryptUnprotectData(&input, nullptr, &entropy, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output))
        return QByteArray();
    const QByteArray plainText(reinterpret_cast<const char *>(output.pbData), static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    return plainText;
#else
    const int separator = protectedValue.indexOf('.');
    if (separator <= 0)
        return QByteArray();
    const QByteArray plainText = QByteArray::fromBase64(protectedValue.left(separator));
    const QByteArray expectedProof = QCryptographicHash::hash(
        plainText + QByteArray("|VisionSelect-LicenseClock-v1"), QCryptographicHash::Sha256).toHex();
    return expectedProof == protectedValue.mid(separator + 1) ? plainText : QByteArray();
#endif
}

bool readLastSeenDate(QDate *date, bool *exists)
{
    QSettings settings;
    const QString key = QString::fromLatin1(kLastSeenDateKey);
    const bool hasValue = settings.contains(key);
    if (exists)
        *exists = hasValue;
    if (!hasValue) {
        if (date)
            *date = QDate();
        return true;
    }

    const QByteArray protectedValue = QByteArray::fromBase64(settings.value(key).toByteArray());
    const QDate parsed = QDate::fromString(QString::fromLatin1(unprotectClockValue(protectedValue)), Qt::ISODate);
    if (!parsed.isValid())
        return false;
    if (date)
        *date = parsed;
    return true;
}

bool writeLastSeenDate(const QDate &date)
{
    const QByteArray protectedValue = protectClockValue(date.toString(Qt::ISODate).toLatin1());
    if (protectedValue.isEmpty())
        return false;
    QSettings settings;
    settings.setValue(QString::fromLatin1(kLastSeenDateKey), protectedValue.toBase64());
    settings.sync();
    return settings.status() == QSettings::NoError;
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
    const QDate today = QDate::currentDate();
    QDate lastSeenDate;
    bool markerExists = false;
    if (!readLastSeenDate(&lastSeenDate, &markerExists))
        return status(LicenseStatusCode::StorageError,
                      QCoreApplication::translate("LicenseManager", "The protected license clock is unreadable."));

    const LicenseStatus checked = validateKeyForMachine(licenseKey, machineCode(), today, lastSeenDate);
    if (!checked.isValid())
        return checked;
    if ((!markerExists || today > lastSeenDate) && !writeLastSeenDate(today))
        return status(LicenseStatusCode::StorageError,
                      QCoreApplication::translate("LicenseManager", "Unable to update the protected license clock."),
                      checked.info);
    return checked;
}

LicenseStatus LicenseManager::validateKeyForMachine(const QString &licenseKey,
                                                    const QString &expectedMachineCode,
                                                    const QDate &today) const
{
    return validateKeyForMachine(licenseKey, expectedMachineCode, today, QDate());
}

LicenseStatus LicenseManager::validateKeyForMachine(const QString &licenseKey,
                                                    const QString &expectedMachineCode,
                                                    const QDate &today,
                                                    const QDate &lastSeenDate) const
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
    if ((info.issuedAt.isValid() && today < info.issuedAt)
        || (lastSeenDate.isValid() && today < lastSeenDate)) {
        return status(LicenseStatusCode::ClockRollback,
                      QCoreApplication::translate("LicenseManager", "The system date is earlier than the protected license clock."), info);
    }
    return status(LicenseStatusCode::Valid, QCoreApplication::translate("LicenseManager", "License is valid."), info);
}

bool LicenseManager::saveLicenseKey(const QString &licenseKey, QString *errorMessage) const
{
    LicenseStatus checked = validateKey(licenseKey);
    if (checked.code == LicenseStatusCode::StorageError) {
        QString repairError;
        if (!repairClockState(licenseKey, &repairError)) {
            if (errorMessage)
                *errorMessage = repairError;
            return false;
        }
        checked = validateKey(licenseKey);
    }
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

void LicenseManager::removeInstalledLicense() const
{
    QSettings settings;
    settings.remove(QString::fromLatin1(kSettingsKey));
}

bool LicenseManager::repairClockState(const QString &licenseKey, QString *errorMessage) const
{
    const LicenseStatus checked = validateKeyForMachine(licenseKey, machineCode(), QDate::currentDate());
    if (!checked.isValid()) {
        if (errorMessage)
            *errorMessage = checked.message;
        return false;
    }
    if (!writeLastSeenDate(QDate::currentDate())) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("LicenseManager", "Unable to repair the protected license clock.");
        return false;
    }
    return true;
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
    if (parts.size() != 3 || parts.at(0) != QLatin1String("VS2")) {
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
    if (parsed.productId.isEmpty() || parsed.licensee.isEmpty() || parsed.serial.isEmpty()
        || parsed.machineCode.isEmpty() || !parsed.issuedAt.isValid() || !parsed.expiresAt.isValid()
        || parsed.issuedAt > parsed.expiresAt) {
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
