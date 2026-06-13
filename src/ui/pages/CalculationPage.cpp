#include "ui/pages/CalculationPage.h"

#include "ui/UiHelpers.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTextEdit>
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
    layout->setContentsMargins(26, 22, 26, 22);
    layout->setSpacing(14);

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
    summaryLayout->setContentsMargins(14, 12, 14, 12);
    m_summaryLabel = new QLabel;
    m_summaryLabel->setObjectName(QStringLiteral("SectionTitle"));
    m_summaryLabel->setWordWrap(true);
    summaryLayout->addWidget(m_summaryLabel);
    layout->addWidget(summaryCard);

    QLabel *cameraTitle = new QLabel(localizedText("相机估算", "Camera Estimates"));
    cameraTitle->setObjectName(QStringLiteral("SectionTitle"));
    layout->addWidget(cameraTitle);

    m_cameraTable = new QTableWidget;
    setupTable(m_cameraTable);
    m_cameraTable->setColumnCount(10);
    m_cameraTable->setHorizontalHeaderLabels({
        localizedText("相机", "Camera"), localizedText("厂家", "Manufacturer"),
        localizedText("分辨率", "Resolution"), localizedText("像元", "Pixel"),
        localizedText("传感器", "Sensor"), localizedText("物方像素", "Object Pixel"),
        localizedText("普通焦距", "Fixed Focal"), QStringLiteral("PMAG"),
        localizedText("带宽", "Bandwidth"), localizedText("判断", "Verdict")
    });
    m_cameraTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_cameraTable->horizontalHeader()->setSectionResizeMode(9, QHeaderView::Stretch);
    connect(m_cameraTable, &QTableWidget::cellClicked, this, [this](int row, int) {
        const int sourceRow = rowSourceIndex(m_cameraTable, row);
        m_selectedCameraEstimateRow = sourceRow >= 0 ? sourceRow : row;
        emit cameraSelectionChanged(m_selectedCameraEstimateRow);
    });
    layout->addWidget(m_cameraTable, 1);

    QLabel *lensTitle = new QLabel(localizedText("镜头候选", "Lens Candidates"));
    lensTitle->setObjectName(QStringLiteral("SectionTitle"));
    layout->addWidget(lensTitle);

    m_lensTable = new QTableWidget;
    setupTable(m_lensTable);
    m_lensTable->setColumnCount(10);
    m_lensTable->setHorizontalHeaderLabels({
        localizedText("类型", "Type"), localizedText("厂家", "Manufacturer"),
        localizedText("镜头", "Lens"), localizedText("接口", "Mount"),
        localizedText("焦距/PMAG", "Focal / PMAG"), QStringLiteral("FOV"),
        localizedText("物方像素", "Object Pixel"), QStringLiteral("WD/DOF"),
        localizedText("像圈", "Image Circle"), localizedText("判断", "Verdict")
    });
    m_lensTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_lensTable->horizontalHeader()->setSectionResizeMode(9, QHeaderView::Stretch);
    layout->addWidget(m_lensTable, 1);

    m_details = new QTextEdit;
    m_details->setReadOnly(true);
    m_details->setMinimumHeight(120);
    layout->addWidget(m_details);
}

void CalculationPage::setSummary(const QString &text)
{
    if (m_summaryLabel)
        m_summaryLabel->setText(text);
}

