#include "ui/pages/ResultsPage.h"

#include "selection/SelectionEngine.h"
#include "ui/UiHelpers.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
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

ResultsPage::ResultsPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);

    layout->addWidget(pageTitle(QString::fromUtf8("\346\216\250\350\215\220\347\273\223\346\236\234")));
    m_summaryLabel = new QLabel;
    m_summaryLabel->setObjectName(QStringLiteral("SectionTitle"));
    layout->addWidget(m_summaryLabel);

    QHBoxLayout *resultActions = new QHBoxLayout;
    QPushButton *compareButton = new QPushButton(QString::fromUtf8("查看方案对比"));
    compareButton->setObjectName(QStringLiteral("SecondaryButton"));
    connect(compareButton, &QPushButton::clicked, this, &ResultsPage::comparisonRequested);
    resultActions->addWidget(compareButton);
    resultActions->addStretch();
    layout->addLayout(resultActions);

    m_table = new QTableWidget;
    setupTable(m_table);
    m_table->setColumnCount(11);
    m_table->setHorizontalHeaderLabels({
        QString::fromUtf8("\347\261\273\345\236\213"), QString::fromUtf8("状态"), QString::fromUtf8("\345\276\227\345\210\206"), QString::fromUtf8("\347\233\270\346\234\272"),
        QString::fromUtf8("\351\225\234\345\244\264"), QString::fromUtf8("\345\205\211\346\272\220"), QStringLiteral("FOV(mm)"),
        QString::fromUtf8("\347\211\251\346\226\271\345\203\217\347\264\240"), QString::fromUtf8("\345\200\215\347\216\207/\347\204\246\350\267\235"), QStringLiteral("WD/DOF"), QString::fromUtf8("\351\243\216\351\231\251")
    });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    const int resultColumnWidths[] = {46, 56, 50, 150, 150, 145, 78, 74, 78, 85};
    for (int column = 0; column < 10; ++column)
        m_table->setColumnWidth(column, resultColumnWidths[column]);
    m_table->horizontalHeader()->setSectionResizeMode(10, QHeaderView::Stretch);
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

void ResultsPage::setResults(const QVector<SelectionResult> &results,
                             const SelectionRequest &request)
{
    m_results = results;
    refreshTable(request);
}

void ResultsPage::refreshTable(const SelectionRequest &request)
{
    if (!m_table)
        return;

    m_summaryLabel->setText(QString::fromUtf8("\351\234\200\346\261\202 FOV\357\274\232%1 x %2 mm\357\274\214\347\233\256\346\240\207\347\211\251\346\226\271\345\203\217\347\264\240\357\274\232%3 um/px\357\274\214\345\200\231\351\200\211\346\226\271\346\241\210\357\274\232%4 \344\270\252")
        .arg(SelectionEngine::requiredFovWidth(request), 0, 'f', 2)
        .arg(SelectionEngine::requiredFovHeight(request), 0, 'f', 2)
        .arg(SelectionEngine::targetObjectPixelUm(request), 0, 'f', 2)
        .arg(m_results.size()));

    m_table->setSortingEnabled(false);
    m_table->setRowCount(m_results.size());
    for (int row = 0; row < m_results.size(); ++row) {
        const SelectionResult &r = m_results.at(row);
        m_table->setItem(row, 0, indexedItem(r.isTelecentric() ? QString::fromUtf8("\350\277\234\345\277\203") : QString::fromUtf8("\346\231\256\351\200\232"), row));
        m_table->setItem(row, 1, item(compatibilityText(r)));
        m_table->setItem(row, 2, item(number(r.score.score, 1)));
        m_table->setItem(row, 3, item(productLabel(r.camera.manufacturer, r.camera.model)));
        m_table->setItem(row, 4, item(productLabel(r.lens.manufacturer, r.lens.model)));
        m_table->setItem(row, 5, item(productLabel(r.light.manufacturer, r.light.model)));
        m_table->setItem(row, 6, item(QStringLiteral("%1 x %2")
            .arg(r.effectiveFovWidthMm, 0, 'f', 1)
            .arg(r.effectiveFovHeightMm, 0, 'f', 1)));
        m_table->setItem(row, 7, item(QStringLiteral("%1 um").arg(r.objectPixelSizeUm, 0, 'f', 2)));
        m_table->setItem(row, 8, item(r.isTelecentric()
            ? QStringLiteral("%1x").arg(r.magnification, 0, 'f', 3)
            : QStringLiteral("%1 mm").arg(r.lens.focalLengthMm, 0, 'f', 1)));
        m_table->setItem(row, 9, item(r.isTelecentric()
            ? QStringLiteral("WD %1 / DOF %2").arg(r.lens.nominalWorkingDistanceMm, 0, 'f', 0).arg(r.estimatedDofMm, 0, 'f', 1)
            : QStringLiteral("min WD %1 / DOF %2").arg(r.lens.minWorkingDistanceMm, 0, 'f', 0).arg(r.estimatedDofMm, 0, 'f', 1)));
        m_table->setItem(row, 10, item(riskSummary(r)));
    }
    m_table->setSortingEnabled(true);
    if (!m_results.isEmpty()) {
        selectRowBySourceIndex(m_table, 0);
        refreshDetails(0);
    }
}

