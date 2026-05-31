#ifndef LICENSEDIALOG_H
#define LICENSEDIALOG_H

#include "license/LicenseManager.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;

class LicenseDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LicenseDialog(LicenseManager *licenseManager, QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;

private:
    LicenseManager *m_licenseManager = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_machineTitleLabel = nullptr;
    QLabel *m_machineCodeLabel = nullptr;
    QLabel *m_keyTitleLabel = nullptr;
    QPlainTextEdit *m_keyEdit = nullptr;
    QComboBox *m_languageCombo = nullptr;
    QPushButton *m_registerButton = nullptr;
    QPushButton *m_quitButton = nullptr;
    QPushButton *m_copyMachineButton = nullptr;

    void registerLicense();
    void syncLanguageCombo();
    void retranslateUi();
};

#endif
