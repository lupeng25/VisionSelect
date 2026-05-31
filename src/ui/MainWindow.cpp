#include "ui/MainWindow.h"

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
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSize>
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
        QMessageBox::critical(this, QString::fromUtf8("\345\217\202\346\225\260\345\272\223\345\212\240\350\275\275\345\244\261\350\264\245"), error);

    buildUi();
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
    setWindowTitle(QString::fromUtf8("VisionSelect - \345\267\245\344\270\232\346\234\272\345\231\250\350\247\206\350\247\211\351\200\211\345\236\213\345\212\251\346\211\213"));
    setActivePage(0);
}

QWidget *MainWindow::createSidebar()
{
    QFrame *sidebar = new QFrame;
    sidebar->setObjectName(QStringLiteral("Sidebar"));
    sidebar->setFixedWidth(248);

    QVBoxLayout *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(10);

    QFrame *brand = new QFrame(sidebar);
    brand->setObjectName(QStringLiteral("SidebarBrand"));
    QVBoxLayout *brandLayout = new QVBoxLayout(brand);
    brandLayout->setContentsMargins(14, 14, 14, 14);
    brandLayout->setSpacing(6);
    QLabel *title = new QLabel(QStringLiteral("VisionSelect"));
    title->setObjectName(QStringLiteral("AppTitle"));
    QLabel *subtitle = new QLabel(QString::fromUtf8("\345\267\245\344\270\232\346\234\272\345\231\250\350\247\206\350\247\211\351\200\211\345\236\213\345\212\251\346\211\213"));
    subtitle->setObjectName(QStringLiteral("AppSubtitle"));
    subtitle->setWordWrap(true);
    QLabel *badge = new QLabel(QString::fromUtf8("\351\234\200\346\261\202 \302\267 \350\256\241\347\256\227 \302\267 \351\200\211\345\236\213"));
    badge->setObjectName(QStringLiteral("AppBadge"));
    brandLayout->addWidget(title);
    brandLayout->addWidget(subtitle);
    brandLayout->addWidget(badge, 0, Qt::AlignLeft);
    layout->addWidget(brand);

    QLabel *navTitle = new QLabel(QString::fromUtf8("\345\267\245\344\275\234\345\217\260"));
    navTitle->setObjectName(QStringLiteral("SidebarSectionLabel"));
    layout->addWidget(navTitle);

    const QStringList pages = {
        QString::fromUtf8("\351\234\200\346\261\202\350\276\223\345\205\245"),
        QString::fromUtf8("纯计算"),
        QString::fromUtf8("产品计算助手"),
        QString::fromUtf8("\346\216\250\350\215\220\347\273\223\346\236\234"),
        QString::fromUtf8("方案对比"),
        QString::fromUtf8("\345\217\202\346\225\260\345\272\223"),
        QString::fromUtf8("PDF \346\212\245\345\221\212")
    };
    const QVector<QStyle::StandardPixmap> icons = {
        QStyle::SP_FileDialogContentsView,
        QStyle::SP_FileDialogDetailedView,
        QStyle::SP_FileDialogInfoView,
        QStyle::SP_DialogApplyButton,
        QStyle::SP_ArrowRight,
        QStyle::SP_DirIcon,
        QStyle::SP_FileIcon
    };
    QStringList navPages = pages;
    navPages.insert(3, QString::fromUtf8("3D 相机助手"));
    QVector<QStyle::StandardPixmap> navIcons = icons;
    navIcons.insert(3, QStyle::SP_ComputerIcon);
    for (int i = 0; i < navPages.size(); ++i) {
        QPushButton *button = new QPushButton(navPages.at(i));
        button->setObjectName(QStringLiteral("NavButton"));
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumHeight(44);
        button->setIcon(style()->standardIcon(navIcons.value(i, QStyle::SP_FileIcon)));
        button->setIconSize(QSize(16, 16));
        button->setFocusPolicy(Qt::NoFocus);
        connect(button, &QPushButton::clicked, this, [this, i]() { setActivePage(i); });
        m_navButtons.append(button);
        layout->addWidget(button);
    }

    layout->addStretch();
    QFrame *summaryBox = new QFrame(sidebar);
    summaryBox->setObjectName(QStringLiteral("SidebarSummary"));
    QVBoxLayout *summaryLayout = new QVBoxLayout(summaryBox);
    summaryLayout->setContentsMargins(12, 12, 12, 12);
    summaryLayout->setSpacing(5);
    QLabel *summaryTitle = new QLabel(QString::fromUtf8("\344\272\247\345\223\201\345\272\223"));
    summaryTitle->setObjectName(QStringLiteral("SidebarSummaryTitle"));
    m_summaryLabel = new QLabel(m_catalog.summary());
    m_summaryLabel->setObjectName(QStringLiteral("SidebarSummaryValue"));
    m_summaryLabel->setWordWrap(true);
    summaryLayout->addWidget(summaryTitle);
    summaryLayout->addWidget(m_summaryLabel);
    layout->addWidget(summaryBox);

    return sidebar;
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
    if (m_summaryLabel)
        m_summaryLabel->setText(m_catalog.summary());
}

