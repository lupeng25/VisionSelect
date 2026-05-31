#include "LicenseGeneratorWindow.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QTextStream>
#include <QVBoxLayout>

namespace {
QString csvCell(QString value)
{
    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + value + QLatin1Char('"');
}
}

LicenseGeneratorWindow::LicenseGeneratorWindow(QWidget *parent)
    : QWidget(parent),
      m_language(QStringLiteral("zh_CN"))
{
    QSettings settings;
    m_language = settings.value(QStringLiteral("licenseGenerator/language"), QStringLiteral("zh_CN")).toString();
    if (m_language != QLatin1String("en_US"))
        m_language = QStringLiteral("zh_CN");

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(22, 20, 22, 20);
    outer->setSpacing(14);

    QHBoxLayout *top = new QHBoxLayout;
    m_titleLabel = new QLabel(this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 5);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    top->addWidget(m_titleLabel, 1);

    m_languageCombo = new QComboBox(this);
    m_languageCombo->addItem(QString::fromUtf8("中文"), QStringLiteral("zh_CN"));
    m_languageCombo->addItem(QStringLiteral("English"), QStringLiteral("en_US"));
    top->addWidget(m_languageCombo);
    outer->addLayout(top);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    outer->addWidget(m_hintLabel);

    QGroupBox *keyBox = new QGroupBox(this);
    QVBoxLayout *keyLayout = new QVBoxLayout(keyBox);
    QHBoxLayout *keyRow = new QHBoxLayout;
    m_privateKeyLabel = new QLabel(keyBox);
    m_privateKeyEdit = new QLineEdit(keyBox);
    m_privateKeyEdit->setReadOnly(true);
    m_browseButton = new QPushButton(keyBox);
    keyRow->addWidget(m_privateKeyLabel);
    keyRow->addWidget(m_privateKeyEdit, 1);
    keyRow->addWidget(m_browseButton);
    m_privateKeyStatusLabel = new QLabel(keyBox);
    m_privateKeyStatusLabel->setWordWrap(true);
    keyLayout->addLayout(keyRow);
    keyLayout->addWidget(m_privateKeyStatusLabel);
    outer->addWidget(keyBox);

    QGroupBox *payloadBox = new QGroupBox(this);
    QFormLayout *form = new QFormLayout(payloadBox);
    form->setLabelAlignment(Qt::AlignLeft);
    m_machineCodeLabel = new QLabel(payloadBox);
    m_licenseeLabel = new QLabel(payloadBox);
    m_serialLabel = new QLabel(payloadBox);
    m_expiresLabel = new QLabel(payloadBox);
    m_featuresLabel = new QLabel(payloadBox);
    m_machineCodeEdit = new QLineEdit(payloadBox);
    m_licenseeEdit = new QLineEdit(payloadBox);
    m_serialEdit = new QLineEdit(LicenseIssuer::defaultSerial(), payloadBox);
    m_featuresEdit = new QLineEdit(QStringLiteral("standard"), payloadBox);
    m_expiresEdit = new QDateEdit(QDate::currentDate().addYears(1), payloadBox);
    m_expiresEdit->setCalendarPopup(true);
    m_expiresEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_expiresEdit->setMinimumDate(QDate::currentDate());
    m_newSerialButton = new QPushButton(payloadBox);

    QHBoxLayout *serialRow = new QHBoxLayout;
    serialRow->addWidget(m_serialEdit, 1);
    serialRow->addWidget(m_newSerialButton);
    form->addRow(m_machineCodeLabel, m_machineCodeEdit);
    form->addRow(m_licenseeLabel, m_licenseeEdit);
    form->addRow(m_serialLabel, serialRow);
    form->addRow(m_expiresLabel, m_expiresEdit);
    form->addRow(m_featuresLabel, m_featuresEdit);
    outer->addWidget(payloadBox);

    m_outputLabel = new QLabel(this);
    outer->addWidget(m_outputLabel);
    m_outputEdit = new QPlainTextEdit(this);
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setMinimumHeight(130);
    outer->addWidget(m_outputEdit, 1);

    QHBoxLayout *buttons = new QHBoxLayout;
    m_generateButton = new QPushButton(this);
    m_copyButton = new QPushButton(this);
    m_saveRecordButton = new QPushButton(this);
    buttons->addStretch();
    buttons->addWidget(m_generateButton);
    buttons->addWidget(m_copyButton);
    buttons->addWidget(m_saveRecordButton);
    outer->addLayout(buttons);

    connect(m_languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        const QString language = m_languageCombo->itemData(index).toString();
        if (language.isEmpty() || language == m_language)
            return;
        m_language = language;
        QSettings settings;
        settings.setValue(QStringLiteral("licenseGenerator/language"), m_language);
        retranslateUi();
    });
    connect(m_browseButton, &QPushButton::clicked, this, [this]() { browsePrivateKey(); });
    connect(m_generateButton, &QPushButton::clicked, this, [this]() { generateLicense(); });
    connect(m_copyButton, &QPushButton::clicked, this, [this]() { copyLicense(); });
    connect(m_saveRecordButton, &QPushButton::clicked, this, [this]() { saveRecord(); });
    connect(m_newSerialButton, &QPushButton::clicked, this, [this]() {
        m_serialEdit->setText(LicenseIssuer::defaultSerial());
    });

    for (int i = 0; i < m_languageCombo->count(); ++i) {
        if (m_languageCombo->itemData(i).toString() == m_language) {
            m_languageCombo->setCurrentIndex(i);
            break;
        }
    }
    retranslateUi();
    setStatus(text("请选择 XML 私钥文件。", "Select an XML private key file."), false);
}

