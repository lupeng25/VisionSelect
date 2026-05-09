#include "ui/MainWindow.h"

#include "report/PdfReportWriter.h"
#include "selection/SelectionEngine.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {
QLabel *pageTitle(const QString &text, const QString &subtitle = QString())
{
    QWidget *unused = nullptr;
    Q_UNUSED(unused)
    QLabel *label = new QLabel(text);
    label->setObjectName(QStringLiteral("PageTitle"));
    if (!subtitle.isEmpty())
        label->setToolTip(subtitle);
    return label;
}

QTableWidgetItem *item(const QString &text)
{
    QTableWidgetItem *tableItem = new QTableWidgetItem(text);
    tableItem->setFlags(tableItem->flags() ^ Qt::ItemIsEditable);
    return tableItem;
}

QString number(double value, int decimals = 2)
{
    return QString::number(value, 'f', decimals);
}

void setupTable(QTableWidget *table)
{
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QString error;
    if (!m_catalog.loadDefaults(&error))
        QMessageBox::critical(this, QString::fromUtf8("\345\217\202\346\225\260\345\272\223\345\212\240\350\275\275\345\244\261\350\264\245"), error);

    buildUi();
    calculate();
}

void MainWindow::buildUi()
{
    QWidget *root = new QWidget(this);
    QHBoxLayout *rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    rootLayout->addWidget(createSidebar());

    m_pages = new QStackedWidget(root);
    m_pages->addWidget(createInputPage());
    m_pages->addWidget(createResultsPage());
    m_pages->addWidget(createCatalogPage());
    m_pages->addWidget(createReportPage());
    rootLayout->addWidget(m_pages, 1);

    setCentralWidget(root);
    setWindowTitle(QString::fromUtf8("VisionSelect - \345\267\245\344\270\232\346\234\272\345\231\250\350\247\206\350\247\211\351\200\211\345\236\213\345\212\251\346\211\213"));
    setActivePage(0);
}

QWidget *MainWindow::createSidebar()
{
    QFrame *sidebar = new QFrame;
    sidebar->setObjectName(QStringLiteral("Sidebar"));
    sidebar->setFixedWidth(230);

    QVBoxLayout *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(16, 18, 16, 18);
    layout->setSpacing(8);

    QLabel *title = new QLabel(QStringLiteral("VisionSelect"));
    title->setObjectName(QStringLiteral("AppTitle"));
    QLabel *subtitle = new QLabel(QString::fromUtf8("\345\267\245\344\270\232\346\234\272\345\231\250\350\247\206\350\247\211\351\200\211\345\236\213\345\212\251\346\211\213"));
    subtitle->setObjectName(QStringLiteral("AppSubtitle"));
    layout->addWidget(title);
    layout->addWidget(subtitle);

    const QStringList pages = {
        QString::fromUtf8("\351\234\200\346\261\202\350\276\223\345\205\245"),
        QString::fromUtf8("\346\216\250\350\215\220\347\273\223\346\236\234"),
        QString::fromUtf8("\345\217\202\346\225\260\345\272\223"),
        QString::fromUtf8("PDF \346\212\245\345\221\212")
    };
    for (int i = 0; i < pages.size(); ++i) {
        QPushButton *button = new QPushButton(pages.at(i));
        button->setObjectName(QStringLiteral("NavButton"));
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, [this, i]() { setActivePage(i); });
        m_navButtons.append(button);
        layout->addWidget(button);
    }

    layout->addStretch();
    m_summaryLabel = new QLabel(m_catalog.summary());
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setStyleSheet(QStringLiteral("color:#9fb2cf; padding:8px 10px;"));
    layout->addWidget(m_summaryLabel);

    return sidebar;
}

