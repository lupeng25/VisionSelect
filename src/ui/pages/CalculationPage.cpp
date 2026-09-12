#include "ui/pages/CalculationPage.h"
#include "core/PixelFormat.h"

#include "ui/UiHelpers.h"
#include "ui/UiSettings.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QTableWidget>
#include <QTextEdit>
#include <QSplitter>
#include <QVBoxLayout>

using namespace UiHelpers;

namespace {
void selectRowBySourceIndex(QTableWidget *table, int sourceIndex)
{
    if (!table || sourceIndex < 0)
        return;
    for (int row = 0; row < table->rowCount(); ++row) {
        if (rowSourceIndex(table, row) == sourceIndex) {
            table->selectRow(row);
            return;
        }
    }
}
}

CalculationPage::CalculationPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(8);

    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *refreshButton = actionButton(localizedText("按当前需求计算", "Calculate From Current Requirements"), QStringLiteral(":/icons/ui/calculate.png"));
    QPushButton *inputButton = actionButton(localizedText("返回需求", "Back to Requirements"), QStringLiteral(":/icons/ui/requirement.png"), true);
    connect(refreshButton, &QPushButton::clicked, this, &CalculationPage::recalculateRequested);
    connect(inputButton, &QPushButton::clicked, this, &CalculationPage::inputRequested);
    buttons->addWidget(refreshButton);
    buttons->addWidget(inputButton);
    buttons->addStretch();
    QWidget *actions = new QWidget;
    actions->setLayout(buttons);
    layout->addWidget(pageHeader(localizedText("产品计算助手", "Product Calculation Assistant"),
        localizedText("从当前需求估算相机下限、镜头候选和关键工程余量。", "Estimate camera floor, lens candidates, and engineering margins from current requirements."),
        actions));

    QFrame *summaryCard = new QFrame;
    summaryCard->setObjectName(QStringLiteral("SectionCard"));
    QVBoxLayout *summaryLayout = new QVBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(10, 6, 10, 6);
    m_summaryLabel = new QLabel;
    m_summaryLabel->setObjectName(QStringLiteral("SectionTitle"));
    m_summaryLabel->setWordWrap(true);
    summaryLayout->addWidget(m_summaryLabel);
    layout->addWidget(summaryCard);

    QHBoxLayout *cameraActions = new QHBoxLayout;
    m_cameraSummary = new QLabel;
    m_cameraSummary->setObjectName("AssistantCameraSummary");
    m_cameraSummary->setTextFormat(Qt::PlainText);
    m_cameraSummary->setWordWrap(true);
    m_cameraSummary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cameraActions->addWidget(m_cameraSummary, 1);
    auto *chooseCamera = actionButton(localizedText("选择相机", "Choose camera"), {}, true);
    chooseCamera->setObjectName("AssistantChooseCamera");
    chooseCamera->setCheckable(true);
    cameraActions->addWidget(chooseCamera);
    layout->addLayout(cameraActions);

    QLabel *cameraTitle = new QLabel(localizedText("相机估算", "Camera Estimates"));
    cameraTitle->setObjectName(QStringLiteral("SectionTitle"));

    m_cameraTable = new QTableWidget;
    m_cameraTable->setObjectName(QStringLiteral("calculation/cameras"));
    m_cameraTable->setAccessibleName(localizedText("相机候选估算表", "Camera candidate estimates"));
    setupTable(m_cameraTable);
    m_cameraTable->setColumnCount(10);
    m_cameraTable->setHorizontalHeaderLabels({
        localizedText("相机", "Camera"), localizedText("厂家", "Manufacturer"),
        localizedText("分辨率", "Resolution"), localizedText("像元", "Pixel"),
        localizedText("传感器", "Sensor"), localizedText("理论采样", "Ideal sampling"),
        localizedText("普通焦距", "Fixed Focal"), QStringLiteral("PMAG"),
        localizedText("带宽", "Bandwidth"), localizedText("判断", "Verdict")
    });
    m_cameraTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    const int cameraWidths[] = {150, 110, 118, 86, 92, 104, 100, 96, 138};
    for (int column = 0; column < 9; ++column)
        m_cameraTable->setColumnWidth(column, cameraWidths[column]);
    m_cameraTable->horizontalHeader()->setSectionResizeMode(9, QHeaderView::Stretch);
    m_cameraTable->horizontalHeader()->setMinimumSectionSize(70);
    connect(m_cameraTable, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) {
        const int sourceRow = rowSourceIndex(m_cameraTable, row);
        if (sourceRow < 0 || sourceRow >= m_cameras.size()) return;
        m_selectedCameraEstimateRow = sourceRow;
        refreshCameraSummary();
        emit cameraSelectionChanged(m_selectedCameraEstimateRow);
    });

    m_lensSummary = new QLabel;
    m_lensSummary->setObjectName(QStringLiteral("SectionTitle"));
    m_lensSummary->setWordWrap(true);

    m_lensTable = new QTableWidget;
    m_lensTable->setObjectName(QStringLiteral("calculation/lenses"));
    m_lensTable->setProperty("headerStateKey", "calculation/lenses-v2");
    m_lensTable->setAccessibleName(localizedText("镜头候选估算表", "Lens candidate estimates"));
    setupTable(m_lensTable);
    m_lensTable->setColumnCount(10);
    m_lensTable->setHorizontalHeaderLabels({
        localizedText("状态", "Status"), localizedText("厂家", "Manufacturer"),
        localizedText("镜头", "Lens"), localizedText("接口", "Mount"),
        localizedText("焦距/PMAG", "Focal / PMAG"), QStringLiteral("FOV"),
        localizedText("物方像素", "Object Pixel"), QStringLiteral("WD/DOF"),
        localizedText("像圈", "Image Circle"), localizedText("判断", "Verdict")
    });
    m_lensTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    const int lensWidths[] = {100, 80, 180, 66, 96, 108, 96, 122, 80};
    for (int column = 0; column < 9; ++column)
        m_lensTable->setColumnWidth(column, lensWidths[column]);
    m_lensTable->horizontalHeader()->setSectionResizeMode(9, QHeaderView::Stretch);
    m_lensTable->horizontalHeader()->setMinimumSectionSize(70);
    connect(m_lensTable, &QTableWidget::currentCellChanged, this, [this]() { refreshLensDetails(); });

    m_details = new QTextEdit;
    m_details->setObjectName("AssistantLensDetails");
    m_details->setAccessibleName(localizedText("候选工程详情", "Candidate engineering details"));
    m_details->setReadOnly(true);
    m_details->setMinimumHeight(120);
    m_details->setMaximumHeight(240);

    QWidget *cameraPane = new QWidget;
    QVBoxLayout *cameraPaneLayout = new QVBoxLayout(cameraPane);
    cameraPaneLayout->setContentsMargins(0, 0, 0, 0);
    cameraPaneLayout->setSpacing(6);
    cameraPaneLayout->addWidget(cameraTitle);
    cameraPaneLayout->addWidget(m_cameraTable);
    cameraPane->setMaximumHeight(190);
    cameraPane->hide();
    layout->addWidget(cameraPane);
    connect(chooseCamera, &QPushButton::toggled, cameraPane, &QWidget::setVisible);
    connect(m_cameraTable, &QTableWidget::cellDoubleClicked, chooseCamera, [chooseCamera]() { chooseCamera->setChecked(false); });
    QWidget *lensPane = new QWidget;
    QVBoxLayout *lensPaneLayout = new QVBoxLayout(lensPane);
    lensPaneLayout->setContentsMargins(0, 0, 0, 0);
    lensPaneLayout->setSpacing(6);
    QHBoxLayout *lensActions = new QHBoxLayout;
    lensActions->addWidget(m_lensSummary, 1);
    m_detailsButton = actionButton(localizedText("校核详情", "Check details"), {}, true);
    m_detailsButton->setObjectName("AssistantDetailsToggle");
    m_detailsButton->setCheckable(true);
    lensActions->addWidget(m_detailsButton);
    lensPaneLayout->addLayout(lensActions);
    lensPaneLayout->addWidget(m_lensTable);

    m_splitter = new QSplitter(Qt::Vertical, this);
    m_splitter->setObjectName(QStringLiteral("calculation/results-v2"));
    m_splitter->addWidget(lensPane);
    m_splitter->addWidget(m_details);
    m_splitter->setStretchFactor(0, 4);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({480, 180});
    m_details->hide();
    connect(m_detailsButton, &QPushButton::toggled, this, [this](bool visible) {
        m_details->setVisible(visible);
        if (visible) { m_splitter->setSizes({480, 190}); refreshLensDetails(); }
    });
    connect(m_lensTable, &QTableWidget::cellDoubleClicked, this, [this]() { m_detailsButton->setChecked(true); });
    UiSettings::instance().restoreHeader(QStringLiteral("calculation/cameras"), m_cameraTable->horizontalHeader());
    UiSettings::instance().restoreHeader(QStringLiteral("calculation/lenses-v2"), m_lensTable->horizontalHeader());
    layout->addWidget(m_splitter, 1);
    m_selectionSummary = new QLabel;
    m_selectionSummary->setObjectName("AssistantSelectionSummary");
    m_selectionSummary->setTextFormat(Qt::PlainText);
    m_selectionSummary->setWordWrap(true);
    m_selectionSummary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_selectionSummary);
}

