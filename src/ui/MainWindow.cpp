#include "ui/MainWindow.h"

#include "i18n/LanguageManager.h"
#include "license/LicenseManager.h"
#include "report/PdfReportWriter.h"
#include "selection/CalculationAssistant.h"
#include "selection/SelectionEngine.h"
#include "ui/CatalogDialogs.h"
#include "ui/UiHelpers.h"
#include "ui/pages/CalculationPage.h"
#include "ui/pages/CatalogPage.h"
#include "ui/pages/ComparisonPage.h"
#include "ui/pages/InputPage.h"
#include "ui/pages/PureCalculationPage.h"
#include "ui/pages/ReportPage.h"
#include "ui/pages/ResultsPage.h"
#include "ui/pages/ThreeDCameraPage.h"

#include <QDateTime>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSize>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>

using namespace UiHelpers;

namespace {
const int kPureCalculationPageIndex = 1;
const int kCalculationPageIndex = 2;
const int kThreeDCameraPageIndex = 3;
const int kResultsPageIndex = 4;
const int kComparisonPageIndex = 5;
const int kCatalogPageIndex = 6;
const int kReportPageIndex = 7;

QString bomSpecForCamera(const CameraSpec &camera, const SelectionResult &result)
{
    return QStringLiteral("%1 x %2, %3, %4, %5 fps, %6 MB/s")
        .arg(camera.resolutionX)
        .arg(camera.resolutionY)
        .arg(camera.pixelSizeUm, 0, 'f', 2)
        .arg(camera.interfaceType)
        .arg(camera.maxFps, 0, 'f', 1)
        .arg(result.interfaceCapacityMBps, 0, 'f', 1);
}

QString bomSpecForLens(const LensSpec &lens, const SelectionResult &result)
{
    if (lens.isTelecentric()) {
        return QStringLiteral("%1, PMAG %2x, WD %3 mm, DOF %4 mm, image %5 mm")
            .arg(lens.typeLabel())
            .arg(result.magnification, 0, 'f', 3)
            .arg(lens.nominalWorkingDistanceMm, 0, 'f', 1)
            .arg(result.estimatedDofMm, 0, 'f', 2)
            .arg(lens.imageCircleMm, 0, 'f', 1);
    }
    return QStringLiteral("%1, f %2 mm, min WD %3 mm, DOF %4 mm, image %5 mm")
        .arg(lens.typeLabel())
        .arg(lens.focalLengthMm, 0, 'f', 1)
        .arg(lens.minWorkingDistanceMm, 0, 'f', 1)
        .arg(result.estimatedDofMm, 0, 'f', 2)
        .arg(lens.imageCircleMm, 0, 'f', 1);
}

QString bomSpecForLight(const LightSpec &light, const SelectionResult &result)
{
    return QStringLiteral("%1, %2, %3, %4 x %5 mm, margin %6%")
        .arg(light.typeLabel())
        .arg(light.color)
        .arg(light.mode)
        .arg(light.activeWidthMm, 0, 'f', 0)
        .arg(light.activeHeightMm, 0, 'f', 0)
        .arg(result.lightCoverageMarginPercent, 0, 'f', 0);
}

QString csvCell(QString value)
{
    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + value + QLatin1Char('"');
}

void replaceStackPage(QStackedWidget *pages, int index, QWidget *page)
{
    if (!pages || !page || index < 0 || index >= pages->count())
        return;

    QWidget *placeholder = pages->widget(index);
    pages->removeWidget(placeholder);
    if (placeholder)
        placeholder->deleteLater();
    pages->insertWidget(index, page);
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QString error;
    if (!m_catalog.loadDefaults(&error))
        QMessageBox::critical(this, tr("Catalog Load Failed"), error);

    buildUi();
    connect(&LanguageManager::instance(), &LanguageManager::languageChanged, this, &MainWindow::rebuildPagesForLanguage);
}

void MainWindow::buildUi()
{
    QWidget *root = new QWidget(this);
    QHBoxLayout *rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    rootLayout->addWidget(createSidebar());

    m_pages = new QStackedWidget(root);
    m_inputPage = new InputPage;
    connect(m_inputPage, &InputPage::calculateRequested, this, &MainWindow::calculate);
    connect(m_inputPage, &InputPage::resultsRequested, this, [this]() {
        calculate();
        setActivePage(kResultsPageIndex);
    });
    m_pages->addWidget(m_inputPage);
    for (int i = 1; i <= kReportPageIndex; ++i)
        m_pages->addWidget(new QWidget);
    rootLayout->addWidget(m_pages, 1);

    setCentralWidget(root);
    retranslateUi();
    setActivePage(0);
}

QWidget *MainWindow::createSidebar()
{
    QFrame *sidebar = new QFrame;
    sidebar->setObjectName(QStringLiteral("Sidebar"));
    sidebar->setFixedWidth(266);

    QVBoxLayout *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);