QWidget *MainWindow::createInputPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *outer = new QVBoxLayout(page);
    outer->setContentsMargins(28, 24, 28, 24);
    outer->setSpacing(16);
    outer->addWidget(pageTitle(QString::fromUtf8("\351\234\200\346\261\202\350\276\223\345\205\245")));

    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *content = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 12, 0);
    layout->setSpacing(14);

    QGroupBox *objectBox = new QGroupBox(QString::fromUtf8("\345\267\245\344\273\266\344\270\216\347\262\276\345\272\246"));
    QFormLayout *objectLayout = new QFormLayout(objectBox);
    objectLayout->setLabelAlignment(Qt::AlignLeft);
    m_widthSpin = makeSpin(0.1, 2000.0, 20.0, QStringLiteral(" mm"));
    m_heightSpin = makeSpin(0.1, 2000.0, 20.0, QStringLiteral(" mm"));
    m_marginSpin = makeSpin(0.0, 200.0, 2.0, QStringLiteral(" mm"));
    m_minFeatureSpin = makeSpin(0.1, 10000.0, 50.0, QStringLiteral(" um"));
    m_toleranceSpin = makeSpin(0.1, 10000.0, 10.0, QStringLiteral(" um"));
    objectLayout->addRow(QString::fromUtf8("\345\267\245\344\273\266\345\256\275\345\272\246"), m_widthSpin);
    objectLayout->addRow(QString::fromUtf8("\345\267\245\344\273\266\351\253\230\345\272\246"), m_heightSpin);
    objectLayout->addRow(QString::fromUtf8("\345\256\232\344\275\215/\350\243\205\345\244\271\344\275\231\351\207\217"), m_marginSpin);
    objectLayout->addRow(QString::fromUtf8("\346\234\200\345\260\217\347\211\271\345\276\201\345\260\272\345\257\270"), m_minFeatureSpin);
    objectLayout->addRow(QString::fromUtf8("\345\205\201\350\256\270\346\265\213\351\207\217\350\257\257\345\267\256"), m_toleranceSpin);

    QGroupBox *processBox = new QGroupBox(QString::fromUtf8("\345\267\245\350\211\272\344\270\216\345\256\211\350\243\205"));
    QFormLayout *processLayout = new QFormLayout(processBox);
    m_detectionCombo = new QComboBox;
    m_detectionCombo->addItems({detectionTypeLabel(DetectionType::Measurement),
                                detectionTypeLabel(DetectionType::Positioning),
                                detectionTypeLabel(DetectionType::DefectInspection),
                                detectionTypeLabel(DetectionType::OcrCode)});
    m_surfaceCombo = new QComboBox;
    m_surfaceCombo->addItems({surfaceTypeLabel(SurfaceType::Matte),
                              surfaceTypeLabel(SurfaceType::ReflectiveMetal),
                              surfaceTypeLabel(SurfaceType::GlassTransparent),
                              surfaceTypeLabel(SurfaceType::PCB),
                              surfaceTypeLabel(SurfaceType::Plastic),
                              surfaceTypeLabel(SurfaceType::Mixed)});
    m_surfaceCombo->setCurrentIndex(1);
    m_wdSpin = makeSpin(5.0, 3000.0, 110.0, QStringLiteral(" mm"));
    m_heightVariationSpin = makeSpin(0.0, 200.0, 2.0, QStringLiteral(" mm"));
    m_speedSpin = makeSpin(0.0, 10000.0, 0.0, QStringLiteral(" mm/s"));
    m_fpsSpin = makeSpin(1.0, 1000.0, 20.0, QStringLiteral(" fps"));
    m_reflectiveCheck = new QCheckBox(QString::fromUtf8("\345\217\215\345\205\211/\351\253\230\345\205\211\350\241\250\351\235\242"));
    m_reflectiveCheck->setChecked(true);
    m_monoCheck = new QCheckBox(QString::fromUtf8("\344\274\230\345\205\210\351\273\221\347\231\275\347\233\270\346\234\272"));
    m_monoCheck->setChecked(true);
    m_allowTelecentricCheck = new QCheckBox(QString::fromUtf8("\345\205\201\350\256\270\350\277\234\345\277\203\351\225\234\345\244\264"));
    m_allowTelecentricCheck->setChecked(true);
    processLayout->addRow(QString::fromUtf8("\346\243\200\346\265\213\347\261\273\345\236\213"), m_detectionCombo);
    processLayout->addRow(QString::fromUtf8("\350\241\250\351\235\242\346\235\220\350\264\250"), m_surfaceCombo);
    processLayout->addRow(QString::fromUtf8("\345\267\245\344\275\234\350\267\235\347\246\273"), m_wdSpin);
    processLayout->addRow(QString::fromUtf8("\351\253\230\345\272\246\346\263\242\345\212\250"), m_heightVariationSpin);
    processLayout->addRow(QString::fromUtf8("\350\277\220\345\212\250\351\200\237\345\272\246"), m_speedSpin);
    processLayout->addRow(QString::fromUtf8("\350\212\202\346\213\215/\345\270\247\347\216\207"), m_fpsSpin);
    processLayout->addRow(QString(), m_reflectiveCheck);
    processLayout->addRow(QString(), m_monoCheck);
    processLayout->addRow(QString(), m_allowTelecentricCheck);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    QPushButton *calculateButton = new QPushButton(QString::fromUtf8("\350\256\241\347\256\227\346\216\250\350\215\220"));
    QPushButton *resultButton = new QPushButton(QString::fromUtf8("\346\237\245\347\234\213\347\273\223\346\236\234"));
    resultButton->setObjectName(QStringLiteral("SecondaryButton"));
    connect(calculateButton, &QPushButton::clicked, this, &MainWindow::calculate);
    connect(resultButton, &QPushButton::clicked, this, [this]() { setActivePage(1); });
    buttonLayout->addWidget(calculateButton);
    buttonLayout->addWidget(resultButton);
    buttonLayout->addStretch();

    layout->addWidget(objectBox);
    layout->addWidget(processBox);
    layout->addLayout(buttonLayout);
    layout->addStretch();
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);

    return page;
}