void CalculationPage::setCameraEstimates(const QVector<CameraEstimate> &estimates)
{
    if (!m_cameraTable)
        return;

    m_cameraTable->setSortingEnabled(false);
    m_cameraTable->setRowCount(estimates.size());
    for (int row = 0; row < estimates.size(); ++row) {
        const CameraEstimate &estimate = estimates.at(row);
        const CameraSpec &camera = estimate.camera;
        QStringList verdict;
        verdict.append(estimate.meetsSampling ? localizedText("像素满足", "Sampling OK") : localizedText("像素不足", "Sampling low"));
        verdict.append(estimate.meetsFps ? localizedText("帧率满足", "FPS OK") : localizedText("帧率不足", "FPS low"));
        if (estimate.interfaceCapacityMBps <= 0.0)
            verdict.append(localizedText("带宽未知", "Bandwidth unknown"));
        else if (!estimate.meetsBandwidth)
            verdict.append(localizedText("带宽不足", "Bandwidth low"));
        else if (estimate.bandwidthUtilizationPercent > 90.0)
            verdict.append(localizedText("带宽接近上限", "Bandwidth tight"));
        if (estimate.globalShutterRecommended)
            verdict.append(localizedText("建议全局快门", "Global shutter recommended"));

        m_cameraTable->setItem(row, 0, indexedItem(camera.model, row));
        m_cameraTable->setItem(row, 1, item(camera.manufacturer));
        m_cameraTable->setItem(row, 2, item(QStringLiteral("%1 x %2").arg(camera.resolutionX).arg(camera.resolutionY)));
        m_cameraTable->setItem(row, 3, item(QStringLiteral("%1 um").arg(camera.pixelSizeUm, 0, 'f', 2)));
        m_cameraTable->setItem(row, 4, item(QStringLiteral("%1 mm").arg(estimate.sensorDiagonalMm, 0, 'f', 2)));
        m_cameraTable->setItem(row, 5, item(QStringLiteral("%1 um/px").arg(estimate.objectPixelSizeUm, 0, 'f', 2)));
        m_cameraTable->setItem(row, 6, item(QStringLiteral("%1 mm").arg(estimate.fixedFocalLengthMm, 0, 'f', 1)));
        m_cameraTable->setItem(row, 7, item(estimate.telecentricFeasible
            ? QStringLiteral("%1 - %2x").arg(estimate.telecentricPmagMin, 0, 'f', 3).arg(estimate.telecentricPmagMax, 0, 'f', 3)
            : localizedText("不建议", "Not recommended")));
        const QString bandwidthText = estimate.interfaceCapacityMBps > 0.0
            ? QStringLiteral("%1 / %2 MB/s (%3%)")
                .arg(estimate.bandwidthRequiredMBps, 0, 'f', 1)
                .arg(estimate.interfaceCapacityMBps, 0, 'f', 1)
                .arg(estimate.bandwidthUtilizationPercent, 0, 'f', 0)
            : QStringLiteral("%1 MB/s").arg(estimate.bandwidthRequiredMBps, 0, 'f', 1);
        m_cameraTable->setItem(row, 8, item(bandwidthText));
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
}

void CalculationPage::setLensEstimates(const QVector<LensEstimate> &estimates)
{
    if (!m_lensTable)
        return;

    m_lensTable->setSortingEnabled(false);
    m_lensTable->setRowCount(estimates.size());
    for (int row = 0; row < estimates.size(); ++row) {
        const LensEstimate &estimate = estimates.at(row);
        const LensSpec &lens = estimate.lens;
        const QString focalOrPmag = lens.isTelecentric()
            ? QStringLiteral("%1x").arg(estimate.magnification, 0, 'f', 3)
            : QStringLiteral("%1 mm").arg(lens.focalLengthMm, 0, 'f', 1);
        const QString wdOrDof = lens.isTelecentric()
            ? QStringLiteral("WD %1 / DOF %2").arg(lens.nominalWorkingDistanceMm, 0, 'f', 0).arg(lens.dofMm, 0, 'f', 1)
            : QStringLiteral("min WD %1 / DOF %2").arg(lens.minWorkingDistanceMm, 0, 'f', 0).arg(estimate.estimatedDofMm, 0, 'f', 1);
        QStringList verdict;
        verdict.append(estimate.fovOk ? localizedText("FOV 满足", "FOV OK") : localizedText("FOV 不足", "FOV low"));
        verdict.append(estimate.samplingOk ? localizedText("像素满足", "Sampling OK") : localizedText("像素不足", "Sampling low"));
        if (!estimate.mountOk)
            verdict.append(localizedText("接口不匹配", "Mount mismatch"));
        if (!estimate.workingDistanceOk)
            verdict.append(QStringLiteral("WD"));
        if (!estimate.dofOk)
            verdict.append(QStringLiteral("DOF"));

        m_lensTable->setItem(row, 0, indexedItem(lens.typeLabel(), row));
        m_lensTable->setItem(row, 1, item(lens.manufacturer));
        m_lensTable->setItem(row, 2, item(lens.model));
        m_lensTable->setItem(row, 3, item(lens.lensMount));
        m_lensTable->setItem(row, 4, item(focalOrPmag));
        m_lensTable->setItem(row, 5, item(QStringLiteral("%1 x %2").arg(estimate.effectiveFovWidthMm, 0, 'f', 1).arg(estimate.effectiveFovHeightMm, 0, 'f', 1)));
        m_lensTable->setItem(row, 6, item(QStringLiteral("%1 um").arg(estimate.objectPixelSizeUm, 0, 'f', 2)));
        m_lensTable->setItem(row, 7, item(wdOrDof));
        m_lensTable->setItem(row, 8, item(QStringLiteral("%1 mm").arg(lens.imageCircleMm, 0, 'f', 1)));
        m_lensTable->setItem(row, 9, item(verdict.join(localizedText("；", "; "))));
    }
    m_lensTable->setSortingEnabled(true);
}

void CalculationPage::setDetails(const QString &text)
{
    if (m_details)
        m_details->setPlainText(text);
}

int CalculationPage::selectedCameraEstimateRow() const
{
    if (m_cameraTable && m_cameraTable->currentRow() >= 0) {
        const int sourceRow = rowSourceIndex(m_cameraTable, m_cameraTable->currentRow());
        return sourceRow >= 0 ? sourceRow : m_cameraTable->currentRow();
    }
    return m_selectedCameraEstimateRow;
}
