#include "ui/LicenseDialog.h"

#include "i18n/LanguageManager.h"
#include "ui/UiHelpers.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

using namespace UiHelpers;

LicenseDialog::LicenseDialog(LicenseManager *licenseManager, QWidget *parent)
    : QDialog(parent),
      m_licenseManager(licenseManager)
{
    setModal(true);
    setMinimumWidth(620);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 22);
    layout->setSpacing(14);

    QHBoxLayout *topActions = new QHBoxLayout;
    m_languageCombo = new QComboBox(this);
    topActions->addWidget(m_languageCombo);
    QWidget *topActionsWidget = new QWidget(this);
    topActionsWidget->setLayout(topActions);

    QWidget *header = new QWidget(this);
    header->setObjectName(QStringLiteral("PageHeader"));
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(12);
    m_titleLabel = new QLabel(header);
    m_titleLabel->setObjectName(QStringLiteral("PageTitle"));
    m_titleLabel->setWordWrap(true);
    headerLayout->addWidget(m_titleLabel, 1);
    headerLayout->addWidget(topActionsWidget, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(header);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("PageSubtitle"));
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    QFrame *machineBox = new QFrame(this);
    machineBox->setObjectName(QStringLiteral("SectionCard"));
    QVBoxLayout *machineLayout = new QVBoxLayout(machineBox);
    machineLayout->setContentsMargins(14, 12, 14, 12);
    machineLayout->setSpacing(8);
    m_machineTitleLabel = new QLabel(machineBox);
    m_machineTitleLabel->setObjectName(QStringLiteral("SectionTitle"));
    m_machineCodeLabel = new QLabel(machineBox);
    m_machineCodeLabel->setObjectName(QStringLiteral("MetricValue"));
    m_machineCodeLabel->setWordWrap(true);
    m_machineCodeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_copyMachineButton = actionButton(QString(), QStringLiteral(":/icons/ui/info.png"), true);
    connect(m_copyMachineButton, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_machineCodeLabel->text());
    });
    machineLayout->addWidget(m_machineTitleLabel);
    machineLayout->addWidget(m_machineCodeLabel);
    machineLayout->addWidget(m_copyMachineButton, 0, Qt::AlignLeft);
    layout->addWidget(machineBox);

    m_keyTitleLabel = new QLabel(this);
    m_keyTitleLabel->setObjectName(QStringLiteral("SectionTitle"));
    layout->addWidget(m_keyTitleLabel);
    m_keyEdit = new QPlainTextEdit(this);
    m_keyEdit->setMinimumHeight(128);
    m_keyEdit->setPlaceholderText(QStringLiteral("VS1-..."));
    layout->addWidget(m_keyEdit);

    QDialogButtonBox *buttons = new QDialogButtonBox(this);
    m_registerButton = buttons->addButton(QDialogButtonBox::Ok);
    m_quitButton = buttons->addButton(QDialogButtonBox::Cancel);
    connect(m_registerButton, &QPushButton::clicked, this, &LicenseDialog::registerLicense);
    connect(m_quitButton, &QPushButton::clicked, this, &LicenseDialog::reject);
    layout->addWidget(buttons);

    connect(&LanguageManager::instance(), &LanguageManager::languageChanged, this, &LicenseDialog::retranslateUi);
    connect(m_languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        const QString language = m_languageCombo->itemData(index).toString();
        if (!language.isEmpty() && language != LanguageManager::instance().currentLanguage())
            LanguageManager::instance().setLanguage(language);
    });

    for (const QString &language : LanguageManager::instance().availableLanguages())
        m_languageCombo->addItem(LanguageManager::instance().displayName(language), language);
    syncLanguageCombo();
    retranslateUi();
}

void LicenseDialog::changeEvent(QEvent *event)
{
    if (event && event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void LicenseDialog::registerLicense()
{
    if (!m_licenseManager)
        return;

    QString error;
    if (!m_licenseManager->saveLicenseKey(m_keyEdit->toPlainText(), &error)) {
        m_statusLabel->setText(error);
        QMessageBox::warning(this, tr("License registration failed"), error);
        return;
    }

    QMessageBox::information(this, tr("License registered"), tr("The license key has been saved."));
    accept();
}

void LicenseDialog::syncLanguageCombo()
{
    const QString language = LanguageManager::instance().currentLanguage();
    for (int i = 0; i < m_languageCombo->count(); ++i) {
        if (m_languageCombo->itemData(i).toString() == language) {
            m_languageCombo->blockSignals(true);
            m_languageCombo->setCurrentIndex(i);
            m_languageCombo->blockSignals(false);
            return;
        }
    }
}

void LicenseDialog::retranslateUi()
{
    const QString title = localizedText("VisionSelect 授权注册", "VisionSelect License");
    setWindowTitle(title);
    if (m_titleLabel)
        m_titleLabel->setText(title);
    m_statusLabel->setText(localizedText("请输入有效的离线注册码后继续使用。", "Enter a valid offline license key to continue."));
    m_machineTitleLabel->setText(tr("Machine code"));
    m_machineCodeLabel->setText(m_licenseManager ? m_licenseManager->machineCode() : QString());
    m_keyTitleLabel->setText(tr("License key"));
    m_registerButton->setText(tr("Register"));
    m_quitButton->setText(tr("Quit"));
    if (m_copyMachineButton)
        m_copyMachineButton->setText(tr("Copy machine code"));
    syncLanguageCombo();
}