QWidget *MainWindow::createResultsPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);

    layout->addWidget(pageTitle(QString::fromUtf8("\346\216\250\350\215\220\347\273\223\346\236\234")));
    m_resultSummaryLabel = new QLabel;
    m_resultSummaryLabel->setObjectName(QStringLiteral("SectionTitle"));
    layout->addWidget(m_resultSummaryLabel);

    m_resultTable = new QTableWidget;
    setupTable(m_resultTable);
    m_resultTable->setColumnCount(10);
    m_resultTable->setHorizontalHeaderLabels({
        QString::fromUtf8("\347\261\273\345\236\213"), QString::fromUtf8("\345\276\227\345\210\206"), QString::fromUtf8("\347\233\270\346\234\272"),
        QString::fromUtf8("\351\225\234\345\244\264"), QString::fromUtf8("\345\205\211\346\272\220"), QStringLiteral("FOV(mm)"),
        QString::fromUtf8("\347\211\251\346\226\271\345\203\217\347\264\240"), QString::fromUtf8("\345\200\215\347\216\207/\347\204\246\350\267\235"), QStringLiteral("WD/DOF"), QString::fromUtf8("\351\243\216\351\231\251")
    });
    m_resultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(9, QHeaderView::Stretch);
    connect(m_resultTable, &QTableWidget::cellClicked, this, [this](int row, int) {
        refreshResultDetails(row);
    });

    m_resultDetails = new QTextEdit;
    m_resultDetails->setReadOnly(true);
    m_resultDetails->setMinimumHeight(150);

    layout->addWidget(m_resultTable, 1);
    layout->addWidget(m_resultDetails);
    return page;
}

QWidget *MainWindow::createCatalogPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);
    layout->addWidget(pageTitle(QString::fromUtf8("\345\217\202\346\225\260\345\272\223")));

    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *importCamera = new QPushButton(QString::fromUtf8("\345\257\274\345\205\245 cameras.csv"));
    QPushButton *importLens = new QPushButton(QString::fromUtf8("\345\257\274\345\205\245 lenses.csv"));
    QPushButton *importLight = new QPushButton(QString::fromUtf8("\345\257\274\345\205\245 lights.csv"));
    connect(importCamera, &QPushButton::clicked, this, &MainWindow::importCameras);
    connect(importLens, &QPushButton::clicked, this, &MainWindow::importLenses);
    connect(importLight, &QPushButton::clicked, this, &MainWindow::importLights);
    buttons->addWidget(importCamera);
    buttons->addWidget(importLens);
    buttons->addWidget(importLight);
    buttons->addStretch();
    layout->addLayout(buttons);

    QTabWidget *tabs = new QTabWidget;
    m_cameraTable = new QTableWidget;
    m_lensTable = new QTableWidget;
    m_lightTable = new QTableWidget;
    setupTable(m_cameraTable);
    setupTable(m_lensTable);
    setupTable(m_lightTable);
    tabs->addTab(m_cameraTable, QString::fromUtf8("\347\233\270\346\234\272"));
    tabs->addTab(m_lensTable, QString::fromUtf8("\351\225\234\345\244\264"));
    tabs->addTab(m_lightTable, QString::fromUtf8("\345\205\211\346\272\220"));
    layout->addWidget(tabs, 1);

    return page;
}