    QFrame *brand = new QFrame(sidebar);
    brand->setObjectName(QStringLiteral("SidebarBrand"));
    QVBoxLayout *brandLayout = new QVBoxLayout(brand);
    brandLayout->setContentsMargins(14, 13, 14, 13);
    brandLayout->setSpacing(6);
    QLabel *title = new QLabel(QStringLiteral("VisionSelect"));
    title->setObjectName(QStringLiteral("AppTitle"));
    m_brandSubtitleLabel = new QLabel;
    m_brandSubtitleLabel->setObjectName(QStringLiteral("AppSubtitle"));
    m_brandSubtitleLabel->setWordWrap(true);
    m_brandBadgeLabel = new QLabel;
    m_brandBadgeLabel->setObjectName(QStringLiteral("AppBadge"));
    brandLayout->addWidget(title);
    brandLayout->addWidget(m_brandSubtitleLabel);
    brandLayout->addWidget(m_brandBadgeLabel, 0, Qt::AlignLeft);
    layout->addWidget(brand);

    m_languageCombo = new QComboBox(sidebar);
    m_languageCombo->setObjectName(QStringLiteral("SidebarLanguage"));
    for (const QString &language : LanguageManager::instance().availableLanguages())
        m_languageCombo->addItem(LanguageManager::instance().displayName(language), language);
    connect(m_languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        const QString language = m_languageCombo->itemData(index).toString();
        if (!language.isEmpty() && language != LanguageManager::instance().currentLanguage())
            LanguageManager::instance().setLanguage(language);
    });

    m_navTitleLabel = new QLabel;
    m_navTitleLabel->setObjectName(QStringLiteral("SidebarSectionLabel"));
    layout->addWidget(m_navTitleLabel);

    m_navButtons.clear();
    m_navButtons.resize(kReportPageIndex + 1);
    m_navSectionLabels.clear();
    const auto addSection = [this, layout](const QString &text) {
        QLabel *label = new QLabel(text);
        label->setObjectName(QStringLiteral("SidebarSectionLabel"));
        m_navSectionLabels.append(label);
        layout->addWidget(label);
    };
    const auto addNav = [this, layout](int pageIndex, const QString &text, const QString &iconPath) {
        QPushButton *button = new QPushButton(text);
        button->setObjectName(QStringLiteral("NavButton"));
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumHeight(38);
        button->setIcon(QIcon(iconPath));
        button->setIconSize(QSize(16, 16));
        button->setFocusPolicy(Qt::NoFocus);
        connect(button, &QPushButton::clicked, this, [this, pageIndex]() { setActivePage(pageIndex); });
        m_navButtons[pageIndex] = button;
        layout->addWidget(button);
    };

    addSection(localizedText("工作流", "Workflow"));
    addNav(0, navigationLabels().at(0), QStringLiteral(":/icons/ui/requirement.png"));
    addNav(kCalculationPageIndex, navigationLabels().at(kCalculationPageIndex), QStringLiteral(":/icons/ui/assistant.png"));
    addNav(kResultsPageIndex, navigationLabels().at(kResultsPageIndex), QStringLiteral(":/icons/ui/results.png"));
    addNav(kComparisonPageIndex, navigationLabels().at(kComparisonPageIndex), QStringLiteral(":/icons/ui/compare.png"));
    addNav(kReportPageIndex, navigationLabels().at(kReportPageIndex), QStringLiteral(":/icons/ui/report.png"));

    addSection(localizedText("工程工具", "Engineering Tools"));
    addNav(kPureCalculationPageIndex, navigationLabels().at(kPureCalculationPageIndex), QStringLiteral(":/icons/ui/calculate.png"));
    addNav(kThreeDCameraPageIndex, navigationLabels().at(kThreeDCameraPageIndex), QStringLiteral(":/icons/ui/camera3d.png"));

    addSection(localizedText("数据与系统", "Data and System"));
    addNav(kCatalogPageIndex, navigationLabels().at(kCatalogPageIndex), QStringLiteral(":/icons/ui/catalog.png"));

    layout->addStretch();
    QFrame *summaryBox = new QFrame(sidebar);
    summaryBox->setObjectName(QStringLiteral("SidebarSummary"));
    QVBoxLayout *summaryLayout = new QVBoxLayout(summaryBox);
    summaryLayout->setContentsMargins(12, 11, 12, 12);
    summaryLayout->setSpacing(7);

    QHBoxLayout *summaryHeader = new QHBoxLayout;
    summaryHeader->setContentsMargins(0, 0, 0, 0);
    summaryHeader->setSpacing(8);
    m_summaryTitleLabel = new QLabel;
    m_summaryTitleLabel->setObjectName(QStringLiteral("SidebarSummaryTitle"));
    m_summaryStatusLabel = new QLabel;
    m_summaryStatusLabel->setObjectName(QStringLiteral("SidebarStatusPill"));
    summaryHeader->addWidget(m_summaryTitleLabel, 1);
    summaryHeader->addWidget(m_summaryStatusLabel, 0, Qt::AlignRight);
    summaryLayout->addLayout(summaryHeader);

    m_summaryLabel = new QLabel;
    m_summaryLabel->setObjectName(QStringLiteral("SidebarSummaryValue"));
    m_summaryLabel->setWordWrap(true);
    summaryLayout->addWidget(m_summaryLabel);

    QGridLayout *statsLayout = new QGridLayout;
    statsLayout->setContentsMargins(0, 0, 0, 0);
    statsLayout->setHorizontalSpacing(6);
    statsLayout->setVerticalSpacing(6);
    const auto addStat = [statsLayout](int column, QLabel **label) {
        *label = new QLabel;
        (*label)->setObjectName(QStringLiteral("SidebarStat"));
        (*label)->setAlignment(Qt::AlignCenter);
        (*label)->setMinimumHeight(44);
        (*label)->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        (*label)->setWordWrap(true);
        statsLayout->addWidget(*label, 0, column);
    };
    addStat(0, &m_cameraCountLabel);
    addStat(1, &m_lensCountLabel);
    addStat(2, &m_lightCountLabel);
    summaryLayout->addLayout(statsLayout);

    m_licenseButton = new QPushButton(summaryBox);
    m_licenseButton->setObjectName(QStringLiteral("SidebarLicenseButton"));
    m_licenseButton->setIcon(QIcon(QStringLiteral(":/icons/ui/info.png")));
    m_licenseButton->setIconSize(QSize(15, 15));
    m_licenseButton->setCursor(Qt::PointingHandCursor);
    connect(m_licenseButton, &QPushButton::clicked, this, &MainWindow::showLicenseInfo);
    summaryLayout->addWidget(m_licenseButton);

    m_languageLabel = new QLabel(summaryBox);
    m_languageLabel->setObjectName(QStringLiteral("SidebarFieldLabel"));
    summaryLayout->addWidget(m_languageLabel);
    summaryLayout->addWidget(m_languageCombo);
    layout->addWidget(summaryBox);

    return sidebar;
}