void ResultsPage::refreshDetails(int row)
{
    if (!m_details || row < 0 || row >= m_results.size())
        return;

    const SelectionResult &r = m_results.at(row);
    QString text;
    text += QStringLiteral("<h3>") + r.schemeTitle + QString::fromUtf8("\357\274\232")
        + productLabel(r.camera.manufacturer, r.camera.model) + QStringLiteral(" + ")
        + productLabel(r.lens.manufacturer, r.lens.model) + QStringLiteral(" + ")
        + productLabel(r.light.manufacturer, r.light.model) + QStringLiteral("</h3>");
    text += QString::fromUtf8("<p><b>\345\205\254\345\274\217\357\274\232</b>%1</p>").arg(r.formulaSummary);
    text += QString::fromUtf8("<p><b>适配状态：</b>%1</p>").arg(compatibilityText(r));
    text += QString::fromUtf8("<p><b>\346\234\211\346\225\210 FOV\357\274\232</b>%1 x %2 mm\357\274\233<b>\347\211\251\346\226\271\345\203\217\347\264\240\357\274\232</b>%3 um/px\357\274\233<b>\346\216\245\345\217\243\345\270\246\345\256\275\357\274\232</b>%4 MB/s\343\200\202</p>")
        .arg(r.effectiveFovWidthMm, 0, 'f', 2)
        .arg(r.effectiveFovHeightMm, 0, 'f', 2)
        .arg(r.objectPixelSizeUm, 0, 'f', 2)
        .arg(r.bandwidthRequiredMBps, 0, 'f', 1);
    text += QString::fromUtf8("<p><b>接口/存储：</b>单帧 %1 MB；接口余量 %2 MB/s；带宽利用率 %3%；原始存储约 %4 GB/h；镜头 MP 利用率 %5%。</p>")
        .arg(r.framePayloadMB, 0, 'f', 2)
        .arg(r.interfaceCapacityMBps, 0, 'f', 1)
        .arg(r.bandwidthUtilizationPercent, 0, 'f', 0)
        .arg(r.storagePerHourGB, 0, 'f', 0)
        .arg(r.lensMegapixelUtilizationPercent, 0, 'f', 0);
    if (r.maxExposureUsForOnePixelBlur > 0.0) {
        text += QString::fromUtf8("<p><b>运动模糊：</b>建议曝光不高于 %1 us 才能控制在约 1 个目标物方像素内。</p>")
            .arg(r.maxExposureUsForOnePixelBlur, 0, 'f', 1);
    }
    text += QString::fromUtf8("<p><b>工程估算：</b>DOF %1 mm；畸变边缘误差约 %2 um；光源覆盖余量 %3%。</p>")
        .arg(r.estimatedDofMm, 0, 'f', 2)
        .arg(r.distortionErrorUm, 0, 'f', 2)
        .arg(r.lightCoverageMarginPercent, 0, 'f', 0);
    if (r.isTelecentric()) {
        text += QString::fromUtf8("<p><b>\350\277\234\345\277\203\345\217\202\346\225\260\357\274\232</b>PMAG %1x\357\274\214\346\240\207\347\247\260 WD %2 mm\357\274\214\350\277\234\345\277\203\345\272\246 %3 deg\357\274\214\347\225\270\345\217\230 %4%\357\274\214DOF %5 mm\357\274\214\346\256\213\344\275\231\350\247\206\345\267\256\344\274\260\347\256\227 %6 um\343\200\202</p>")
            .arg(r.magnification, 0, 'f', 3)
            .arg(r.lens.nominalWorkingDistanceMm, 0, 'f', 1)
            .arg(r.lens.telecentricityDeg, 0, 'f', 3)
            .arg(r.lens.distortionPercent, 0, 'f', 3)
            .arg(r.estimatedDofMm, 0, 'f', 2)
            .arg(r.residualTelecentricErrorUm, 0, 'f', 2);
    }
    text += QString::fromUtf8("<p><b>\346\216\250\350\215\220\347\220\206\347\224\261\357\274\232</b>%1</p>").arg(r.score.reasons.join(QString::fromUtf8("\357\274\233")));
    const QString riskText = (r.score.risks.isEmpty() && r.hardFailures.isEmpty())
        ? QString::fromUtf8("\346\227\240\344\270\273\350\246\201\351\243\216\351\231\251\357\274\214\344\273\215\345\273\272\350\256\256\347\273\223\345\220\210\345\216\202\345\256\266 MTF/DOF \344\270\216\347\216\260\345\234\272\345\205\211\346\272\220\345\256\236\346\265\213\347\241\256\350\256\244\343\200\202")
        : riskSummary(r);
    text += QString::fromUtf8("<p><b>\351\243\216\351\231\251\346\217\220\347\244\272\357\274\232</b>%1</p>").arg(riskText);
    m_details->setHtml(text);
}