QWidget *MainWindow::createReportPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);
    layout->addWidget(pageTitle(QString::fromUtf8("PDF \346\212\245\345\221\212")));

    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *exportButton = new QPushButton(QString::fromUtf8("\345\257\274\345\207\272 PDF"));
    QPushButton *recalculateButton = new QPushButton(QString::fromUtf8("\351\207\215\346\226\260\350\256\241\347\256\227"));
    recalculateButton->setObjectName(QStringLiteral("SecondaryButton"));
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportPdf);
    connect(recalculateButton, &QPushButton::clicked, this, &MainWindow::calculate);
    buttons->addWidget(exportButton);
    buttons->addWidget(recalculateButton);
    buttons->addStretch();
    layout->addLayout(buttons);

    m_reportPreview = new QTextEdit;
    m_reportPreview->setReadOnly(true);
    layout->addWidget(m_reportPreview, 1);
    return page;
}

QDoubleSpinBox *MainWindow::makeSpin(double min, double max, double value, const QString &suffix, int decimals)
{
    QDoubleSpinBox *spin = new QDoubleSpinBox;
    spin->setRange(min, max);
    spin->setValue(value);
    spin->setDecimals(decimals);
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);
    return spin;
}

void MainWindow::setActivePage(int index)
{
    if (!m_pages || index < 0 || index >= m_pages->count())
        return;
    m_pages->setCurrentIndex(index);
    for (int i = 0; i < m_navButtons.size(); ++i) {
        m_navButtons.at(i)->setProperty("active", i == index);
        m_navButtons.at(i)->style()->unpolish(m_navButtons.at(i));
        m_navButtons.at(i)->style()->polish(m_navButtons.at(i));
    }
}

void MainWindow::calculate()
{
    m_request = readRequest();
    SelectionEngine engine;
    m_results = engine.select(m_request, m_catalog.cameras(), m_catalog.lenses(), m_catalog.lights(), 20);
    refreshResultTable();
    refreshCatalogTables();
    refreshReportPreview();
    if (m_summaryLabel)
        m_summaryLabel->setText(m_catalog.summary());
}

SelectionRequest MainWindow::readRequest() const
{
    SelectionRequest request;
    request.objectWidthMm = m_widthSpin->value();
    request.objectHeightMm = m_heightSpin->value();
    request.placementMarginMm = m_marginSpin->value();
    request.minFeatureUm = m_minFeatureSpin->value();
    request.measurementToleranceUm = m_toleranceSpin->value();
    request.workingDistanceMm = m_wdSpin->value();
    request.heightVariationMm = m_heightVariationSpin->value();
    request.motionSpeedMmS = m_speedSpin->value();
    request.requiredFps = m_fpsSpin->value();
    request.detectionType = detectionTypeFromIndex(m_detectionCombo->currentIndex());
    request.surfaceType = surfaceTypeFromIndex(m_surfaceCombo->currentIndex());
    request.reflective = m_reflectiveCheck->isChecked();
    request.preferMono = m_monoCheck->isChecked();
    request.allowTelecentric = m_allowTelecentricCheck->isChecked();
    return request;
}