CalculationPage::~CalculationPage()
{
    UiSettings::instance().saveHeader(QStringLiteral("calculation/cameras"), m_cameraTable ? m_cameraTable->horizontalHeader() : nullptr);
    UiSettings::instance().saveHeader(QStringLiteral("calculation/lenses-v2"), m_lensTable ? m_lensTable->horizontalHeader() : nullptr);
}

void CalculationPage::setSummary(const QString &text)
{
    if (m_summaryLabel)
        m_summaryLabel->setText(text);
}

void CalculationPage::setCameraEstimates(const QVector<CameraEstimate> &estimates, const CameraSpec *initialCamera)
{
    if (!m_cameraTable)
        return;

    const int previousIndex = selectedCameraEstimateRow();
    const CameraSpec previous = previousIndex >= 0 && previousIndex < m_cameras.size()
        ? m_cameras.at(previousIndex).camera : (initialCamera ? *initialCamera : CameraSpec());
    m_cameras = estimates;
    m_selectedCameraEstimateRow = estimates.isEmpty() ? -1 : 0;
    for (int i = 0; i < estimates.size(); ++i)
        if (estimates[i].camera.model == previous.model && estimates[i].camera.manufacturer == previous.manufacturer)
            m_selectedCameraEstimateRow = i;
    const QSignalBlocker blocker(m_cameraTable);

    m_cameraTable->setSortingEnabled(false);
    m_cameraTable->setRowCount(estimates.size());
    for (int row = 0; row < estimates.size(); ++row) {
        const CameraEstimate &estimate = estimates.at(row);
        const CameraSpec &camera = estimate.camera;
        QStringList verdict;
        verdict.append(estimate.meetsSampling ? localizedText("像素满足", "Sampling OK") : localizedText("像素不足", "Sampling low"));
        verdict.append(estimate.meetsFps ? localizedText("帧率满足", "FPS OK") : localizedText("帧率不足", "FPS low"));
        if (camera.bandwidthMBps <= 0.0 || camera.bandwidthSource != QLatin1String("specified")
            || !PixelFormat::layout(camera.transportPixelFormat()))
            verdict.append(localizedText("传输格式/容量待确认", "Transfer format/capacity pending"));
        else if (!estimate.meetsBandwidth)
            verdict.append(localizedText("带宽不足", "Bandwidth low"));
        else if (estimate.bandwidthUtilizationPercent > 90.0)
            verdict.append(localizedText("带宽接近上限", "Bandwidth tight"));
        if (estimate.globalShutterRecommended)
            verdict.append(localizedText("建议全局快门", "Global shutter recommended"));

        m_cameraTable->setItem(row, 0, indexedItem(camera.model, row));
        m_cameraTable->setItem(row, 1, item(camera.manufacturer));
        m_cameraTable->setItem(row, 2, numericItem(QStringLiteral("%1 x %2").arg(camera.resolutionX).arg(camera.resolutionY), camera.megapixels()));
        m_cameraTable->setItem(row, 3, numericItem(QStringLiteral("%1 um").arg(camera.pixelSizeUm, 0, 'f', 2), camera.pixelSizeUm));
        m_cameraTable->setItem(row, 4, numericItem(QStringLiteral("%1 mm").arg(estimate.sensorDiagonalMm, 0, 'f', 2), estimate.sensorDiagonalMm));
        m_cameraTable->setItem(row, 5, numericItem(QStringLiteral("%1 um/px").arg(estimate.objectPixelSizeUm, 0, 'f', 2), estimate.objectPixelSizeUm));
        m_cameraTable->setItem(row, 6, numericItem(QStringLiteral("%1 mm").arg(estimate.fixedFocalLengthMm, 0, 'f', 1), estimate.fixedFocalLengthMm));
        m_cameraTable->setItem(row, 7, numericItem(estimate.telecentricFeasible
            ? QStringLiteral("%1 - %2x").arg(estimate.telecentricPmagMin, 0, 'f', 3).arg(estimate.telecentricPmagMax, 0, 'f', 3)
            : localizedText("不可行", "Not feasible"), estimate.telecentricFeasible ? std::optional(estimate.telecentricPmagMin) : std::nullopt));
        const QString bandwidthText = estimate.interfaceCapacityMBps > 0.0
            ? QStringLiteral("%1 / %2 MB/s (%3%)")
                .arg(estimate.bandwidthRequiredMBps, 0, 'f', 1)
                .arg(estimate.interfaceCapacityMBps, 0, 'f', 1)
                .arg(estimate.bandwidthUtilizationPercent, 0, 'f', 0)
            : QStringLiteral("%1 MB/s").arg(estimate.bandwidthRequiredMBps, 0, 'f', 1);
        m_cameraTable->setItem(row, 8, numericItem(bandwidthText, estimate.bandwidthRequiredMBps));
        m_cameraTable->item(row, 8)->setToolTip(bandwidthText + QLatin1Char('\n')
            + (camera.bandwidthSource == QLatin1String("specified") ? localizedText("容量来源：已确认", "Capacity source: confirmed")
                : localizedText("容量来源：估算或未知，不能作为验收依据", "Capacity source: estimated or unknown; verify before acceptance")));
        m_cameraTable->setItem(row, 9, item(verdict.join(localizedText("；", "; "))));
    }
    m_cameraTable->setSortingEnabled(true);

    if (estimates.isEmpty()) {
        m_selectedCameraEstimateRow = -1;
    } else if (m_selectedCameraEstimateRow < 0 || m_selectedCameraEstimateRow >= estimates.size()) {
        m_selectedCameraEstimateRow = 0;
    }
    if (m_selectedCameraEstimateRow >= 0)
        selectRowBySourceIndex(m_cameraTable, m_selectedCameraEstimateRow);
    refreshCameraSummary();
}

