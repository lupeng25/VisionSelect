#include "ui/pages/ComparisonPage.h"

#include "ui/UiHelpers.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QtGlobal>

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

QString lensCategory(const SelectionResult &result)
{
    return result.isTelecentric()
        ? localizedText("远心", "Telecentric")
        : localizedText("普通", "Fixed-focal");
}
}

ComparisonPage::ComparisonPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(26, 22, 26, 22);
    layout->setSpacing(14);

    QHBoxLayout *actions = new QHBoxLayout;
    QPushButton *recalculateButton = actionButton(localizedText("重新计算", "Recalculate"), QStringLiteral(":/icons/ui/calculate.png"));
    QPushButton *exportBomButton = actionButton(localizedText("导出 BOM CSV", "Export BOM CSV"), QStringLiteral(":/icons/ui/export.png"), true);
    connect(recalculateButton, &QPushButton::clicked, this, &ComparisonPage::recalculateRequested);
    connect(exportBomButton, &QPushButton::clicked, this, &ComparisonPage::exportBomRequested);
    actions->addWidget(recalculateButton);
    actions->addWidget(exportBomButton);
    actions->addStretch();
    QWidget *actionsWidget = new QWidget;
    actionsWidget->setLayout(actions);
    layout->addWidget(pageHeader(localizedText("方案对比", "Plan Comparison"),
        localizedText("横向比较前五个候选方案的 BOM、余量、风险和交付判断。", "Compare BOM, margins, risks, and delivery verdicts across the top five plans."),
        actionsWidget));

    m_table = new QTableWidget;
    setupTable(m_table);
    m_table->setColumnCount(13);
    m_table->setHorizontalHeaderLabels({
        localizedText("方案", "Plan"), localizedText("状态", "Status"), localizedText("得分", "Score"),
        localizedText("相机", "Camera"), localizedText("镜头", "Lens"), localizedText("光源", "Light"),
        QStringLiteral("FOV"), localizedText("物方像素", "Object Pixel"), localizedText("曝光上限", "Exposure Limit"),
        localizedText("带宽/存储", "Bandwidth / Storage"), QStringLiteral("DOF/Dist."),
        localizedText("光源余量", "Light Margin"), localizedText("主要风险", "Main Risk")
    });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    const int compareWidths[] = {80, 92, 56, 122, 122, 118, 82, 92, 92, 108, 92, 82};
    for (int column = 0; column < 12; ++column)
        m_table->setColumnWidth(column, compareWidths[column]);
    m_table->horizontalHeader()->setSectionResizeMode(12, QHeaderView::Stretch);
    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) {
        const int sourceRow = rowSourceIndex(m_table, row);
        refreshDetails(sourceRow >= 0 ? sourceRow : row);
    });

    m_details = new QTextEdit;
    m_details->setReadOnly(true);
    m_details->setMinimumHeight(150);

    layout->addWidget(m_table, 1);
    layout->addWidget(m_details);
}

void ComparisonPage::setResults(const QVector<SelectionResult> &results)
{
    m_results = results;

    if (!m_table)
        return;

    const int count = qMin(5, m_results.size());
    m_table->setSortingEnabled(false);
    m_table->setRowCount(count);
    for (int row = 0; row < count; ++row) {
        const SelectionResult &r = m_results.at(row);
        m_table->setItem(row, 0, indexedItem(QStringLiteral("#%1 %2")
            .arg(row + 1)
            .arg(lensCategory(r)), row));
        m_table->setItem(row, 1, item(compatibilityText(r)));
        m_table->setItem(row, 2, item(number(r.score.score, 1)));
        m_table->setItem(row, 3, item(productLabel(r.camera.manufacturer, r.camera.model)));
        m_table->setItem(row, 4, item(productLabel(r.lens.manufacturer, r.lens.model)));
        m_table->setItem(row, 5, item(productLabel(r.light.manufacturer, r.light.model)));
        m_table->setItem(row, 6, item(QStringLiteral("%1 x %2")
            .arg(r.effectiveFovWidthMm, 0, 'f', 1)
            .arg(r.effectiveFovHeightMm, 0, 'f', 1)));
        m_table->setItem(row, 7, item(QStringLiteral("%1 um").arg(r.objectPixelSizeUm, 0, 'f', 2)));
        m_table->setItem(row, 8, item(exposureText(r.maxExposureUsForOnePixelBlur)));
        m_table->setItem(row, 9, item(QStringLiteral("%1% / %2 GB/h")
            .arg(r.bandwidthUtilizationPercent, 0, 'f', 0)
            .arg(r.storagePerHourGB, 0, 'f', 0)));
        m_table->setItem(row, 10, item(QStringLiteral("%1 mm / %2 um")
            .arg(r.estimatedDofMm, 0, 'f', 2)
            .arg(r.distortionErrorUm, 0, 'f', 1)));
        m_table->setItem(row, 11, item(QStringLiteral("%1%")
            .arg(r.lightCoverageMarginPercent, 0, 'f', 0)));
        m_table->setItem(row, 12, item(riskSummary(r)));
    }
    m_table->setSortingEnabled(true);

    if (count > 0) {
        selectRowBySourceIndex(m_table, 0);
        refreshDetails(0);
    } else if (m_details) {
        m_details->setPlainText(localizedText("暂无方案，请先计算推荐结果。", "No plan is available. Calculate recommendations first."));
    }
}