void MainWindow::refreshResultTable()
{
    if (!m_resultTable)
        return;

    m_resultSummaryLabel->setText(QString::fromUtf8("\351\234\200\346\261\202 FOV\357\274\232%1 x %2 mm\357\274\214\347\233\256\346\240\207\347\211\251\346\226\271\345\203\217\347\264\240\357\274\232%3 um/px\357\274\214\345\200\231\351\200\211\346\226\271\346\241\210\357\274\232%4 \344\270\252")
        .arg(SelectionEngine::requiredFovWidth(m_request), 0, 'f', 2)
        .arg(SelectionEngine::requiredFovHeight(m_request), 0, 'f', 2)
        .arg(SelectionEngine::targetObjectPixelUm(m_request), 0, 'f', 2)
        .arg(m_results.size()));

    m_resultTable->setRowCount(m_results.size());
    for (int row = 0; row < m_results.size(); ++row) {
        const SelectionResult &r = m_results.at(row);
        m_resultTable->setItem(row, 0, item(r.isTelecentric() ? QString::fromUtf8("\350\277\234\345\277\203") : QString::fromUtf8("\346\231\256\351\200\232")));
        m_resultTable->setItem(row, 1, item(number(r.score.score, 1)));
        m_resultTable->setItem(row, 2, item(r.camera.model));
        m_resultTable->setItem(row, 3, item(r.lens.model));
        m_resultTable->setItem(row, 4, item(r.light.model));
        m_resultTable->setItem(row, 5, item(QStringLiteral("%1 x %2")
            .arg(r.effectiveFovWidthMm, 0, 'f', 1)
            .arg(r.effectiveFovHeightMm, 0, 'f', 1)));
        m_resultTable->setItem(row, 6, item(QStringLiteral("%1 um").arg(r.objectPixelSizeUm, 0, 'f', 2)));
        m_resultTable->setItem(row, 7, item(r.isTelecentric()
            ? QStringLiteral("%1x").arg(r.magnification, 0, 'f', 3)
            : QStringLiteral("%1 mm").arg(r.lens.focalLengthMm, 0, 'f', 1)));
        m_resultTable->setItem(row, 8, item(r.isTelecentric()
            ? QStringLiteral("WD %1 / DOF %2").arg(r.lens.nominalWorkingDistanceMm, 0, 'f', 0).arg(r.lens.dofMm, 0, 'f', 1)
            : QStringLiteral("min WD %1").arg(r.lens.minWorkingDistanceMm, 0, 'f', 0)));
        m_resultTable->setItem(row, 9, item(r.score.risks.isEmpty()
            ? QString::fromUtf8("\346\227\240\344\270\273\350\246\201\351\243\216\351\231\251")
            : r.score.risks.join(QString::fromUtf8("\357\274\233"))));
    }
    if (!m_results.isEmpty()) {
        m_resultTable->selectRow(0);
        refreshResultDetails(0);
    }
}

void MainWindow::refreshResultDetails(int row)
{
    if (!m_resultDetails || row < 0 || row >= m_results.size())
        return;

    const SelectionResult &r = m_results.at(row);
    QString text;
    text += QStringLiteral("<h3>") + r.schemeTitle + QString::fromUtf8("\357\274\232")
        + r.camera.model + QStringLiteral(" + ")
        + r.lens.model + QStringLiteral(" + ")
        + r.light.model + QStringLiteral("</h3>");
    text += QString::fromUtf8("<p><b>\345\205\254\345\274\217\357\274\232</b>%1</p>").arg(r.formulaSummary);
    text += QString::fromUtf8("<p><b>\346\234\211\346\225\210 FOV\357\274\232</b>%1 x %2 mm\357\274\233<b>\347\211\251\346\226\271\345\203\217\347\264\240\357\274\232</b>%3 um/px\357\274\233<b>\346\216\245\345\217\243\345\270\246\345\256\275\357\274\232</b>%4 MB/s\343\200\202</p>")
        .arg(r.effectiveFovWidthMm, 0, 'f', 2)
        .arg(r.effectiveFovHeightMm, 0, 'f', 2)
        .arg(r.objectPixelSizeUm, 0, 'f', 2)
        .arg(r.bandwidthRequiredMBps, 0, 'f', 1);
    if (r.isTelecentric()) {
        text += QString::fromUtf8("<p><b>\350\277\234\345\277\203\345\217\202\346\225\260\357\274\232</b>PMAG %1x\357\274\214\346\240\207\347\247\260 WD %2 mm\357\274\214\350\277\234\345\277\203\345\272\246 %3 deg\357\274\214\347\225\270\345\217\230 %4%\357\274\214DOF %5 mm\357\274\214\346\256\213\344\275\231\350\247\206\345\267\256\344\274\260\347\256\227 %6 um\343\200\202</p>")
            .arg(r.magnification, 0, 'f', 3)
            .arg(r.lens.nominalWorkingDistanceMm, 0, 'f', 1)
            .arg(r.lens.telecentricityDeg, 0, 'f', 3)
            .arg(r.lens.distortionPercent, 0, 'f', 3)
            .arg(r.lens.dofMm, 0, 'f', 2)
            .arg(r.residualTelecentricErrorUm, 0, 'f', 2);
    }
    text += QString::fromUtf8("<p><b>\346\216\250\350\215\220\347\220\206\347\224\261\357\274\232</b>%1</p>").arg(r.score.reasons.join(QString::fromUtf8("\357\274\233")));
    text += QString::fromUtf8("<p><b>\351\243\216\351\231\251\346\217\220\347\244\272\357\274\232</b>%1</p>").arg(r.score.risks.isEmpty()
        ? QString::fromUtf8("\346\227\240\344\270\273\350\246\201\351\243\216\351\231\251\357\274\214\344\273\215\345\273\272\350\256\256\347\273\223\345\220\210\345\216\202\345\256\266 MTF/DOF \344\270\216\347\216\260\345\234\272\345\205\211\346\272\220\345\256\236\346\265\213\347\241\256\350\256\244\343\200\202")
        : r.score.risks.join(QString::fromUtf8("\357\274\233")));
    m_resultDetails->setHtml(text);
}