void MainWindow::refreshCalculationAssistant()
{
    if (!m_calculationPage)
        return;

    const RequirementEstimate requirement = CalculationAssistant::estimateRequirement(m_request);
    m_assistantCameraEstimates = CalculationAssistant::estimateCameras(m_request, m_catalog.cameras(), 12);

    m_calculationPage->setSummary(QString::fromUtf8("\351\234\200\346\261\202 FOV\357\274\232%1 x %2 mm\357\274\214\347\233\256\346\240\207\347\211\251\346\226\271\345\203\217\347\264\240\357\274\232%3 um/px\357\274\214\347\233\270\346\234\272\344\270\213\351\231\220\357\274\232%4 x %5\357\274\210%6 MP\357\274\211")
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
    details += QString::fromUtf8("\345\217\202\346\225\260\350\246\201\346\261\202\n");
    details += QString::fromUtf8("- \346\234\200\344\275\216\345\210\206\350\276\250\347\216\207\357\274\232%1 x %2\357\274\214\345\273\272\350\256\256\344\270\215\344\275\216\344\272\216 %3 MP\343\200\202\n")
        .arg(requirement.requiredResolutionX)
        .arg(requirement.requiredResolutionY)
        .arg(requirement.requiredMegapixels, 0, 'f', 2);
    details += QString::fromUtf8("- 12 bit \345\216\237\345\247\213\346\225\260\346\215\256\345\270\246\345\256\275\344\274\260\347\256\227\357\274\232%1 MB/s @ %2 fps\343\200\202\n")
        .arg(requirement.requiredBandwidthMBps12Bit, 0, 'f', 1)
        .arg(m_request.requiredFps, 0, 'f', 1);
    details += QString::fromUtf8("- \351\225\234\345\244\264\347\261\273\345\236\213\345\200\276\345\220\221\357\274\232%1\343\200\202\n")
        .arg(requirement.telecentricPreferred
            ? QString::fromUtf8("\351\253\230\347\262\276\345\272\246/\351\253\230\345\272\246\346\263\242\345\212\250\357\274\214\344\274\230\345\205\210\350\257\204\344\274\260\350\277\234\345\277\203\351\225\234\345\244\264")
            : QString::fromUtf8("\346\231\256\351\200\232\345\267\245\344\270\232\351\225\234\345\244\264\345\217\257\344\273\245\345\205\210\347\262\227\347\256\227"));
    details += QString::fromUtf8("- \350\277\220\345\212\250\346\233\235\345\205\211\344\270\212\351\231\220\357\274\232%1\343\200\202\n")
        .arg(requirement.hasMotionConstraint
            ? QStringLiteral("%1 us").arg(requirement.maxExposureUsForOnePixelBlur, 0, 'f', 1)
            : QString::fromUtf8("\346\227\240\350\277\220\345\212\250\346\250\241\347\263\212\347\272\246\346\235\237"));

    if (m_assistantSelectedCameraRow < 0 || m_assistantSelectedCameraRow >= m_assistantCameraEstimates.size()) {
        m_calculationPage->setLensEstimates(QVector<LensCalculationEstimate>());
        details += QString::fromUtf8("\n\351\225\234\345\244\264\345\200\231\351\200\211\357\274\232\346\232\202\346\227\240\345\217\257\347\224\250\347\233\270\346\234\272\344\274\260\347\256\227\343\200\202");
        m_calculationPage->setDetails(details);
        return;
    }

    const CameraSpec camera = m_assistantCameraEstimates.at(m_assistantSelectedCameraRow).camera;
    m_assistantLensEstimates = CalculationAssistant::estimateLenses(m_request, camera, m_catalog.lenses(), 12);
    m_calculationPage->setLensEstimates(m_assistantLensEstimates);

    details += QString::fromUtf8("\n\345\275\223\345\211\215\351\225\234\345\244\264\345\200\231\351\200\211\345\237\272\344\272\216\347\233\270\346\234\272\357\274\232%1\343\200\202\n")
        .arg(productLabel(camera.manufacturer, camera.model));
    if (!m_assistantLensEstimates.isEmpty()) {
        const LensCalculationEstimate &top = m_assistantLensEstimates.first();
        details += QString::fromUtf8("- \351\246\226\351\200\211\351\225\234\345\244\264\357\274\232%1 %2\357\274\214FOV %3 x %4 mm\357\274\214\347\211\251\346\226\271\345\203\217\347\264\240 %5 um/px\343\200\202\n")
            .arg(top.lens.manufacturer, top.lens.model)
            .arg(top.effectiveFovWidthMm, 0, 'f', 2)
            .arg(top.effectiveFovHeightMm, 0, 'f', 2)
            .arg(top.objectPixelSizeUm, 0, 'f', 2);
        details += QString::fromUtf8("- 工程估算：DOF %1 mm，畸变边缘误差约 %2 um。\n")
            .arg(top.estimatedDofMm, 0, 'f', 2)
            .arg(top.distortionErrorUm, 0, 'f', 2);
        details += QString::fromUtf8("- \345\205\254\345\274\217\357\274\232%1\343\200\202\n").arg(top.formulaSummary);
        details += QString::fromUtf8("- \346\216\250\350\215\220\347\220\206\347\224\261\357\274\232%1\343\200\202\n")
            .arg(top.reasons.isEmpty() ? QString::fromUtf8("\346\214\211\347\273\274\345\220\210\345\217\202\346\225\260\346\216\222\345\220\215") : top.reasons.join(QString::fromUtf8("\357\274\233")));
        details += QString::fromUtf8("- \351\243\216\351\231\251\357\274\232%1")
            .arg(top.risks.isEmpty() ? QString::fromUtf8("\346\227\240\344\270\273\350\246\201\351\243\216\351\231\251") : top.risks.join(QString::fromUtf8("\357\274\233")));
    } else {
        details += QString::fromUtf8("- \346\262\241\346\234\211\347\254\246\345\220\210\345\275\223\345\211\215\351\231\220\345\210\266\347\232\204\351\225\234\345\244\264\345\200\231\351\200\211\343\200\202");
    }
    m_calculationPage->setDetails(details);
}

void MainWindow::refreshCatalogTables()
{
    if (m_catalogPageInitialized && m_catalogPage)
        m_catalogPage->setCatalog(&m_catalog);
}

void MainWindow::importCameras()
{
    const QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8("\345\257\274\345\205\245\347\233\270\346\234\272 CSV"), QString(), QStringLiteral("CSV (*.csv)"));
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
    const QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8("\345\257\274\345\205\245\351\225\234\345\244\264 CSV"), QString(), QStringLiteral("CSV (*.csv)"));
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
    const QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8("\345\257\274\345\205\245\345\205\211\346\272\220 CSV"), QString(), QStringLiteral("CSV (*.csv)"));
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
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("导出相机 CSV"), QStringLiteral("cameras.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.exportCameraCsv(path, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, QString::fromUtf8("导出完成"), path);
}