QStringList MainWindow::navigationLabels() const
{
    return {
        localizedText("需求输入", "Requirement Input"),
        localizedText("纯计算", "Pure Calculation"),
        localizedText("产品计算", "Calculation Assistant"),
        localizedText("3D 相机", "3D Camera"),
        localizedText("推荐结果", "Recommended Results"),
        localizedText("方案对比", "Plan Comparison"),
        localizedText("参数库", "Catalog"),
        localizedText("PDF 报告", "PDF Report")
    };
}

void MainWindow::syncLanguageCombo()
{
    if (!m_languageCombo)
        return;
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

void MainWindow::refreshSidebarSummary()
{
    if (m_summaryTitleLabel)
        m_summaryTitleLabel->setText(localizedText("参数库", "Catalog"));
    if (m_summaryStatusLabel)
        m_summaryStatusLabel->setText(localizedText("就绪", "Ready"));
    if (m_summaryLabel)
        m_summaryLabel->setText(localizedText("推荐、对比和报告可用。",
                                             "Ready for recommendations, comparison, and reports."));

    const auto statText = [](const QString &label, int value) {
        return QStringLiteral("<span style=\"color:#9fb0c7; font-size:10px;\">%1</span><br>"
                              "<span style=\"color:#ffffff; font-size:15px; font-weight:800;\">%2</span>")
            .arg(label, QString::number(value));
    };
    if (m_cameraCountLabel)
        m_cameraCountLabel->setText(statText(localizedText("相机", "Cameras"), m_catalog.cameras().size()));
    if (m_lensCountLabel)
        m_lensCountLabel->setText(statText(localizedText("镜头", "Lenses"), m_catalog.lenses().size()));
    if (m_lightCountLabel)
        m_lightCountLabel->setText(statText(localizedText("光源", "Lights"), m_catalog.lights().size()));
    if (m_languageLabel)
        m_languageLabel->setText(localizedText("界面语言", "Language"));
}

void MainWindow::retranslateUi()
{
    setWindowTitle(tr("VisionSelect - Industrial Machine Vision Selection Assistant"));
    if (m_brandSubtitleLabel)
        m_brandSubtitleLabel->setText(tr("Industrial Machine Vision Selection Assistant"));
    if (m_brandBadgeLabel)
        m_brandBadgeLabel->setText(localizedText("需求 · 计算 · 选型", "Selection Workflow"));
    if (m_navTitleLabel)
        m_navTitleLabel->setText(localizedText("工作台", "Workbench"));
    const QStringList sections = {
        localizedText("工作流", "Workflow"),
        localizedText("工程工具", "Engineering Tools"),
        localizedText("数据与系统", "Data and System")
    };
    for (int i = 0; i < m_navSectionLabels.size() && i < sections.size(); ++i)
        m_navSectionLabels.at(i)->setText(sections.at(i));
    const QStringList labels = navigationLabels();
    for (int i = 0; i < m_navButtons.size() && i < labels.size(); ++i)
        if (m_navButtons.at(i))
            m_navButtons.at(i)->setText(labels.at(i));
    refreshSidebarSummary();
    if (m_licenseButton)
        m_licenseButton->setText(localizedText("授权信息", "License Info"));
    syncLanguageCombo();
}

void MainWindow::rebuildPagesForLanguage()
{
    if (!m_pages)
        return;

    const int currentIndex = m_pages->currentIndex();
    const SelectionRequest savedRequest = m_inputPage ? m_inputPage->request() : m_request;
    const bool hadPureCalculationPage = m_pureCalculationPage != nullptr;
    const bool hadCalculationPage = m_calculationPage != nullptr;
    const bool hadThreeDPage = m_threeDCameraPage != nullptr;
    const bool hadResultsPage = m_resultsPage != nullptr;
    const bool hadComparisonPage = m_comparisonPage != nullptr;
    const bool hadCatalogPage = m_catalogPage != nullptr;
    const bool hadReportPage = m_reportPage != nullptr;
    const bool hadResults = !m_results.isEmpty();

    m_inputPage = new InputPage;
    m_inputPage->setRequest(savedRequest);
    connect(m_inputPage, &InputPage::calculateRequested, this, &MainWindow::calculate);
    connect(m_inputPage, &InputPage::resultsRequested, this, [this]() {
        calculate();
        setActivePage(kResultsPageIndex);
    });
    replaceStackPage(m_pages, 0, m_inputPage);

    m_pureCalculationPage = nullptr;
    m_calculationPage = nullptr;
    m_threeDCameraPage = nullptr;
    m_resultsPage = nullptr;
    m_comparisonPage = nullptr;
    m_catalogPage = nullptr;
    m_reportPage = nullptr;
    m_catalogPageInitialized = false;
    m_request = savedRequest;

    if (hadPureCalculationPage)
        ensurePureCalculationPage();
    if (hadCalculationPage) {
        ensureCalculationPage();
        refreshCalculationAssistant();
    }
    if (hadThreeDPage) {
        ensureThreeDCameraPage();
        if (m_threeDCameraPage)
            m_threeDCameraPage->activate();
    }
    if (hadResultsPage)
        ensureResultsPage();
    if (hadComparisonPage)
        ensureComparisonPage();
    if (hadCatalogPage)
        ensureCatalogPageInitialized();
    if (hadReportPage)
        ensureReportPage();

    if (hadResults) {
        SelectionEngine engine;
        m_results = engine.select(m_request, m_catalog.cameras(), m_catalog.lenses(), m_catalog.lights(), 20);
        if (m_resultsPage)
            m_resultsPage->setResults(m_results, m_request);
        if (m_comparisonPage)
            m_comparisonPage->setResults(m_results);
        if (m_reportPage)
            m_reportPage->setReportData(m_request, m_results);
    }

    retranslateUi();
    setActivePage(currentIndex);
}

void MainWindow::showLicenseInfo()
{
    LicenseManager manager;
    const LicenseStatus status = manager.currentStatus();
    if (!status.isValid()) {
        QMessageBox::warning(this, tr("License Info"), status.message);
        return;
    }

    const LicenseInfo &info = status.info;
    QMessageBox::information(this, tr("License Info"),
        tr("Licensee: %1\nSerial: %2\nMachine code: %3\nExpires: %4")
            .arg(info.licensee,
                 info.serial,
                 info.machineCode,
                 info.expiresAt.toString(Qt::ISODate)));
}

void MainWindow::setActivePage(int index)
{
    if (!m_pages || index < 0 || index >= m_pages->count())
        return;
    if (index == kPureCalculationPageIndex) {
        ensurePureCalculationPage();
        if (m_pureCalculationPage)
            m_pureCalculationPage->refresh();
    }
    if (index == kCalculationPageIndex) {
        ensureCalculationPage();
        m_request = m_inputPage->request();
        refreshCalculationAssistant();
    }
    if (index == kThreeDCameraPageIndex) {
        ensureThreeDCameraPage();
        if (m_threeDCameraPage)
            m_threeDCameraPage->activate();
    }
    if (index == kResultsPageIndex)
        ensureResultsPage();
    if (index == kComparisonPageIndex)
        ensureComparisonPage();
    if (index == kCatalogPageIndex)
        ensureCatalogPageInitialized();
    if (index == kReportPageIndex)
        ensureReportPage();
    if ((index == kResultsPageIndex || index == kComparisonPageIndex || index == kReportPageIndex)
        && m_results.isEmpty()) {
        calculate();
    }
    m_pages->setCurrentIndex(index);
    for (int i = 0; i < m_navButtons.size(); ++i) {
        if (!m_navButtons.at(i))
            continue;
        m_navButtons.at(i)->setProperty("active", i == index);
        m_navButtons.at(i)->style()->unpolish(m_navButtons.at(i));
        m_navButtons.at(i)->style()->polish(m_navButtons.at(i));
    }
}

void MainWindow::ensurePureCalculationPage()
{
    if (m_pureCalculationPage || !m_pages)
        return;

    m_pureCalculationPage = new PureCalculationPage;
    replaceStackPage(m_pages, kPureCalculationPageIndex, m_pureCalculationPage);
}

void MainWindow::ensureCalculationPage()
{
    if (m_calculationPage || !m_pages)
        return;

    m_calculationPage = new CalculationPage;
    connect(m_calculationPage, &CalculationPage::recalculateRequested, this, [this]() {
        m_request = m_inputPage->request();
        refreshCalculationAssistant();
    });
    connect(m_calculationPage, &CalculationPage::inputRequested, this, [this]() { setActivePage(0); });
    connect(m_calculationPage, &CalculationPage::cameraSelectionChanged, this, [this](int row) {
        m_assistantSelectedCameraRow = row;
        refreshAssistantLensTable();
    });
    replaceStackPage(m_pages, kCalculationPageIndex, m_calculationPage);
}

void MainWindow::ensureResultsPage()
{
    if (m_resultsPage || !m_pages)
        return;

    m_resultsPage = new ResultsPage;
    connect(m_resultsPage, &ResultsPage::comparisonRequested, this, [this]() { setActivePage(kComparisonPageIndex); });
    replaceStackPage(m_pages, kResultsPageIndex, m_resultsPage);
    if (!m_results.isEmpty())
        m_resultsPage->setResults(m_results, m_request);
}

void MainWindow::ensureComparisonPage()
{
    if (m_comparisonPage || !m_pages)
        return;

    m_comparisonPage = new ComparisonPage;
    connect(m_comparisonPage, &ComparisonPage::recalculateRequested, this, &MainWindow::calculate);
    connect(m_comparisonPage, &ComparisonPage::exportBomRequested, this, &MainWindow::exportBomCsv);
    replaceStackPage(m_pages, kComparisonPageIndex, m_comparisonPage);
    if (!m_results.isEmpty())
        m_comparisonPage->setResults(m_results);
}

void MainWindow::ensureCatalogPage()
{
    if (m_catalogPage || !m_pages)
        return;

    m_catalogPage = new CatalogPage;
    connect(m_catalogPage, &CatalogPage::cameraAddRequested, this, &MainWindow::addCamera);
    connect(m_catalogPage, &CatalogPage::cameraEditRequested, this, &MainWindow::editCamera);
    connect(m_catalogPage, &CatalogPage::cameraRemoveRequested, this, &MainWindow::removeCamera);
    connect(m_catalogPage, &CatalogPage::cameraImportRequested, this, &MainWindow::importCameras);
    connect(m_catalogPage, &CatalogPage::cameraExportRequested, this, &MainWindow::exportCameras);
    connect(m_catalogPage, &CatalogPage::cameraExportFilteredRequested, this, &MainWindow::exportFilteredCameras);
    connect(m_catalogPage, &CatalogPage::cameraResetRequested, this, &MainWindow::resetCameras);
    connect(m_catalogPage, &CatalogPage::lensAddRequested, this, &MainWindow::addLens);
    connect(m_catalogPage, &CatalogPage::lensEditRequested, this, &MainWindow::editLens);
    connect(m_catalogPage, &CatalogPage::lensRemoveRequested, this, &MainWindow::removeLens);
    connect(m_catalogPage, &CatalogPage::lensImportRequested, this, &MainWindow::importLenses);
    connect(m_catalogPage, &CatalogPage::lensExportRequested, this, &MainWindow::exportLenses);
    connect(m_catalogPage, &CatalogPage::lensExportFilteredRequested, this, &MainWindow::exportFilteredLenses);
    connect(m_catalogPage, &CatalogPage::lensResetRequested, this, &MainWindow::resetLenses);
    connect(m_catalogPage, &CatalogPage::lightAddRequested, this, &MainWindow::addLight);
    connect(m_catalogPage, &CatalogPage::lightEditRequested, this, &MainWindow::editLight);
    connect(m_catalogPage, &CatalogPage::lightRemoveRequested, this, &MainWindow::removeLight);
    connect(m_catalogPage, &CatalogPage::lightImportRequested, this, &MainWindow::importLights);
    connect(m_catalogPage, &CatalogPage::lightExportRequested, this, &MainWindow::exportLights);
    connect(m_catalogPage, &CatalogPage::lightExportFilteredRequested, this, &MainWindow::exportFilteredLights);
    connect(m_catalogPage, &CatalogPage::lightResetRequested, this, &MainWindow::resetLights);
    replaceStackPage(m_pages, kCatalogPageIndex, m_catalogPage);
}

void MainWindow::ensureReportPage()
{
    if (m_reportPage || !m_pages)
        return;

    m_reportPage = new ReportPage;
    connect(m_reportPage, &ReportPage::exportPdfRequested, this, &MainWindow::exportReportPdf);
    connect(m_reportPage, &ReportPage::exportBomRequested, this, &MainWindow::exportBomCsv);
    connect(m_reportPage, &ReportPage::recalculateRequested, this, &MainWindow::calculate);
    replaceStackPage(m_pages, kReportPageIndex, m_reportPage);
    if (!m_results.isEmpty())
        m_reportPage->setReportData(m_request, m_results);
}

void MainWindow::ensureThreeDCameraPage()
{
    if (m_threeDCameraPage || !m_pages)
        return;

    m_threeDCameraPage = new ThreeDCameraPage;
    replaceStackPage(m_pages, kThreeDCameraPageIndex, m_threeDCameraPage);
}

void MainWindow::ensureCatalogPageInitialized()
{
    ensureCatalogPage();
    if (m_catalogPageInitialized || !m_catalogPage)
        return;

    m_catalogPage->setCatalog(&m_catalog);
    m_catalogPageInitialized = true;
}

void MainWindow::calculate()
{
    m_request = m_inputPage->request();
    SelectionEngine engine;
    m_results = engine.select(m_request, m_catalog.cameras(), m_catalog.lenses(), m_catalog.lights(), 20);
    if (m_pages && m_pages->currentIndex() == kCalculationPageIndex)
        refreshCalculationAssistant();
    if (m_resultsPage)
        m_resultsPage->setResults(m_results, m_request);
    if (m_comparisonPage)
        m_comparisonPage->setResults(m_results);
    refreshCatalogTables();
    if (m_reportPage)
        m_reportPage->setReportData(m_request, m_results);
    refreshSidebarSummary();
}

void MainWindow::refreshCalculationAssistant()
{
    if (!m_calculationPage)
        return;

    const RequirementEstimate requirement = CalculationAssistant::estimateRequirement(m_request);
    m_assistantCameraEstimates = CalculationAssistant::estimateCameras(m_request, m_catalog.cameras(), 12);

    m_calculationPage->setSummary(localizedText("需求 FOV：%1 x %2 mm，目标物方像素：%3 um/px，相机下限：%4 x %5（%6 MP）",
                                                "Required FOV: %1 x %2 mm, target object pixel: %3 um/px, minimum camera: %4 x %5 (%6 MP)")
        .arg(requirement.requiredFovWidthMm, 0, 'f', 2)
        .arg(requirement.requiredFovHeightMm, 0, 'f', 2)
        .arg(requirement.targetObjectPixelUm, 0, 'f', 2)
        .arg(requirement.requiredResolutionX)
        .arg(requirement.requiredResolutionY)
        .arg(requirement.requiredMegapixels, 0, 'f', 2));

    m_calculationPage->setCameraEstimates(m_assistantCameraEstimates);
    m_assistantSelectedCameraRow = m_calculationPage->selectedCameraEstimateRow();
    refreshAssistantLensTable();
}

void MainWindow::refreshAssistantLensTable()
{
    if (!m_calculationPage)
        return;

    const RequirementEstimate requirement = CalculationAssistant::estimateRequirement(m_request);
    QString details;
    details += localizedText("参数要求\n", "Requirements\n");
    details += localizedText("- 最低分辨率：%1 x %2，建议不低于 %3 MP。\n",
                             "- Minimum resolution: %1 x %2, recommended at least %3 MP.\n")
        .arg(requirement.requiredResolutionX)
        .arg(requirement.requiredResolutionY)
        .arg(requirement.requiredMegapixels, 0, 'f', 2);
    details += localizedText("- 12 bit 原始数据带宽估算：%1 MB/s @ %2 fps。\n",
                             "- Estimated 12-bit raw bandwidth: %1 MB/s @ %2 fps.\n")
        .arg(requirement.requiredBandwidthMBps12Bit, 0, 'f', 1)
        .arg(m_request.requiredFps, 0, 'f', 1);
    details += localizedText("- 镜头类型倾向：%1。\n", "- Lens type preference: %1.\n")
        .arg(requirement.telecentricPreferred
            ? localizedText("高精度/高度波动，优先评估远心镜头", "high precision / height variation, evaluate telecentric lenses first")
            : localizedText("普通工业镜头可以先粗算", "fixed-focal industrial lenses can be estimated first"));
    details += localizedText("- 运动曝光上限：%1。\n", "- Motion exposure limit: %1.\n")
        .arg(requirement.hasMotionConstraint
            ? QStringLiteral("%1 us").arg(requirement.maxExposureUsForOnePixelBlur, 0, 'f', 1)
            : localizedText("无运动模糊约束", "no motion blur constraint"));

    if (m_assistantSelectedCameraRow < 0 || m_assistantSelectedCameraRow >= m_assistantCameraEstimates.size()) {
        m_calculationPage->setLensEstimates(QVector<LensCalculationEstimate>());
        details += localizedText("\n镜头候选：暂无可用相机估算。", "\nLens candidates: no available camera estimate.");
        m_calculationPage->setDetails(details);
        return;
    }

    const CameraSpec camera = m_assistantCameraEstimates.at(m_assistantSelectedCameraRow).camera;
    m_assistantLensEstimates = CalculationAssistant::estimateLenses(m_request, camera, m_catalog.lenses(), 12);
    m_calculationPage->setLensEstimates(m_assistantLensEstimates);

    details += localizedText("\n当前镜头候选基于相机：%1。\n", "\nCurrent lens candidates are based on camera: %1.\n")
        .arg(productLabel(camera.manufacturer, camera.model));
    if (!m_assistantLensEstimates.isEmpty()) {
        const LensCalculationEstimate &top = m_assistantLensEstimates.first();
        details += localizedText("- 首选镜头：%1 %2，FOV %3 x %4 mm，物方像素 %5 um/px。\n",
                                 "- Top lens: %1 %2, FOV %3 x %4 mm, object pixel %5 um/px.\n")
            .arg(top.lens.manufacturer, top.lens.model)
            .arg(top.effectiveFovWidthMm, 0, 'f', 2)
            .arg(top.effectiveFovHeightMm, 0, 'f', 2)
            .arg(top.objectPixelSizeUm, 0, 'f', 2);
        details += localizedText("- 工程估算：DOF %1 mm，畸变边缘误差约 %2 um。\n",
                                 "- Engineering estimate: DOF %1 mm, distortion edge error about %2 um.\n")
            .arg(top.estimatedDofMm, 0, 'f', 2)
            .arg(top.distortionErrorUm, 0, 'f', 2);
        details += localizedText("- 公式：%1。\n", "- Formula: %1.\n").arg(top.formulaSummary);
        details += localizedText("- 推荐理由：%1。\n", "- Reasons: %1.\n")
            .arg(top.reasons.isEmpty() ? localizedText("按综合参数排名", "ranked by combined parameters") : top.reasons.join(localizedText("；", "; ")));
        details += localizedText("- 风险：%1", "- Risks: %1")
            .arg(top.risks.isEmpty() ? localizedText("无主要风险", "No major risk") : top.risks.join(localizedText("；", "; ")));
    } else {
        details += localizedText("- 没有符合当前限制的镜头候选。", "- No lens candidates match the current constraints.");
    }
    m_calculationPage->setDetails(details);
}
void MainWindow::refreshCatalogTables()
{
    if (m_catalogPageInitialized && m_catalogPage)
        m_catalogPage->setCatalog(&m_catalog);
    refreshSidebarSummary();
}

void MainWindow::importCameras()
{
    const QString path = QFileDialog::getOpenFileName(this, localizedText("导入相机 CSV", "Import Camera CSV"), QString(), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.loadCameraCsv(path, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::importLenses()
{
    const QString path = QFileDialog::getOpenFileName(this, localizedText("导入镜头 CSV", "Import Lens CSV"), QString(), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.loadLensCsv(path, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::importLights()
{
    const QString path = QFileDialog::getOpenFileName(this, localizedText("导入光源 CSV", "Import Light CSV"), QString(), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.loadLightCsv(path, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::exportCameras()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Camera CSV"), QStringLiteral("cameras.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.exportCameraCsv(path, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, tr("Export Complete"), path);
}

void MainWindow::exportLenses()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Lens CSV"), QStringLiteral("lenses.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.exportLensCsv(path, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, tr("Export Complete"), path);
}

void MainWindow::exportLights()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Light CSV"), QStringLiteral("lights.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.exportLightCsv(path, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, tr("Export Complete"), path);
}

void MainWindow::exportFilteredCameras()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Filtered Camera CSV"), QStringLiteral("cameras_filtered.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    const QVector<int> indexes = m_catalogPage ? m_catalogPage->visibleCameraCatalogIndexes() : QVector<int>();
    if (!m_catalog.exportCameraCsv(path, indexes, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, tr("Export Complete"), path);
}

void MainWindow::exportFilteredLenses()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Filtered Lens CSV"), QStringLiteral("lenses_filtered.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    const QVector<int> indexes = m_catalogPage ? m_catalogPage->visibleLensCatalogIndexes() : QVector<int>();
    if (!m_catalog.exportLensCsv(path, indexes, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, tr("Export Complete"), path);
}

void MainWindow::exportFilteredLights()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Filtered Light CSV"), QStringLiteral("lights_filtered.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    const QVector<int> indexes = m_catalogPage ? m_catalogPage->visibleLightCatalogIndexes() : QVector<int>();
    if (!m_catalog.exportLightCsv(path, indexes, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, tr("Export Complete"), path);
}

void MainWindow::addCamera()
{
    CameraSpec camera;
    camera.resolutionX = 2448;
    camera.resolutionY = 2048;
    camera.pixelSizeUm = 3.45;
    camera.sensorFormat = QStringLiteral("2/3\"");
    camera.colorMode = QStringLiteral("Mono");
    camera.shutterType = QStringLiteral("Global");
    camera.maxFps = 30.0;
    camera.interfaceType = QStringLiteral("USB3");
    camera.bandwidthMBps = 380.0;
    camera.bitDepth = 12.0;
    camera.dynamicRangeDb = 60.0;
    camera.lensMount = QStringLiteral("C");
    if (!editCameraDialog(this, &camera, tr("Add Camera")))
        return;
    QString error;
    if (!m_catalog.addCamera(camera, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::editCamera()
{
    const int index = m_catalogPage ? m_catalogPage->selectedCameraCatalogIndex() : -1;
    if (index < 0)
        return;
    CameraSpec camera = m_catalog.cameras().at(index);
    if (!editCameraDialog(this, &camera, tr("Edit Camera")))
        return;
    QString error;
    if (!m_catalog.updateCamera(index, camera, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::removeCamera()
{
    const int index = m_catalogPage ? m_catalogPage->selectedCameraCatalogIndex() : -1;
    if (index < 0)
        return;
    const CameraSpec camera = m_catalog.cameras().at(index);
    if (QMessageBox::question(this, tr("Delete Camera"),
            tr("Delete camera %1?").arg(productLabel(camera.manufacturer, camera.model))) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_catalog.removeCamera(index, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::addLens()
{
    LensSpec lens;
    lens.lensType = LensType::FixedFocal;
    lens.lensMount = QStringLiteral("C");
    lens.focalLengthMm = 25.0;
    lens.minWorkingDistanceMm = 100.0;
    lens.imageCircleMm = 11.0;
    lens.megapixelRating = 5.0;
    lens.recommendedMinPixelUm = 3.45;
    lens.fNumber = 2.8;
    if (!editLensDialog(this, &lens, tr("Add Lens")))
        return;
    QString error;
    if (!m_catalog.addLens(lens, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::editLens()
{
    const int index = m_catalogPage ? m_catalogPage->selectedLensCatalogIndex() : -1;
    if (index < 0)
        return;
    LensSpec lens = m_catalog.lenses().at(index);
    if (!editLensDialog(this, &lens, tr("Edit Lens")))
        return;
    QString error;
    if (!m_catalog.updateLens(index, lens, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::removeLens()
{
    const int index = m_catalogPage ? m_catalogPage->selectedLensCatalogIndex() : -1;
    if (index < 0)
        return;
    const LensSpec lens = m_catalog.lenses().at(index);
    if (QMessageBox::question(this, tr("Delete Lens"),
            tr("Delete lens %1?").arg(productLabel(lens.manufacturer, lens.model))) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_catalog.removeLens(index, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::addLight()
{
    LightSpec light;
    light.model = QStringLiteral("NEW-LIGHT");
    light.manufacturer = QStringLiteral("Manual");
    light.lightType = LightType::Ring;
    light.color = QStringLiteral("White");
    light.mode = QStringLiteral("Continuous");
    light.activeWidthMm = 100.0;
    light.activeHeightMm = 100.0;
    if (!editLightDialog(this, &light, tr("Add Light")))
        return;
    QString error;
    if (!m_catalog.addLight(light, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::editLight()
{
    const int index = m_catalogPage ? m_catalogPage->selectedLightCatalogIndex() : -1;
    if (index < 0)
        return;
    LightSpec light = m_catalog.lights().at(index);
    if (!editLightDialog(this, &light, tr("Edit Light")))
        return;
    QString error;
    if (!m_catalog.updateLight(index, light, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::removeLight()
{
    const int index = m_catalogPage ? m_catalogPage->selectedLightCatalogIndex() : -1;
    if (index < 0)
        return;
    const LightSpec light = m_catalog.lights().at(index);
    if (QMessageBox::question(this, tr("Delete Light"),
            tr("Delete light %1?").arg(productLabel(light.manufacturer, light.model))) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_catalog.removeLight(index, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::resetCameras()
{
    if (QMessageBox::question(this, tr("Reset Camera Catalog"),
            tr("Overwrite local camera catalog data with the built-in camera catalog?")) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_catalog.resetCamerasToBuiltIn(&error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::resetLenses()
{
    if (QMessageBox::question(this, tr("Reset Lens Catalog"),
            tr("Overwrite local lens catalog data with the built-in lens catalog?")) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_catalog.resetLensesToBuiltIn(&error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::resetLights()
{
    if (QMessageBox::question(this, tr("Reset Light Catalog"),
            tr("Overwrite local light catalog data with the built-in light catalog?")) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_catalog.resetLightsToBuiltIn(&error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::exportBomCsv()
{
    if (m_results.isEmpty())
        calculate();
    if (m_results.isEmpty()) {
        showError(tr("No recommended plan is available; BOM cannot be exported."));
        return;
    }

    const QString defaultName = QStringLiteral("VisionSelect_BOM_%1.csv")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(this, tr("Export BOM CSV"), defaultName, QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showError(tr("Unable to write BOM CSV: %1").arg(path));
        return;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << "scheme,rank,category,manufacturer,model,key_specs,notes\n";
    const int count = qMin(5, m_results.size());
    for (int i = 0; i < count; ++i) {
        const SelectionResult &r = m_results.at(i);
        const QString scheme = QStringLiteral("#%1 %2").arg(i + 1).arg(r.schemeTitle);
        out << csvCell(scheme) << "," << (i + 1) << ","
            << csvCell(tr("Camera")) << ","
            << csvCell(r.camera.manufacturer) << ","
            << csvCell(r.camera.model) << ","
            << csvCell(bomSpecForCamera(r.camera, r)) << ","
            << csvCell(QStringLiteral("FOV %1 x %2 mm; object pixel %3 um")
                .arg(r.effectiveFovWidthMm, 0, 'f', 2)
                .arg(r.effectiveFovHeightMm, 0, 'f', 2)
                .arg(r.objectPixelSizeUm, 0, 'f', 2))
            << "\n";
        out << csvCell(scheme) << "," << (i + 1) << ","
            << csvCell(tr("Lens")) << ","
            << csvCell(r.lens.manufacturer) << ","
            << csvCell(r.lens.model) << ","
            << csvCell(bomSpecForLens(r.lens, r)) << ","
            << csvCell(QStringLiteral("distortion %1 um; lens MP utilization %2%")
                .arg(r.distortionErrorUm, 0, 'f', 2)
                .arg(r.lensMegapixelUtilizationPercent, 0, 'f', 0))
            << "\n";
        out << csvCell(scheme) << "," << (i + 1) << ","
            << csvCell(tr("Light")) << ","
            << csvCell(r.light.manufacturer) << ","
            << csvCell(r.light.model) << ","
            << csvCell(bomSpecForLight(r.light, r)) << ","
            << csvCell(riskSummary(r))
            << "\n";
    }
    file.close();
    QMessageBox::information(this, tr("Export Complete"), path);
}

void MainWindow::exportReportPdf()
{
    if (m_results.isEmpty())
        calculate();

    const QString defaultName = QStringLiteral("VisionSelect_%1.pdf")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(this, localizedText("导出 PDF 报告", "Export PDF Report"), defaultName, QStringLiteral("PDF (*.pdf)"));
    if (path.isEmpty())
        return;

    PdfReportWriter writer;
    QString error;
    if (!writer.write(path, m_request, m_results, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, localizedText("导出完成", "Export Complete"),
        localizedText("PDF 报告已生成：\n%1", "PDF report generated:\n%1").arg(path));
}

void MainWindow::showError(const QString &message)
{
    QMessageBox::warning(this, localizedText("操作失败", "Operation Failed"), message);
}