void MainWindow::refreshCatalogTables()
{
    if (!m_cameraTable || !m_lensTable || !m_lightTable)
        return;

    m_cameraTable->setColumnCount(9);
    m_cameraTable->setHorizontalHeaderLabels({QString::fromUtf8("\345\236\213\345\217\267"), QString::fromUtf8("\345\210\206\350\276\250\347\216\207"), QString::fromUtf8("\345\203\217\345\205\203"),
        QString::fromUtf8("\344\274\240\346\204\237\345\231\250"), QString::fromUtf8("\351\235\266\351\235\242"), QString::fromUtf8("\345\277\253\351\227\250"), QStringLiteral("fps"),
        QString::fromUtf8("\346\216\245\345\217\243"), QString::fromUtf8("\351\225\234\345\244\264\345\217\243")});
    m_cameraTable->setRowCount(m_catalog.cameras().size());
    for (int i = 0; i < m_catalog.cameras().size(); ++i) {
        const CameraSpec &c = m_catalog.cameras().at(i);
        m_cameraTable->setItem(i, 0, item(c.model));
        m_cameraTable->setItem(i, 1, item(QStringLiteral("%1 x %2").arg(c.resolutionX).arg(c.resolutionY)));
        m_cameraTable->setItem(i, 2, item(QStringLiteral("%1 um").arg(c.pixelSizeUm, 0, 'f', 2)));
        m_cameraTable->setItem(i, 3, item(QStringLiteral("%1 x %2 mm").arg(c.sensorWidthMm(), 0, 'f', 2).arg(c.sensorHeightMm(), 0, 'f', 2)));
        m_cameraTable->setItem(i, 4, item(c.sensorFormat));
        m_cameraTable->setItem(i, 5, item(c.shutterType));
        m_cameraTable->setItem(i, 6, item(number(c.maxFps, 1)));
        m_cameraTable->setItem(i, 7, item(c.interfaceType));
        m_cameraTable->setItem(i, 8, item(c.lensMount));
    }

    m_lensTable->setColumnCount(11);
    m_lensTable->setHorizontalHeaderLabels({QString::fromUtf8("\345\236\213\345\217\267"), QString::fromUtf8("\347\261\273\345\236\213"), QString::fromUtf8("\346\216\245\345\217\243"),
        QString::fromUtf8("\347\204\246\350\267\235"), QStringLiteral("PMAG"), QStringLiteral("WD"), QString::fromUtf8("\345\203\217\345\234\206"),
        QString::fromUtf8("\350\277\234\345\277\203\345\272\246"), QString::fromUtf8("\347\225\270\345\217\230"), QStringLiteral("DOF"), QString::fromUtf8("\345\220\214\350\275\264")});
    m_lensTable->setRowCount(m_catalog.lenses().size());
    for (int i = 0; i < m_catalog.lenses().size(); ++i) {
        const LensSpec &l = m_catalog.lenses().at(i);
        m_lensTable->setItem(i, 0, item(l.model));
        m_lensTable->setItem(i, 1, item(l.typeLabel()));
        m_lensTable->setItem(i, 2, item(l.lensMount));
        m_lensTable->setItem(i, 3, item(l.isTelecentric() ? QStringLiteral("-") : QStringLiteral("%1 mm").arg(l.focalLengthMm, 0, 'f', 1)));
        m_lensTable->setItem(i, 4, item(l.isTelecentric() ? QStringLiteral("%1x").arg(l.pmag, 0, 'f', 3) : QStringLiteral("-")));
        m_lensTable->setItem(i, 5, item(l.isTelecentric() ? QStringLiteral("%1 mm").arg(l.nominalWorkingDistanceMm, 0, 'f', 1) : QStringLiteral(">=%1 mm").arg(l.minWorkingDistanceMm, 0, 'f', 1)));
        m_lensTable->setItem(i, 6, item(QStringLiteral("%1 mm").arg(l.imageCircleMm, 0, 'f', 1)));
        m_lensTable->setItem(i, 7, item(l.isTelecentric() ? QStringLiteral("%1 deg").arg(l.telecentricityDeg, 0, 'f', 3) : QStringLiteral("-")));
        m_lensTable->setItem(i, 8, item(QStringLiteral("%1%").arg(l.distortionPercent, 0, 'f', 3)));
        m_lensTable->setItem(i, 9, item(l.isTelecentric() ? QStringLiteral("%1 mm").arg(l.dofMm, 0, 'f', 1) : QStringLiteral("-")));
        m_lensTable->setItem(i, 10, item(boolLabel(l.coaxialIllumination)));
    }

    m_lightTable->setColumnCount(7);
    m_lightTable->setHorizontalHeaderLabels({QString::fromUtf8("\345\236\213\345\217\267"), QString::fromUtf8("\347\261\273\345\236\213"), QString::fromUtf8("\351\242\234\350\211\262"),
        QString::fromUtf8("\346\263\242\351\225\277"), QString::fromUtf8("\346\250\241\345\274\217"), QString::fromUtf8("\346\234\211\346\225\210\351\235\242\347\247\257"), QString::fromUtf8("\351\200\202\347\224\250\345\234\272\346\231\257")});
    m_lightTable->setRowCount(m_catalog.lights().size());
    for (int i = 0; i < m_catalog.lights().size(); ++i) {
        const LightSpec &l = m_catalog.lights().at(i);
        m_lightTable->setItem(i, 0, item(l.model));
        m_lightTable->setItem(i, 1, item(l.typeLabel()));
        m_lightTable->setItem(i, 2, item(l.color));
        m_lightTable->setItem(i, 3, item(l.wavelengthNm > 0 ? QStringLiteral("%1 nm").arg(l.wavelengthNm) : QString::fromUtf8("\345\256\275\350\260\261")));
        m_lightTable->setItem(i, 4, item(l.mode));
        m_lightTable->setItem(i, 5, item(QStringLiteral("%1 x %2 mm").arg(l.activeWidthMm, 0, 'f', 0).arg(l.activeHeightMm, 0, 'f', 0)));
        m_lightTable->setItem(i, 6, item(l.bestFor));
    }
}

