#ifndef LICENSEMANAGER_H
#define LICENSEMANAGER_H

#include <QByteArray>
#include <QDate>
#include <QString>
#include <QStringList>

enum class LicenseStatusCode {
    Valid,
    Missing,
    InvalidFormat,
    BadSignature,
    ProductMismatch,
    MachineMismatch,
    Expired,
    StorageError
};

struct LicenseInfo {
    QString productId;
    QString licensee;
    QString serial;
    QString machineCode;
    QDate issuedAt;
    QDate expiresAt;
    QStringList features;
};

struct LicenseStatus {
    LicenseStatusCode code = LicenseStatusCode::Missing;
    QString message;
    LicenseInfo info;

    bool isValid() const { return code == LicenseStatusCode::Valid; }
};

class LicenseManager
{
public:
    LicenseManager();

    QString machineCode() const;
    LicenseStatus currentStatus() const;
    LicenseStatus validateKey(const QString &licenseKey) const;
    LicenseStatus validateKeyForMachine(const QString &licenseKey,
                                        const QString &expectedMachineCode,
                                        const QDate &today) const;

    bool saveLicenseKey(const QString &licenseKey, QString *errorMessage = nullptr) const;
    QString storedLicenseKey() const;
    void clearStoredLicense() const;

    static QString machineCodeForSeeds(const QStringList &seeds);

    void setPublicKeyForTesting(const QByteArray &modulus, const QByteArray &exponent);

private:
    QByteArray m_publicModulusOverride;
    QByteArray m_publicExponentOverride;

    bool parseKey(const QString &licenseKey,
                  QByteArray *signedPayload,
                  QByteArray *payload,
                  QByteArray *signature,
                  LicenseInfo *info,
                  QString *errorMessage) const;
    bool verifySignature(const QByteArray &signedPayload, const QByteArray &signature) const;
    bool publicKey(QByteArray *modulus, QByteArray *exponent) const;
    static LicenseStatus status(LicenseStatusCode code, const QString &message, const LicenseInfo &info = LicenseInfo());
};

#endif
