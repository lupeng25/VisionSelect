#include "license/LicenseIssuer.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QXmlStreamReader>

#include <cstring>

#ifdef Q_OS_WIN
#include <windows.h>
#include <bcrypt.h>
#endif

namespace {
const char *kProductId = "VisionSelect";

QString issuerError(const char *text)
{
    return QString::fromLatin1(text);
}

QByteArray compactBase64(QString text)
{
    text.remove(QChar::Space);
    text.remove(QChar::Tabulation);
    text.remove(QChar::LineFeed);
    text.remove(QChar::CarriageReturn);
    return text.toLatin1();
}

QByteArray readBase64Element(QXmlStreamReader *reader)
{
    return QByteArray::fromBase64(compactBase64(reader->readElementText()));
}

bool requirePart(const QByteArray &value, const char *name, QString *errorMessage)
{
    if (!value.isEmpty())
        return true;
    if (errorMessage)
        *errorMessage = QString::fromLatin1("Private key XML is missing %1.").arg(QString::fromLatin1(name));
    return false;
}

QStringList normalizedFeatures(const QStringList &features)
{
    QStringList normalized;
    for (const QString &feature : features) {
        const QString trimmed = feature.trimmed();
        if (!trimmed.isEmpty())
            normalized << trimmed;
    }
    if (normalized.isEmpty())
        normalized << QStringLiteral("standard");
    return normalized;
}

bool rsaPartFits(QByteArray value, int size)
{
    while (value.size() > size && value.startsWith('\0'))
        value.remove(0, 1);
    return value.size() <= size;
}

#ifdef Q_OS_WIN
QByteArray normalizedSize(QByteArray value, int size)
{
    while (value.size() > size && value.startsWith('\0'))
        value.remove(0, 1);
    if (value.size() < size)
        value.prepend(QByteArray(size - value.size(), '\0'));
    return value;
}

QString bcryptError(const char *message, NTSTATUS status)
{
    return QString::fromLatin1("%1 (0x%2)")
        .arg(QString::fromLatin1(message),
             QString::number(static_cast<ULONG>(status), 16).rightJustified(8, QLatin1Char('0')));
}

#endif
}

LicenseIssuer::LicenseIssuer()
{
}

bool LicenseIssuer::loadPrivateKeyFile(const QString &path, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        clear();
        if (errorMessage)
            *errorMessage = issuerError("Unable to read private key file.");
        return false;
    }
    return loadPrivateKeyXml(file.readAll(), errorMessage);
}

bool LicenseIssuer::loadPrivateKeyXml(const QByteArray &xml, QString *errorMessage)
{
    clear();
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement())
            continue;

        const QStringRef name = reader.name();
        if (name == QLatin1String("Modulus"))
            m_modulus = readBase64Element(&reader);
        else if (name == QLatin1String("Exponent"))
            m_exponent = readBase64Element(&reader);
        else if (name == QLatin1String("P"))
            m_p = readBase64Element(&reader);
        else if (name == QLatin1String("Q"))
            m_q = readBase64Element(&reader);
        else if (name == QLatin1String("DP"))
            m_dp = readBase64Element(&reader);
        else if (name == QLatin1String("DQ"))
            m_dq = readBase64Element(&reader);
        else if (name == QLatin1String("InverseQ"))
            m_inverseQ = readBase64Element(&reader);
        else if (name == QLatin1String("D"))
            m_d = readBase64Element(&reader);
    }

    if (reader.hasError()) {
        clear();
        if (errorMessage)
            *errorMessage = issuerError("Private key XML is not well formed.");
        return false;
    }

    if (!requirePart(m_modulus, "Modulus", errorMessage)
        || !requirePart(m_exponent, "Exponent", errorMessage)
        || !requirePart(m_p, "P", errorMessage)
        || !requirePart(m_q, "Q", errorMessage)
        || !requirePart(m_dp, "DP", errorMessage)
        || !requirePart(m_dq, "DQ", errorMessage)
        || !requirePart(m_inverseQ, "InverseQ", errorMessage)
        || !requirePart(m_d, "D", errorMessage)) {
        clear();
        return false;
    }

    const int modulusSize = m_modulus.size();
    const int primeSize = modulusSize / 2;
    if (modulusSize < 64 || modulusSize % 2 != 0
        || !rsaPartFits(m_p, primeSize)
        || !rsaPartFits(m_q, primeSize)
        || !rsaPartFits(m_dp, primeSize)
        || !rsaPartFits(m_dq, primeSize)
        || !rsaPartFits(m_inverseQ, primeSize)
        || !rsaPartFits(m_d, modulusSize)) {
        clear();
        if (errorMessage)
            *errorMessage = issuerError("Private key XML has inconsistent RSA component sizes.");
        return false;
    }

    return true;
}

