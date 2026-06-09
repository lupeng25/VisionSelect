#include "ui/pages/ResultsPage.h"

#include "selection/SelectionEngine.h"
#include "ui/UiHelpers.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLayoutItem>
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

QString shortProduct(const QString &manufacturer, const QString &model)
{
    const QString label = productLabel(manufacturer, model);
    return label.size() > 34 ? label.left(31) + QStringLiteral("...") : label;
}
}

ResultsPage::ResultsPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(26, 22, 26, 22);
    layout->setSpacing(14);

    QHBoxLayout *resultActions = new QHBoxLayout;
    QPushButton *exportPdfButton = actionButton(localizedText("导出 PDF", "Export PDF"), QStringLiteral(":/icons/ui/export.png"), true);
    QPushButton *exportBomButton = actionButton(localizedText("导出 BOM CSV", "Export BOM CSV"), QStringLiteral(":/icons/ui/export.png"), true);
    connect(exportPdfButton, &QPushButton::clicked, this, &ResultsPage::exportPdfRequested);
    connect(exportBomButton, &QPushButton::clicked, this, &ResultsPage::exportBomRequested);
    resultActions->addWidget(exportPdfButton);
    resultActions->addWidget(exportBomButton);
    resultActions->addStretch();
    QWidget *actionsWidget = new QWidget;
    actionsWidget->setLayout(resultActions);
    layout->addWidget(pageHeader(localizedText("推荐结果", "Recommended Results"),
        localizedText("优先展示可交付方案、主要风险和核心 BOM，保留完整明细用于工程复核。",
                      "Prioritize deliverable plans, key risks, and core BOM while preserving full engineering detail."),
        actionsWidget));

    m_summaryLabel = new QLabel;
    m_summaryLabel->setObjectName(QStringLiteral("SectionTitle"));
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_summaryLabel);

    QFrame *cards = new QFrame;
    cards->setObjectName(QStringLiteral("SectionCard"));
    m_cardsLayout = new QHBoxLayout(cards);
    m_cardsLayout->setContentsMargins(12, 12, 12, 12);
    m_cardsLayout->setSpacing(12);
    layout->addWidget(cards);

    m_table = new QTableWidget;
    setupTable(m_table);
    m_table->setColumnCount(11);
    m_table->setHorizontalHeaderLabels({
        localizedText("类型", "Type"), localizedText("状态", "Status"), localizedText("得分", "Score"),
        localizedText("相机", "Camera"), localizedText("镜头", "Lens"), localizedText("光源", "Light"),
        QStringLiteral("FOV(mm)"), localizedText("物方像素", "Obj Pixel"),
        localizedText("倍率/焦距", "Mag/Focal"), QStringLiteral("WD/DOF"),
        localizedText("风险", "Risk")
    });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    const int resultColumnWidths[] = {70, 86, 54, 156, 156, 150, 88, 92, 96, 102};
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