void MainWindow::refreshReportPreview()
{
    if (!m_reportPreview)
        return;
    QString text;
    text += QString::fromUtf8("PDF \345\260\206\345\214\205\345\220\253\357\274\232\n");
    text += QString::fromUtf8("- \351\234\200\346\261\202\350\276\223\345\205\245\343\200\201\347\233\256\346\240\207 FOV\343\200\201\347\233\256\346\240\207\347\211\251\346\226\271\345\203\217\347\264\240\n");
    text += QString::fromUtf8("- \346\231\256\351\200\232\351\225\234\345\244\264\344\270\216\350\277\234\345\277\203\351\225\234\345\244\264\347\232\204\345\205\263\351\224\256\345\205\254\345\274\217\n");
    text += QString::fromUtf8("- Top \346\216\250\350\215\220\346\226\271\346\241\210\343\200\201\351\243\216\351\231\251\346\217\220\347\244\272\343\200\201\350\265\204\346\226\231\344\276\235\346\215\256\n\n");
    if (!m_results.isEmpty()) {
        const SelectionResult &top = m_results.first();
        text += QString::fromUtf8("\345\275\223\345\211\215\351\246\226\351\200\211\357\274\232") + top.schemeTitle
            + QString::fromUtf8("\n\347\233\270\346\234\272\357\274\232") + top.camera.model
            + QString::fromUtf8("\n\351\225\234\345\244\264\357\274\232") + top.lens.model
            + QString::fromUtf8("\n\345\205\211\346\272\220\357\274\232") + top.light.model
            + QString::fromUtf8("\n\345\276\227\345\210\206\357\274\232") + QString::number(top.score.score, 'f', 1)
            + QStringLiteral("\n");
    }
    m_reportPreview->setPlainText(text);
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

void MainWindow::exportPdf()
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