bool LicenseIssuer::hasPrivateKey() const
{
    return !m_modulus.isEmpty() && !m_exponent.isEmpty() && !m_d.isEmpty();
}

void LicenseIssuer::clear()
{
    m_modulus.clear();
    m_exponent.clear();
    m_p.clear();
    m_q.clear();
    m_dp.clear();
    m_dq.clear();
    m_inverseQ.clear();
    m_d.clear();
}

bool LicenseIssuer::issue(const LicenseIssueRequest &request,
                          LicenseIssueResult *result,
                          QString *errorMessage) const
{
    if (!hasPrivateKey()) {
        if (errorMessage)
            *errorMessage = issuerError("Private key has not been loaded.");
        return false;
    }

    const QString licensee = request.licensee.trimmed();
    const QString machineCode = normalizeMachineCode(request.machineCode);
    const QString serial = request.serial.trimmed();
    const QDate issuedAt = request.issuedAt.isValid() ? request.issuedAt : QDate::currentDate();
    const QDate expiresAt = request.expiresAt;

    if (licensee.isEmpty()) {
        if (errorMessage)
            *errorMessage = issuerError("Licensee is required.");
        return false;
    }
    if (machineCode.isEmpty()) {
        if (errorMessage)
            *errorMessage = issuerError("Machine code is required.");
        return false;
    }
    if (serial.isEmpty()) {
        if (errorMessage)
            *errorMessage = issuerError("Serial is required.");
        return false;
    }
    if (!expiresAt.isValid() || expiresAt < QDate::currentDate()) {
        if (errorMessage)
            *errorMessage = issuerError("Expiration date must be today or later.");
        return false;
    }

    QJsonArray features;
    for (const QString &feature : normalizedFeatures(request.features))
        features.append(feature);

    QJsonObject payload;
    payload.insert(QStringLiteral("productId"), QString::fromLatin1(kProductId));
    payload.insert(QStringLiteral("licensee"), licensee);
    payload.insert(QStringLiteral("serial"), serial);
    payload.insert(QStringLiteral("machineCode"), machineCode);
    payload.insert(QStringLiteral("issuedAt"), issuedAt.toString(Qt::ISODate));
    payload.insert(QStringLiteral("expiresAt"), expiresAt.toString(Qt::ISODate));
    payload.insert(QStringLiteral("features"), features);

    const QByteArray payloadJson = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    const QByteArray payloadBase64 = payloadJson.toBase64();
    QByteArray signature;
    if (!signPayload(payloadBase64, &signature, errorMessage))
        return false;

    if (result) {
        result->payloadJson = payloadJson;
        result->payloadBase64 = payloadBase64;
        result->signature = signature;
        result->normalizedMachineCode = machineCode;
        result->licenseKey = QStringLiteral("VS1-%1-%2")
            .arg(QString::fromLatin1(payloadBase64),
                 QString::fromLatin1(signature.toBase64()));
    }
    return true;
}

QByteArray LicenseIssuer::publicModulus() const
{
    return m_modulus;
}