QString LicenseGeneratorWindow::text(const char *zhUtf8, const char *enUtf8) const
{
    return m_language == QLatin1String("en_US")
        ? QString::fromUtf8(enUtf8)
        : QString::fromUtf8(zhUtf8);
}

QStringList LicenseGeneratorWindow::featureList() const
{
    QStringList features;
    for (const QString &feature : m_featuresEdit->text().split(QLatin1Char(','))) {
        const QString trimmed = feature.trimmed();
        if (!trimmed.isEmpty())
            features << trimmed;
    }
    if (features.isEmpty())
        features << QStringLiteral("standard");
    return features;
}

void LicenseGeneratorWindow::browsePrivateKey()
{
    const QString path = QFileDialog::getOpenFileName(this,
        text("选择 XML 私钥", "Select XML Private Key"),
        QString(),
        QStringLiteral("XML (*.xml);;All Files (*.*)"));
    if (path.isEmpty())
        return;
    m_privateKeyEdit->setText(path);
    loadPrivateKey();
}

bool LicenseGeneratorWindow::loadPrivateKey()
{
    const QString path = m_privateKeyEdit->text().trimmed();
    if (path.isEmpty()) {
        setStatus(text("请先选择私钥文件。", "Select a private key file first."), false);
        return false;
    }

    QString error;
    if (!m_issuer.loadPrivateKeyFile(path, &error)) {
        setStatus(text("私钥加载失败：", "Private key load failed: ") + error, false);
        return false;
    }
    setStatus(text("私钥已加载。", "Private key loaded."), true);
    return true;
}

void LicenseGeneratorWindow::generateLicense()
{
    if (!m_issuer.hasPrivateKey() && !loadPrivateKey())
        return;

    LicenseIssueRequest request;
    request.licensee = m_licenseeEdit->text();
    request.serial = m_serialEdit->text();
    request.machineCode = m_machineCodeEdit->text();
    request.issuedAt = QDate::currentDate();
    request.expiresAt = m_expiresEdit->date();
    request.features = featureList();

    QString error;
    LicenseIssueResult result;
    if (!m_issuer.issue(request, &result, &error)) {
        QMessageBox::warning(this, text("生成失败", "Generation Failed"), error);
        return;
    }

    m_lastRequest = request;
    m_lastResult = result;
    m_hasGeneratedLicense = true;
    m_machineCodeEdit->setText(result.normalizedMachineCode);
    m_outputEdit->setPlainText(result.licenseKey);
    setStatus(text("注册码已生成。", "License key generated."), true);
}