void CalculationPage::setLensEstimates(const QVector<LensEstimate> &estimates)
{
    if (!m_lensTable)
        return;

    const int previousIndex = rowSourceIndex(m_lensTable, m_lensTable->currentRow());
    const LensSpec previous = previousIndex >= 0 && previousIndex < m_lenses.size()
        ? m_lenses.at(previousIndex).lens : LensSpec();
    m_lenses = estimates;
    int selected = estimates.isEmpty() ? -1 : 0;
    int passed = 0, unknown = 0, failed = 0;
    for (int i = 0; i < estimates.size(); ++i) {
        if (estimates[i].lens.model == previous.model && estimates[i].lens.manufacturer == previous.manufacturer) selected = i;
        if (estimates[i].checks.failed()) ++failed;
        else if (estimates[i].checks.unknown()) ++unknown;
        else ++passed;
    }
    const QSignalBlocker blocker(m_lensTable);

    m_lensTable->setSortingEnabled(false);
    m_lensTable->setRowCount(estimates.size());
    for (int row = 0; row < estimates.size(); ++row) {
        const LensEstimate &estimate = estimates.at(row);
        const LensSpec &lens = estimate.lens;
        const QString focalOrPmag = lens.isTelecentric()
            ? QStringLiteral("%1x").arg(estimate.magnification, 0, 'f', 3)
            : QStringLiteral("%1 mm").arg(lens.focalLengthMm, 0, 'f', 1);
        const double wd = lens.isTelecentric() ? lens.nominalWorkingDistanceMm : lens.minWorkingDistanceMm;
        const QString wdOrDof = QStringLiteral("%1 %2 / DOF %3")
            .arg(lens.isTelecentric() ? QStringLiteral("WD") : QStringLiteral("min WD"),
                 wd > 0 ? number(wd, 0) : localizedText("未知", "Unknown"),
                 estimate.estimatedDofMm > 0 ? number(estimate.estimatedDofMm, 1) : localizedText("未知", "Unknown"));
        QStringList verdict = candidateCheckMessages(estimate.checks, CandidateCheckState::Failed)
            + candidateCheckMessages(estimate.checks, CandidateCheckState::Unknown);
        if (verdict.isEmpty()) verdict.append(localizedText("几何与安装初筛通过", "Geometry and mounting screening passed"));

        m_lensTable->setItem(row, 0, indexedItem(candidateStatusText(estimate.checks), row));
        decorateCandidateStatus(m_lensTable->item(row, 0), estimate.checks);
        m_lensTable->item(row, 0)->setToolTip(lens.typeLabel() + "\n" + verdict.join("\n"));
        m_lensTable->setItem(row, 1, item(lens.manufacturer));
        m_lensTable->setItem(row, 2, item(lens.model));
        m_lensTable->setItem(row, 3, item(lens.lensMount));
        m_lensTable->setItem(row, 4, numericItem(focalOrPmag, lens.isTelecentric() ? estimate.magnification : lens.focalLengthMm));
        m_lensTable->setItem(row, 5, numericItem(QStringLiteral("%1 x %2").arg(estimate.effectiveFovWidthMm, 0, 'f', 1).arg(estimate.effectiveFovHeightMm, 0, 'f', 1), estimate.effectiveFovWidthMm));
        m_lensTable->setItem(row, 6, numericItem(QStringLiteral("%1 um").arg(estimate.objectPixelSizeUm, 0, 'f', 2), estimate.objectPixelSizeUm));
        m_lensTable->setItem(row, 7, numericItem(wdOrDof, wd > 0 ? std::optional(wd) : std::nullopt));
        m_lensTable->setItem(row, 8, numericItem(lens.imageCircleMm > 0 ? QStringLiteral("%1 mm").arg(lens.imageCircleMm, 0, 'f', 1) : localizedText("未知", "Unknown"), lens.imageCircleMm > 0 ? std::optional(lens.imageCircleMm) : std::nullopt));
        m_lensTable->setItem(row, 9, item(verdict.join(localizedText("；", "; "))));
    }
    m_lensTable->setSortingEnabled(true);
    selectRowBySourceIndex(m_lensTable, selected);
    m_lensSummary->setText(localizedText("镜头候选 %1 · 通过 %2 / 待确认 %3 / 不满足 %4",
        "Lenses %1 · Passed %2 / Pending %3 / Failed %4").arg(estimates.size()).arg(passed).arg(unknown).arg(failed));
    m_detailsButton->setEnabled(!estimates.isEmpty());
    refreshLensDetails();
}