QByteArray LicenseIssuer::publicExponent() const
{
    return m_exponent;
}

QString LicenseIssuer::defaultSerial()
{
    return QStringLiteral("VS-%1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddHHmmss")));
}

QString LicenseIssuer::normalizeMachineCode(QString machineCode)
{
    machineCode.remove(QChar::Space);
    machineCode.remove(QChar::Tabulation);
    machineCode.remove(QChar::LineFeed);
    machineCode.remove(QChar::CarriageReturn);
    return machineCode.trimmed().toUpper();
}

bool LicenseIssuer::signPayload(const QByteArray &payloadBase64,
                                QByteArray *signature,
                                QString *errorMessage) const
{
#ifdef Q_OS_WIN
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_KEY_HANDLE key = nullptr;
    bool ok = false;

    NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_RSA_ALGORITHM, nullptr, 0);
    if (status != 0) {
        if (errorMessage)
            *errorMessage = bcryptError("Unable to open RSA provider.", status);
        return false;
    }

    const int modulusSize = m_modulus.size();
    const int primeSize = modulusSize / 2;

    BCRYPT_RSAKEY_BLOB header;
    header.Magic = BCRYPT_RSAFULLPRIVATE_MAGIC;
    header.BitLength = static_cast<ULONG>(modulusSize * 8);
    header.cbPublicExp = static_cast<ULONG>(m_exponent.size());
    header.cbModulus = static_cast<ULONG>(modulusSize);
    header.cbPrime1 = static_cast<ULONG>(primeSize);
    header.cbPrime2 = static_cast<ULONG>(primeSize);

    QByteArray blob;
    blob.resize(sizeof(BCRYPT_RSAKEY_BLOB));
    memcpy(blob.data(), &header, sizeof(BCRYPT_RSAKEY_BLOB));
    blob.append(m_exponent);
    blob.append(normalizedSize(m_modulus, modulusSize));
    blob.append(normalizedSize(m_p, primeSize));
    blob.append(normalizedSize(m_q, primeSize));
    blob.append(normalizedSize(m_dp, primeSize));
    blob.append(normalizedSize(m_dq, primeSize));
    blob.append(normalizedSize(m_inverseQ, primeSize));
    blob.append(normalizedSize(m_d, modulusSize));

    status = BCryptImportKeyPair(algorithm, nullptr, BCRYPT_RSAFULLPRIVATE_BLOB, &key,
                                 reinterpret_cast<PUCHAR>(blob.data()),
                                 static_cast<ULONG>(blob.size()), 0);
    if (status != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        if (errorMessage)
            *errorMessage = bcryptError("Unable to import RSA private key.", status);
        return false;
    }

    const QByteArray digest = QCryptographicHash::hash(payloadBase64, QCryptographicHash::Sha256);
    BCRYPT_PKCS1_PADDING_INFO paddingInfo;
    paddingInfo.pszAlgId = BCRYPT_SHA256_ALGORITHM;

    ULONG signatureSize = static_cast<ULONG>(modulusSize);
    QByteArray signedBytes;
    signedBytes.resize(modulusSize);
    status = BCryptSignHash(key, &paddingInfo,
                            reinterpret_cast<PUCHAR>(const_cast<char *>(digest.constData())),
                            static_cast<ULONG>(digest.size()),
                            reinterpret_cast<PUCHAR>(signedBytes.data()),
                            signatureSize, &signatureSize, BCRYPT_PAD_PKCS1);
    ok = status == 0;
    if (ok) {
        signedBytes.resize(static_cast<int>(signatureSize));
        if (signature)
            *signature = signedBytes;
    }

    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    if (!ok && errorMessage)
        *errorMessage = bcryptError("Unable to sign license payload.", status);
    return ok;
#else
    Q_UNUSED(payloadBase64)
    Q_UNUSED(signature)
    if (errorMessage)
        *errorMessage = issuerError("License signing is only supported on Windows.");
    return false;
#endif
}