void LicenseGeneratorWindow::copyLicense()
{
    const QString key = m_outputEdit->toPlainText().trimmed();
    if (key.isEmpty()) {
        QMessageBox::information(this, text("没有注册码", "No License Key"),
            text("请先生成注册码。", "Generate a license key first."));
        return;
    }
    QApplication::clipboard()->setText(key);
    setStatus(text("注册码已复制到剪贴板。", "License key copied to clipboard."), true);
}

void LicenseGeneratorWindow::saveRecord()
{
    if (!m_hasGeneratedLicense) {
        QMessageBox::information(this, text("没有记录", "No Record"),
            text("请先生成注册码。", "Generate a license key first."));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(this,
        text("保存授权记录 CSV", "Save License Record CSV"),
        QStringLiteral("license_records.csv"),
        QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;

    const bool writeHeader = !QFile::exists(path) || QFileInfo(path).size() == 0;
    QFile file(path);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        QMessageBox::warning(this, text("保存失败", "Save Failed"),
            text("无法写入授权记录文件。", "Unable to write the license record file."));
        return;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");
    if (writeHeader)
        out << "issued_at,expires_at,licensee,serial,machine_code,features,license_key\n";
    out << csvCell(m_lastRequest.issuedAt.toString(Qt::ISODate)) << ","
        << csvCell(m_lastRequest.expiresAt.toString(Qt::ISODate)) << ","
        << csvCell(m_lastRequest.licensee.trimmed()) << ","
        << csvCell(m_lastRequest.serial.trimmed()) << ","
        << csvCell(m_lastResult.normalizedMachineCode) << ","
        << csvCell(featureList().join(QLatin1Char(';'))) << ","
        << csvCell(m_lastResult.licenseKey) << "\n";
    file.close();
    setStatus(text("授权记录已保存。", "License record saved."), true);
}

void LicenseGeneratorWindow::setStatus(const QString &message, bool ok)
{
    m_privateKeyStatusLabel->setText(message);
    m_privateKeyStatusLabel->setStyleSheet(ok
        ? QStringLiteral("color: #167c3a;")
        : QStringLiteral("color: #9a3412;"));
}

void LicenseGeneratorWindow::retranslateUi()
{
    setWindowTitle(text("VisionSelect 注册码生成器", "VisionSelect License Key Generator"));
    m_titleLabel->setText(text("VisionSelect 注册码生成器", "VisionSelect License Key Generator"));
    m_hintLabel->setText(text(
        "该工具仅供发布方内部使用。私钥只应保存在发布方电脑上，不要放入客户安装包。",
        "This tool is for internal publisher use only. Keep the private key on publisher machines and never include it in customer installers."));
    m_privateKeyLabel->setText(text("XML 私钥", "XML private key"));
    m_browseButton->setText(text("选择...", "Browse..."));
    m_machineCodeLabel->setText(text("机器码", "Machine code"));
    m_licenseeLabel->setText(text("客户名称", "Licensee"));
    m_serialLabel->setText(text("序列号", "Serial"));
    m_expiresLabel->setText(text("到期日", "Expires at"));
    m_featuresLabel->setText(text("功能项", "Features"));
    m_outputLabel->setText(text("注册码", "License key"));
    m_generateButton->setText(text("生成注册码", "Generate"));
    m_copyButton->setText(text("复制注册码", "Copy"));
    m_saveRecordButton->setText(text("保存记录 CSV", "Save CSV"));
    m_newSerialButton->setText(text("新序列号", "New"));
    m_machineCodeEdit->setPlaceholderText(text("客户软件注册窗口显示的机器码", "Machine code shown in the customer registration dialog"));
    m_licenseeEdit->setPlaceholderText(text("客户或公司名称", "Customer or company name"));
    m_featuresEdit->setPlaceholderText(QStringLiteral("standard"));
}