void ComparisonPage::refreshDetails(int row)
{
    if (!m_details || row < 0 || row >= m_results.size())
        return;

    const SelectionResult &r = m_results.at(row);
    QString text;
    text += QStringLiteral("<h3>") + localizedText("方案 #", "Plan #") + QString::number(row + 1)
        + QStringLiteral(" - ") + r.schemeTitle + QStringLiteral("</h3>");
    text += localizedText("<p><b>BOM：</b>相机 %1；镜头 %2；光源 %3。</p>",
                          "<p><b>BOM:</b> camera %1; lens %2; light %3.</p>")
        .arg(productLabel(r.camera.manufacturer, r.camera.model),
             productLabel(r.lens.manufacturer, r.lens.model),
             productLabel(r.light.manufacturer, r.light.model));
    text += localizedText("<p><b>适配状态：</b>%1</p>", "<p><b>Compatibility:</b> %1</p>").arg(compatibilityText(r));
    text += localizedText("<p><b>计算结果：</b>FOV %1 x %2 mm；物方像素 %3 um/px；曝光上限 %4；DOF %5 mm；畸变误差 %6 um。</p>",
                          "<p><b>Calculation:</b> FOV %1 x %2 mm; object pixel %3 um/px; exposure limit %4; DOF %5 mm; distortion error %6 um.</p>")
        .arg(r.effectiveFovWidthMm, 0, 'f', 2)
        .arg(r.effectiveFovHeightMm, 0, 'f', 2)
        .arg(r.objectPixelSizeUm, 0, 'f', 2)
        .arg(exposureText(r.maxExposureUsForOnePixelBlur))
        .arg(r.estimatedDofMm, 0, 'f', 2)
        .arg(r.distortionErrorUm, 0, 'f', 2);
    text += localizedText("<p><b>接口与存储：</b>单帧原始数据 %1 MB；吞吐 %2 MB/s；接口余量 %3 MB/s；带宽利用率 %4%；原始存储约 %5 GB/h。</p>",
                          "<p><b>Interface and storage:</b> raw frame %1 MB; throughput %2 MB/s; interface margin %3 MB/s; bandwidth utilization %4%; raw storage about %5 GB/h.</p>")
        .arg(r.framePayloadMB, 0, 'f', 2)
        .arg(r.bandwidthRequiredMBps, 0, 'f', 1)
        .arg(r.interfaceCapacityMBps, 0, 'f', 1)
        .arg(r.bandwidthUtilizationPercent, 0, 'f', 0)
        .arg(r.storagePerHourGB, 0, 'f', 0);
    text += localizedText("<p><b>镜头/光源余量：</b>镜头 MP 利用率 %1%；光源覆盖余量 %2%。</p>",
                          "<p><b>Lens / light margins:</b> lens MP utilization %1%; light coverage margin %2%.</p>")
        .arg(r.lensMegapixelUtilizationPercent, 0, 'f', 0)
        .arg(r.lightCoverageMarginPercent, 0, 'f', 0);
    text += localizedText("<p><b>推荐理由：</b>%1</p>", "<p><b>Reasons:</b> %1</p>").arg(r.score.reasons.join(localizedText("；", "; ")));
    text += localizedText("<p><b>风险：</b>%1</p>", "<p><b>Risks:</b> %1</p>").arg(riskSummary(r));
    m_details->setHtml(text);
}