void CalculationPage::setDetails(const QString &text)
{
    m_requirementDetails = text;
    refreshLensDetails();
}

int CalculationPage::selectedCameraEstimateRow() const
{
    if (m_cameraTable && m_cameraTable->currentRow() >= 0) {
        const int sourceRow = rowSourceIndex(m_cameraTable, m_cameraTable->currentRow());
        return sourceRow >= 0 ? sourceRow : m_cameraTable->currentRow();
    }
    return m_selectedCameraEstimateRow;
}

void CalculationPage::refreshCameraSummary()
{
    const int index = selectedCameraEstimateRow();
    if (index < 0 || index >= m_cameras.size()) {
        m_cameraSummary->setText(localizedText("暂无相机候选", "No camera candidates"));
        return;
    }
    const auto &camera = m_cameras.at(index).camera;
    m_cameraSummary->setText(localizedText("当前相机：%1 · %2 × %3 px · 像元 %4 μm",
        "Camera: %1 · %2 × %3 px · Pixel %4 μm")
        .arg(productLabel(camera.manufacturer, camera.model)).arg(camera.resolutionX).arg(camera.resolutionY).arg(camera.pixelSizeUm, 0, 'f', 2));
}

void CalculationPage::refreshLensDetails()
{
    if (!m_selectionSummary) return;
    const int index = rowSourceIndex(m_lensTable, m_lensTable->currentRow());
    if (index < 0 || index >= m_lenses.size()) {
        m_selectionSummary->setText(localizedText("没有镜头候选，请调整需求或选择相机。", "No lens candidates. Change requirements or choose a camera."));
        m_details->clear();
        return;
    }
    const auto &estimate = m_lenses.at(index);
    const QString title = productLabel(estimate.lens.manufacturer, estimate.lens.model);
    const QStringList issues = candidateCheckMessages(estimate.checks, CandidateCheckState::Failed)
        + candidateCheckMessages(estimate.checks, CandidateCheckState::Unknown);
    m_selectionSummary->setText(title + QStringLiteral("  |  ") + candidateStatusText(estimate.checks)
        + (issues.isEmpty() ? QString() : QStringLiteral("\n") + issues.join(QStringLiteral("；"))));
    if (m_details->isHidden()) return;
    QString html = QStringLiteral("<b>%1</b><p>%2</p>").arg(title.toHtmlEscaped(), m_cameraSummary->text().toHtmlEscaped());
    const int cameraIndex = selectedCameraEstimateRow();
    if (cameraIndex >= 0 && cameraIndex < m_cameras.size()) {
        const CameraSpec &camera = m_cameras.at(cameraIndex).camera;
        html += localizedText("<p>像圈 %1 mm / 传感器对角线 %2 mm；接口 %3 / %4。</p>",
            "<p>Image circle %1 mm / sensor diagonal %2 mm; mounts %3 / %4.</p>")
            .arg(estimate.lens.imageCircleMm, 0, 'f', 2).arg(camera.sensorDiagonalMm(), 0, 'f', 2)
            .arg(camera.lensMount.toHtmlEscaped(), estimate.lens.lensMount.toHtmlEscaped());
    }
    html += candidateChecksHtml(estimate.checks);
    html += QStringLiteral("<p>FOV %1 × %2 mm · %3 μm/px · DOF %4 mm</p>")
        .arg(estimate.effectiveFovWidthMm, 0, 'f', 2).arg(estimate.effectiveFovHeightMm, 0, 'f', 2)
        .arg(estimate.objectPixelSizeUm, 0, 'f', 2).arg(estimate.estimatedDofMm, 0, 'f', 2);
    html += QStringLiteral("<p>%1</p><p>%2</p><pre>%3</pre>").arg(estimate.risks.join("\n").toHtmlEscaped().replace("\n", "<br>"),
        estimate.formulaSummary.toHtmlEscaped(), m_requirementDetails.toHtmlEscaped());
    m_details->setHtml(html);
}
