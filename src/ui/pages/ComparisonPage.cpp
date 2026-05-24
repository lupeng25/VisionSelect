#include "ui/pages/ComparisonPage.h"

#include "ui/UiHelpers.h"

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
}

ComparisonPage::ComparisonPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);

    layout->addWidget(pageTitle(QString::fromUtf8("方案对比")));

    QHBoxLayout *actions = new QHBoxLayout;
    QPushButton *recalculateButton = new QPushButton(QString::fromUtf8("重新计算"));
    QPushButton *exportBomButton = new QPushButton(QString::fromUtf8("导出 BOM CSV"));
    exportBomButton->setObjectName(QStringLiteral("SecondaryButton"));
    connect(recalculateButton, &QPushButton::clicked, this, &ComparisonPage::recalculateRequested);
    connect(exportBomButton, &QPushButton::clicked, this, &ComparisonPage::exportBomRequested);
    actions->addWidget(recalculateButton);
    actions->addWidget(exportBomButton);
    actions->addStretch();
    layout->addLayout(actions);

    m_table = new QTableWidget;
    setupTable(m_table);
    m_table->setColumnCount(12);
    m_table->setHorizontalHeaderLabels({
        QString::fromUtf8("方案"), QString::fromUtf8("得分"), QString::fromUtf8("相机"),
        QString::fromUtf8("镜头"), QString::fromUtf8("光源"), QStringLiteral("FOV"),
        QString::fromUtf8("物方像素"), QString::fromUtf8("曝光上限"), QString::fromUtf8("带宽/存储"),
        QStringLiteral("DOF/畸变"), QString::fromUtf8("光源余量"), QString::fromUtf8("主要风险")
    });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    const int compareWidths[] = {58, 44, 112, 112, 110, 75, 70, 75, 90, 88, 60};
    for (int column = 0; column < 11; ++column)
        m_table->setColumnWidth(column, compareWidths[column]);
    m_table->horizontalHeader()->setSectionResizeMode(11, QHeaderView::Stretch);
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
            .arg(r.isTelecentric() ? QString::fromUtf8("远心") : QString::fromUtf8("普通")), row));
        m_table->setItem(row, 1, item(number(r.score.score, 1)));
        m_table->setItem(row, 2, item(productLabel(r.camera.manufacturer, r.camera.model)));
        m_table->setItem(row, 3, item(productLabel(r.lens.manufacturer, r.lens.model)));
        m_table->setItem(row, 4, item(productLabel(r.light.manufacturer, r.light.model)));
        m_table->setItem(row, 5, item(QStringLiteral("%1 x %2")
            .arg(r.effectiveFovWidthMm, 0, 'f', 1)
            .arg(r.effectiveFovHeightMm, 0, 'f', 1)));
        m_table->setItem(row, 6, item(QStringLiteral("%1 um").arg(r.objectPixelSizeUm, 0, 'f', 2)));
        m_table->setItem(row, 7, item(exposureText(r.maxExposureUsForOnePixelBlur)));
        m_table->setItem(row, 8, item(QStringLiteral("%1% / %2 GB/h")
            .arg(r.bandwidthUtilizationPercent, 0, 'f', 0)
            .arg(r.storagePerHourGB, 0, 'f', 0)));
        m_table->setItem(row, 9, item(QStringLiteral("%1 mm / %2 um")
            .arg(r.estimatedDofMm, 0, 'f', 2)
            .arg(r.distortionErrorUm, 0, 'f', 1)));
        m_table->setItem(row, 10, item(QStringLiteral("%1%")
            .arg(r.lightCoverageMarginPercent, 0, 'f', 0)));
        m_table->setItem(row, 11, item(riskSummary(r)));
    }
    m_table->setSortingEnabled(true);

    if (count > 0) {
        selectRowBySourceIndex(m_table, 0);
        refreshDetails(0);
    } else if (m_details) {
        m_details->setPlainText(QString::fromUtf8("暂无方案，请先计算推荐结果。"));
    }
}

void ComparisonPage::refreshDetails(int row)
{
    if (!m_details || row < 0 || row >= m_results.size())
        return;

    const SelectionResult &r = m_results.at(row);
    QString text;
    text += QStringLiteral("<h3>") + QString::fromUtf8("方案 #") + QString::number(row + 1)
        + QStringLiteral(" - ") + r.schemeTitle + QStringLiteral("</h3>");
    text += QString::fromUtf8("<p><b>BOM：</b>相机 %1；镜头 %2；光源 %3。</p>")
        .arg(productLabel(r.camera.manufacturer, r.camera.model),
             productLabel(r.lens.manufacturer, r.lens.model),
             productLabel(r.light.manufacturer, r.light.model));
    text += QString::fromUtf8("<p><b>计算结果：</b>FOV %1 x %2 mm；物方像素 %3 um/px；曝光上限 %4；DOF %5 mm；畸变误差 %6 um。</p>")
        .arg(r.effectiveFovWidthMm, 0, 'f', 2)
        .arg(r.effectiveFovHeightMm, 0, 'f', 2)
        .arg(r.objectPixelSizeUm, 0, 'f', 2)
        .arg(exposureText(r.maxExposureUsForOnePixelBlur))
        .arg(r.estimatedDofMm, 0, 'f', 2)
        .arg(r.distortionErrorUm, 0, 'f', 2);
    text += QString::fromUtf8("<p><b>接口与存储：</b>单帧原始数据 %1 MB；吞吐 %2 MB/s；接口余量 %3 MB/s；带宽利用率 %4%；原始存储约 %5 GB/h。</p>")
        .arg(r.framePayloadMB, 0, 'f', 2)
        .arg(r.bandwidthRequiredMBps, 0, 'f', 1)
        .arg(r.interfaceCapacityMBps, 0, 'f', 1)
        .arg(r.bandwidthUtilizationPercent, 0, 'f', 0)
        .arg(r.storagePerHourGB, 0, 'f', 0);
    text += QString::fromUtf8("<p><b>镜头/光源余量：</b>镜头 MP 利用率 %1%；光源覆盖余量 %2%。</p>")
        .arg(r.lensMegapixelUtilizationPercent, 0, 'f', 0)
        .arg(r.lightCoverageMarginPercent, 0, 'f', 0);
    text += QString::fromUtf8("<p><b>推荐理由：</b>%1</p>").arg(r.score.reasons.join(QString::fromUtf8("；")));
    text += QString::fromUtf8("<p><b>风险：</b>%1</p>").arg(riskSummary(r));
    m_details->setHtml(text);
}