void ResultsPage::refreshCards(const SelectionRequest &request)
{
    Q_UNUSED(request)
    if (!m_cardsLayout)
        return;

    while (QLayoutItem *child = m_cardsLayout->takeAt(0)) {
        if (child->widget())
            child->widget()->deleteLater();
        delete child;
    }

    const int count = qMin(3, m_results.size());
    if (count == 0) {
        m_cardsLayout->addWidget(metricCard(localizedText("暂无推荐方案", "No Recommendations"),
            localizedText("请先计算", "Calculate first"),
            localizedText("返回需求建模页输入约束后生成候选方案。", "Return to requirements and generate candidate plans."),
            QStringLiteral("warn")));
        return;
    }

    QVector<int> cardIndexes;
    cardIndexes.reserve(count);
    for (int i = 0; i < count; ++i)
        cardIndexes.append(i);

    bool hasFixedFocalCard = false;
    for (int index : cardIndexes) {
        if (!m_results.at(index).isTelecentric()) {
            hasFixedFocalCard = true;
            break;
        }
    }
    if (!hasFixedFocalCard && cardIndexes.size() >= 3) {
        for (int i = count; i < m_results.size(); ++i) {
            if (!m_results.at(i).isTelecentric() && m_results.at(i).hardConstraintsPassed) {
                cardIndexes[2] = i;
                break;
            }
        }
    }

    for (int index : cardIndexes) {
        const int i = index;
        const SelectionResult &r = m_results.at(i);
        QFrame *card = new QFrame;
        card->setObjectName(QStringLiteral("PlanCard"));
        setWidgetState(card, r.hardConstraintsPassed ? QStringLiteral("good") : QStringLiteral("bad"));
        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(14, 12, 14, 12);
        cardLayout->setSpacing(7);

        QHBoxLayout *top = new QHBoxLayout;
        QLabel *rank = new QLabel(QStringLiteral("#%1  %2").arg(i + 1).arg(lensCategory(r)));
        rank->setObjectName(QStringLiteral("MetricLabel"));
        top->addWidget(rank, 1);
        top->addWidget(statusBadge(compatibilityText(r), r.hardConstraintsPassed ? QStringLiteral("good") : QStringLiteral("bad")));
        cardLayout->addLayout(top);

        QLabel *score = new QLabel(QStringLiteral("%1").arg(r.score.score, 0, 'f', 1));
        score->setObjectName(QStringLiteral("MetricValue"));
        cardLayout->addWidget(score);

        QLabel *bom = new QLabel(QStringLiteral("%1\n%2\n%3")
            .arg(shortProduct(r.camera.manufacturer, r.camera.model),
                 shortProduct(r.lens.manufacturer, r.lens.model),
                 shortProduct(r.light.manufacturer, r.light.model)));
        bom->setObjectName(QStringLiteral("MetricDetail"));
        bom->setWordWrap(true);
        cardLayout->addWidget(bom);

        QLabel *calculation = new QLabel(QStringLiteral("FOV %1 x %2 mm | %3 um/px")
            .arg(r.effectiveFovWidthMm, 0, 'f', 1)
            .arg(r.effectiveFovHeightMm, 0, 'f', 1)
            .arg(r.objectPixelSizeUm, 0, 'f', 2));
        calculation->setObjectName(QStringLiteral("MetricDetail"));
        calculation->setWordWrap(true);
        cardLayout->addWidget(calculation);

        QLabel *risk = statusBadge(riskSummary(r), (r.score.risks.isEmpty() && r.hardFailures.isEmpty()) ? QStringLiteral("good") : QStringLiteral("warn"));
        risk->setWordWrap(true);
        cardLayout->addWidget(risk);
        m_cardsLayout->addWidget(card, 1);
    }
}

void ResultsPage::refreshTable(const SelectionRequest &request)
{
    if (!m_table)
        return;

    refreshCards(request);
    m_summaryLabel->setText(localizedText("需求 FOV：%1 x %2 mm，目标物方像素：%3 um/px，候选方案：%4 个",
                                          "Required FOV: %1 x %2 mm, target object pixel: %3 um/px, candidate plans: %4")
        .arg(SelectionEngine::requiredFovWidth(request), 0, 'f', 2)
        .arg(SelectionEngine::requiredFovHeight(request), 0, 'f', 2)
        .arg(SelectionEngine::targetObjectPixelUm(request), 0, 'f', 2)
        .arg(m_results.size()));

    m_table->setSortingEnabled(false);
    m_table->setRowCount(m_results.size());
    for (int row = 0; row < m_results.size(); ++row) {
        const SelectionResult &r = m_results.at(row);
        m_table->setItem(row, 0, indexedItem(lensCategory(r), row));
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
    } else if (m_details) {
        m_details->clear();
    }
}

