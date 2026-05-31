#ifndef LICENSEISSUER_H
#define LICENSEISSUER_H

#include <QByteArray>
#include <QDate>
#include <QString>
#include <QStringList>

struct LicenseIssueRequest {
    QString licensee;
    QString serial;
    QString machineCode;
    QDate issuedAt;
    QDate expiresAt;
    QStringList features;
};

struct LicenseIssueResult {
    QString licenseKey;
    QByteArray payloadJson;
    QByteArray payloadBase64;
    QByteArray signature;
    QString normalizedMachineCode;
};

class LicenseIssuer
{
public:
    LicenseIssuer();

    bool loadPrivateKeyFile(const QString &path, QString *errorMessage = nullptr);
    bool loadPrivateKeyXml(const QByteArray &xml, QString *errorMessage = nullptr);
    bool hasPrivateKey() const;
    void clear();

    bool issue(const LicenseIssueRequest &request,
               LicenseIssueResult *result,
               QString *errorMessage = nullptr) const;

    QByteArray publicModulus() const;
    QByteArray publicExponent() const;

    static QString defaultSerial();
    static QString normalizeMachineCode(QString machineCode);

private:
    QByteArray m_modulus;
    QByteArray m_exponent;
    QByteArray m_p;
    QByteArray m_q;
    QByteArray m_dp;
    QByteArray m_dq;
    QByteArray m_inverseQ;
    QByteArray m_d;

    bool signPayload(const QByteArray &payloadBase64,
                     QByteArray *signature,
                     QString *errorMessage) const;
};

#endif