void MainWindow::exportLenses()
{
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("导出镜头 CSV"), QStringLiteral("lenses.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.exportLensCsv(path, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, QString::fromUtf8("导出完成"), path);
}

void MainWindow::exportLights()
{
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("导出光源 CSV"), QStringLiteral("lights.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.exportLightCsv(path, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, QString::fromUtf8("导出完成"), path);
}

void MainWindow::exportFilteredCameras()
{
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("导出筛选相机 CSV"), QStringLiteral("cameras_filtered.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    const QVector<int> indexes = m_catalogPage ? m_catalogPage->visibleCameraCatalogIndexes() : QVector<int>();
    if (!m_catalog.exportCameraCsv(path, indexes, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, QString::fromUtf8("导出完成"), path);
}

void MainWindow::exportFilteredLenses()
{
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("导出筛选镜头 CSV"), QStringLiteral("lenses_filtered.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    const QVector<int> indexes = m_catalogPage ? m_catalogPage->visibleLensCatalogIndexes() : QVector<int>();
    if (!m_catalog.exportLensCsv(path, indexes, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, QString::fromUtf8("导出完成"), path);
}

void MainWindow::exportFilteredLights()
{
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("导出筛选光源 CSV"), QStringLiteral("lights_filtered.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    const QVector<int> indexes = m_catalogPage ? m_catalogPage->visibleLightCatalogIndexes() : QVector<int>();
    if (!m_catalog.exportLightCsv(path, indexes, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, QString::fromUtf8("导出完成"), path);
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
    if (!editCameraDialog(this, &camera, QString::fromUtf8("新增相机")))
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
    if (!editCameraDialog(this, &camera, QString::fromUtf8("编辑相机")))
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
    if (QMessageBox::question(this, QString::fromUtf8("删除相机"),
            QString::fromUtf8("确定删除相机：%1？").arg(productLabel(camera.manufacturer, camera.model)))
        != QMessageBox::Yes) {
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
    if (!editLensDialog(this, &lens, QString::fromUtf8("新增镜头")))
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
    if (!editLensDialog(this, &lens, QString::fromUtf8("编辑镜头")))
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
    if (QMessageBox::question(this, QString::fromUtf8("删除镜头"),
            QString::fromUtf8("确定删除镜头：%1？").arg(productLabel(lens.manufacturer, lens.model)))
        != QMessageBox::Yes) {
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
    light.manufacturer = QStringLiteral("VisionSelect Sample");
    light.lightType = LightType::Ring;
    light.color = QStringLiteral("White");
    light.mode = QStringLiteral("Continuous");
    light.activeWidthMm = 100.0;
    light.activeHeightMm = 100.0;
    if (!editLightDialog(this, &light, QString::fromUtf8("新增光源")))
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
    if (!editLightDialog(this, &light, QString::fromUtf8("编辑光源")))
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
    if (QMessageBox::question(this, QString::fromUtf8("删除光源"),
            QString::fromUtf8("确定删除光源：%1？").arg(productLabel(light.manufacturer, light.model)))
        != QMessageBox::Yes) {
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
    if (QMessageBox::question(this, QString::fromUtf8("重置相机库"),
            QString::fromUtf8("确定用内置相机库覆盖本地相机维护数据？")) != QMessageBox::Yes) {
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
    if (QMessageBox::question(this, QString::fromUtf8("重置镜头库"),
            QString::fromUtf8("确定用内置镜头库覆盖本地镜头维护数据？")) != QMessageBox::Yes) {
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
    if (QMessageBox::question(this, QString::fromUtf8("重置光源库"),
            QString::fromUtf8("确定用内置光源库覆盖本地光源维护数据？")) != QMessageBox::Yes) {
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
        showError(QString::fromUtf8("暂无推荐方案，无法导出 BOM。"));
        return;
    }

    const QString defaultName = QStringLiteral("VisionSelect_BOM_%1.csv")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("导出 BOM CSV"), defaultName, QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showError(QString::fromUtf8("无法写入 BOM CSV：%1").arg(path));
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
            << csvCell(QString::fromUtf8("相机")) << ","
            << csvCell(r.camera.manufacturer) << ","
            << csvCell(r.camera.model) << ","
            << csvCell(bomSpecForCamera(r.camera, r)) << ","
            << csvCell(QStringLiteral("FOV %1 x %2 mm; object pixel %3 um")
                .arg(r.effectiveFovWidthMm, 0, 'f', 2)
                .arg(r.effectiveFovHeightMm, 0, 'f', 2)
                .arg(r.objectPixelSizeUm, 0, 'f', 2))
            << "\n";
        out << csvCell(scheme) << "," << (i + 1) << ","
            << csvCell(QString::fromUtf8("镜头")) << ","
            << csvCell(r.lens.manufacturer) << ","
            << csvCell(r.lens.model) << ","
            << csvCell(bomSpecForLens(r.lens, r)) << ","
            << csvCell(QStringLiteral("distortion %1 um; lens MP utilization %2%")
                .arg(r.distortionErrorUm, 0, 'f', 2)
                .arg(r.lensMegapixelUtilizationPercent, 0, 'f', 0))
            << "\n";
        out << csvCell(scheme) << "," << (i + 1) << ","
            << csvCell(QString::fromUtf8("光源")) << ","
            << csvCell(r.light.manufacturer) << ","
            << csvCell(r.light.model) << ","
            << csvCell(bomSpecForLight(r.light, r)) << ","
            << csvCell(riskSummary(r))
            << "\n";
    }
    file.close();
    QMessageBox::information(this, QString::fromUtf8("导出完成"), path);
}

void MainWindow::exportReportPdf()
{
    if (m_results.isEmpty())
        calculate();

    const QString defaultName = QStringLiteral("VisionSelect_%1.pdf")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("\345\257\274\345\207\272 PDF \346\212\245\345\221\212"), defaultName, QStringLiteral("PDF (*.pdf)"));
    if (path.isEmpty())
        return;

    PdfReportWriter writer;
    QString error;
    if (!writer.write(path, m_request, m_results, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, QString::fromUtf8("\345\257\274\345\207\272\345\256\214\346\210\220"), QString::fromUtf8("PDF \346\212\245\345\221\212\345\267\262\347\224\237\346\210\220\357\274\232\n%1").arg(path));
}

void MainWindow::showError(const QString &message)
{
    QMessageBox::warning(this, QString::fromUtf8("\346\223\215\344\275\234\345\244\261\350\264\245"), message);
}