void ResultsPage::refreshDetails(int row)
{
    if (!m_details || row < 0 || row >= m_results.size())
        return;

    const SelectionResult &r = m_results.at(row);
    QString text;
    text += QStringLiteral("<h3>") + r.schemeTitle + localizedText("：", ": ")
        + productLabel(r.camera.manufacturer, r.camera.model) + QStringLiteral(" + ")
        + productLabel(r.lens.manufacturer, r.lens.model) + QStringLiteral(" + ")
        + productLabel(r.light.manufacturer, r.light.model) + QStringLiteral("</h3>");
    text += localizedText("<p><b>公式：</b>%1</p>", "<p><b>Formula:</b> %1</p>").arg(r.formulaSummary);
    text += localizedText("<p><b>适配状态：</b>%1</p>", "<p><b>Compatibility:</b> %1</p>").arg(compatibilityText(r));
    text += localizedText("<p><b>有效 FOV：</b>%1 x %2 mm；<b>物方像素：</b>%3 um/px；<b>接口带宽需求：</b>%4 MB/s。</p>",
                          "<p><b>Effective FOV:</b> %1 x %2 mm; <b>object pixel:</b> %3 um/px; <b>required bandwidth:</b> %4 MB/s.</p>")
        .arg(r.effectiveFovWidthMm, 0, 'f', 2)
        .arg(r.effectiveFovHeightMm, 0, 'f', 2)
        .arg(r.objectPixelSizeUm, 0, 'f', 2)
        .arg(r.bandwidthRequiredMBps, 0, 'f', 1);
    text += localizedText("<p><b>接口/存储：</b>单帧 %1 MB；接口余量 %2 MB/s；带宽利用率 %3%；原始存储约 %4 GB/h；镜头 MP 利用率 %5%。</p>",
                          "<p><b>Interface / storage:</b> frame %1 MB; interface margin %2 MB/s; bandwidth utilization %3%; raw storage about %4 GB/h; lens MP utilization %5%.</p>")
        .arg(r.framePayloadMB, 0, 'f', 2)
        .arg(r.interfaceCapacityMBps, 0, 'f', 1)
        .arg(r.bandwidthUtilizationPercent, 0, 'f', 0)
        .arg(r.storagePerHourGB, 0, 'f', 0)
        .arg(r.lensMegapixelUtilizationPercent, 0, 'f', 0);
    if (r.maxExposureUsForOnePixelBlur > 0.0) {
        text += localizedText("<p><b>运动模糊：</b>建议曝光不高于 %1 us，约束在 1 个目标物方像素内。</p>",
                              "<p><b>Motion blur:</b> keep exposure no higher than %1 us to stay near one target object pixel.</p>")
            .arg(r.maxExposureUsForOnePixelBlur, 0, 'f', 1);
    }
    text += localizedText("<p><b>工程估算：</b>DOF %1 mm；畸变边缘误差约 %2 um；光源覆盖余量 %3%。</p>",
                          "<p><b>Engineering estimate:</b> DOF %1 mm; edge distortion error about %2 um; light coverage margin %3%.</p>")
        .arg(r.estimatedDofMm, 0, 'f', 2)
        .arg(r.distortionErrorUm, 0, 'f', 2)
        .arg(r.lightCoverageMarginPercent, 0, 'f', 0);
    if (r.isTelecentric()) {
        text += localizedText("<p><b>远心参数：</b>PMAG %1x，标称 WD %2 mm，远心度 %3 deg，畸变 %4%，DOF %5 mm，残余视差估算 %6 um。</p>",
                              "<p><b>Telecentric parameters:</b> PMAG %1x, nominal WD %2 mm, telecentricity %3 deg, distortion %4%, DOF %5 mm, residual parallax estimate %6 um.</p>")
            .arg(r.magnification, 0, 'f', 3)
            .arg(r.lens.nominalWorkingDistanceMm, 0, 'f', 1)
            .arg(r.lens.telecentricityDeg, 0, 'f', 3)
            .arg(r.lens.distortionPercent, 0, 'f', 3)
            .arg(r.estimatedDofMm, 0, 'f', 2)
            .arg(r.residualTelecentricErrorUm, 0, 'f', 2);
    }
    text += localizedText("<p><b>推荐理由：</b>%1</p>", "<p><b>Reasons:</b> %1</p>").arg(r.score.reasons.join(localizedText("；", "; ")));
    const QString riskText = (r.score.risks.isEmpty() && r.hardFailures.isEmpty())
        ? localizedText("无主要风险，仍建议结合厂家 MTF/DOF 与现场光源实测确认。",
                        "No major risk; still verify with vendor MTF/DOF data and on-site lighting tests.")
        : riskSummary(r);
    text += localizedText("<p><b>风险提示：</b>%1</p>", "<p><b>Risks:</b> %1</p>").arg(riskText);
    m_details->setHtml(text);
}
