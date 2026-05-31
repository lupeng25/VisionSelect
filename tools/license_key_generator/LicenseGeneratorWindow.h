#ifndef LICENSEGENERATORWINDOW_H
#define LICENSEGENERATORWINDOW_H

#include "license/LicenseIssuer.h"

#include <QWidget>

class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

class LicenseGeneratorWindow : public QWidget
{
public:
    explicit LicenseGeneratorWindow(QWidget *parent = nullptr);

private:
    QString m_language;
    LicenseIssuer m_issuer;
    LicenseIssueRequest m_lastRequest;
    LicenseIssueResult m_lastResult;
    bool m_hasGeneratedLicense = false;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_privateKeyLabel = nullptr;
    QLabel *m_privateKeyStatusLabel = nullptr;
    QLabel *m_machineCodeLabel = nullptr;
    QLabel *m_licenseeLabel = nullptr;
    QLabel *m_serialLabel = nullptr;
    QLabel *m_expiresLabel = nullptr;
    QLabel *m_featuresLabel = nullptr;
    QLabel *m_outputLabel = nullptr;
    QLabel *m_hintLabel = nullptr;

    QComboBox *m_languageCombo = nullptr;
    QLineEdit *m_privateKeyEdit = nullptr;
    QLineEdit *m_machineCodeEdit = nullptr;
    QLineEdit *m_licenseeEdit = nullptr;
    QLineEdit *m_serialEdit = nullptr;
    QLineEdit *m_featuresEdit = nullptr;
    QDateEdit *m_expiresEdit = nullptr;
    QPlainTextEdit *m_outputEdit = nullptr;
    QPushButton *m_browseButton = nullptr;
    QPushButton *m_generateButton = nullptr;
    QPushButton *m_copyButton = nullptr;
    QPushButton *m_saveRecordButton = nullptr;
    QPushButton *m_newSerialButton = nullptr;

    QString text(const char *zhUtf8, const char *enUtf8) const;
    QStringList featureList() const;
    void browsePrivateKey();
    bool loadPrivateKey();
    void generateLicense();
    void copyLicense();
    void saveRecord();
    void setStatus(const QString &message, bool ok);
    void retranslateUi();
};

#endif
