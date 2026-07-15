#include "ui/pages/ResultsPage.h"

#include "selection/SelectionEngine.h"
#include "ui/ResultPresentation.h"
#include "ui/UiHelpers.h"
#include "ui/UiSettings.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLayoutItem>
#include <QMouseEvent>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QSplitter>
#include <QStyle>
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

QString htmlText(const QString &text)
{
    return text.toHtmlEscaped();
}

QString htmlList(const QStringList &values, const QString &separator)
{
    QStringList escaped;
    for (const QString &value : values)
        escaped << htmlText(value);
    return escaped.join(separator);
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
    m_table->setObjectName(QStringLiteral("results/table"));
    m_table->setAccessibleName(localizedText("推荐方案表", "Recommended plans table"));
    setupTable(m_table);
    m_table->setColumnCount(11);
    m_table->setHorizontalHeaderLabels({
        localizedText("类型", "Type"), localizedText("状态", "Status"), localizedText("匹配度", "Match"),
        localizedText("相机", "Camera"), localizedText("镜头", "Lens"), localizedText("光源", "Light"),
        QStringLiteral("FOV(mm)"), localizedText("物方像素", "Obj Pixel"),
        localizedText("倍率/焦距", "Mag/Focal"), QStringLiteral("WD/DOF"),
        localizedText("风险", "Risk")
    });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    const int resultColumnWidths[] = {70, 96, 78, 150, 150, 140, 96, 96, 100, 110};
    for (int column = 0; column < 10; ++column)
        m_table->setColumnWidth(column, resultColumnWidths[column]);
    m_table->horizontalHeader()->setSectionResizeMode(10, QHeaderView::Stretch);
    m_table->setMinimumWidth(640);
    connect(m_table, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) {
        const int sourceRow = rowSourceIndex(m_table, row);
        refreshDetails(sourceRow >= 0 ? sourceRow : row);
        selectCard(sourceRow >= 0 ? sourceRow : row);
    });

    m_details = new QTextEdit;
    m_details->setObjectName(QStringLiteral("ResultsDetails"));
    m_details->setAccessibleName(localizedText("方案工程详情", "Plan engineering details"));
    m_details->setReadOnly(true);
    m_details->setMinimumHeight(150);

    m_splitter = new QSplitter(Qt::Vertical, this);
    m_splitter->setObjectName(QStringLiteral("results/main"));
    m_splitter->addWidget(m_table);
    m_splitter->addWidget(m_details);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({430, 180});
    UiSettings::instance().restoreSplitter(QStringLiteral("results/main"), m_splitter);
    UiSettings::instance().restoreHeader(QStringLiteral("results/table"), m_table->horizontalHeader());
    layout->addWidget(m_splitter, 1);
}

ResultsPage::~ResultsPage()
{
    UiSettings::instance().saveSplitter(QStringLiteral("results/main"), m_splitter);
    UiSettings::instance().saveHeader(QStringLiteral("results/table"), m_table ? m_table->horizontalHeader() : nullptr);
}

void ResultsPage::setBusy(const SelectionRequest &request)
{
    Q_UNUSED(request)
    m_results.clear();
    m_presentations.clear();

    if (m_cardsLayout) {
        while (QLayoutItem *child = m_cardsLayout->takeAt(0)) {
            if (child->widget())
                child->widget()->deleteLater();
            delete child;
        }
        m_cardsLayout->addWidget(metricCard(localizedText("计算中", "Calculating"),
            localizedText("候选检索", "Candidate Search"),
            localizedText("正在从产品库检索候选并进行评分。",
                          "Fetching catalog candidates and scoring recommendations."),
            QStringLiteral("warning")));
    }

    if (m_summaryLabel)
        m_summaryLabel->setText(localizedText("正在计算推荐方案...",
                                             "Calculating recommended plans..."));
    if (m_table) {
        m_table->setSortingEnabled(false);
        m_table->setRowCount(0);
    }
    if (m_details)
        m_details->setPlainText(localizedText("请稍候，计算完成后会自动更新结果。",
                                             "Please wait; results will update automatically when calculation completes."));
}

void ResultsPage::setError(const QString &message)
{
    m_results.clear();
    m_presentations.clear();
    while (QLayoutItem *child = m_cardsLayout->takeAt(0)) {
        if (child->widget())
            child->widget()->deleteLater();
        delete child;
    }
    QFrame *stateCard = new QFrame;
    stateCard->setObjectName(QStringLiteral("SectionCard"));
    QVBoxLayout *stateLayout = new QVBoxLayout(stateCard);
    stateLayout->addWidget(statusBadge(localizedText("计算失败", "Calculation failed"), QStringLiteral("error")));
    QLabel *messageLabel = new QLabel(message);
    messageLabel->setWordWrap(true);
    messageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    stateLayout->addWidget(messageLabel);
    QHBoxLayout *actions = new QHBoxLayout;
    QPushButton *backButton = actionButton(localizedText("返回修改", "Back to inputs"), QString(), true);
    QPushButton *retryButton = actionButton(localizedText("重试", "Retry"));
    connect(backButton, &QPushButton::clicked, this, &ResultsPage::inputRequested);
    connect(retryButton, &QPushButton::clicked, this, &ResultsPage::retryRequested);
    actions->addStretch();
    actions->addWidget(backButton);
    actions->addWidget(retryButton);
    stateLayout->addLayout(actions);
    m_cardsLayout->addWidget(stateCard);
    m_summaryLabel->setText(localizedText("未能生成推荐方案", "Unable to generate recommendations"));
    m_table->setRowCount(0);
    m_details->setPlainText(message);
}

void ResultsPage::setResults(const QVector<SelectionResult> &results,
                             const SelectionRequest &request)
{
    m_results = results;
    m_presentations = buildResultPresentations(results);
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
            QStringLiteral("warning")));
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
        const ResultPresentation presentation = m_presentations.value(i);
        QFrame *card = new QFrame;
        card->setObjectName(QStringLiteral("PlanCard"));
        card->setProperty("sourceIndex", i);
        card->setProperty("selected", i == 0);
        card->setCursor(Qt::PointingHandCursor);
        card->setAccessibleName(localizedText("推荐方案卡", "Recommendation card") + QStringLiteral(" %1").arg(i + 1));
        card->installEventFilter(this);
        setWidgetState(card, presentation.compatible ? QStringLiteral("success") : QStringLiteral("error"));
        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(14, 12, 14, 12);
        cardLayout->setSpacing(7);

        QHBoxLayout *top = new QHBoxLayout;
        QLabel *rank = new QLabel(QStringLiteral("#%1  %2").arg(i + 1).arg(lensCategory(r)));
        rank->setObjectName(QStringLiteral("MetricLabel"));
        top->addWidget(rank, 1);
        top->addWidget(statusBadge(compatibilityText(r), presentation.compatible ? QStringLiteral("success") : QStringLiteral("error")));
        cardLayout->addLayout(top);

        const QString matchText = !presentation.compatible
            ? localizedText("不兼容", "Incompatible")
            : (presentation.matchAvailable
                ? localizedText("相对匹配度 %1%", "Relative match %1%").arg(presentation.relativeMatchPercent)
                : localizedText("暂无有效匹配度", "No valid match score"));
        QLabel *score = new QLabel(matchText);
        score->setObjectName(QStringLiteral("MetricValue"));
        score->setToolTip(localizedText("算法原始分：%1；相对匹配度仅用于本批候选比较。",
                                        "Raw algorithm score: %1. Relative match is only comparable within this batch.")
                              .arg(r.score.score, 0, 'f', 1));
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

        const QString riskText = presentation.riskItems.isEmpty()
            ? localizedText("✓ 0 项风险", "✓ No risks")
            : localizedText("⚠ %1 项风险：%2", "⚠ %1 risks: %2")
                  .arg(presentation.riskItems.size())
                  .arg(presentation.riskItems.first());
        const QString riskState = presentation.riskLevel == ResultRiskLevel::Error
            ? QStringLiteral("error")
            : (presentation.riskLevel == ResultRiskLevel::Warning ? QStringLiteral("warning") : QStringLiteral("success"));
        QLabel *risk = statusBadge(riskText, riskState);
        risk->setToolTip(presentation.riskItems.join(localizedText("；", "; ")));
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
        const ResultPresentation presentation = m_presentations.value(row);
        m_table->setItem(row, 0, indexedItem(lensCategory(r), row));
        m_table->setItem(row, 1, item(compatibilityText(r)));
        const QString matchText = !presentation.compatible
            ? localizedText("不兼容", "Incompatible")
            : (presentation.matchAvailable
                ? QStringLiteral("%1%").arg(presentation.relativeMatchPercent)
                : QStringLiteral("—"));
        QTableWidgetItem *matchItem = item(matchText);
        matchItem->setToolTip(localizedText("算法原始分：%1", "Raw algorithm score: %1").arg(r.score.score, 0, 'f', 1));
        m_table->setItem(row, 2, matchItem);
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
        m_table->setItem(row, 10, item(presentation.riskItems.isEmpty()
            ? localizedText("✓ 无主要风险", "✓ No major risk")
            : localizedText("%1 项：%2", "%1: %2").arg(presentation.riskItems.size()).arg(presentation.riskItems.first())));
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
    text += QStringLiteral("<h3>") + htmlText(r.schemeTitle) + localizedText("：", ": ")
        + htmlText(productLabel(r.camera.manufacturer, r.camera.model)) + QStringLiteral(" + ")
        + htmlText(productLabel(r.lens.manufacturer, r.lens.model)) + QStringLiteral(" + ")
        + htmlText(productLabel(r.light.manufacturer, r.light.model)) + QStringLiteral("</h3>");
    text += localizedText("<p><b>公式：</b>%1</p>", "<p><b>Formula:</b> %1</p>").arg(htmlText(r.formulaSummary));
    text += localizedText("<p><b>适配状态：</b>%1</p>", "<p><b>Compatibility:</b> %1</p>").arg(htmlText(compatibilityText(r)));
    const ResultPresentation presentation = m_presentations.value(row);
    const QString relativeText = presentation.matchAvailable
        ? QStringLiteral("%1%").arg(presentation.relativeMatchPercent)
        : localizedText("不可用", "Unavailable");
    text += localizedText("<p><b>相对匹配度：</b>%1；<b>算法原始分：</b>%2。相对匹配度仅用于本批兼容候选比较。</p>",
                          "<p><b>Relative match:</b> %1; <b>raw algorithm score:</b> %2. Relative match is only comparable within this compatible batch.</p>")
        .arg(relativeText)
        .arg(r.score.score, 0, 'f', 1);
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
        const QString telecentricityText = r.lens.hasTelecentricity()
            ? QStringLiteral("%1 deg").arg(r.lens.telecentricityDeg, 0, 'f', 3)
            : localizedText("未知", "Unknown");
        const QString residualParallaxText = r.lens.hasTelecentricity()
            ? QStringLiteral("%1 um").arg(r.residualTelecentricErrorUm, 0, 'f', 2)
            : localizedText("未知", "Unknown");
        text += localizedText("<p><b>远心参数：</b>PMAG %1x，标称 WD %2 mm，远心度 %3，畸变 %4%，DOF %5 mm，残余视差估算 %6。</p>",
                              "<p><b>Telecentric parameters:</b> PMAG %1x, nominal WD %2 mm, telecentricity %3, distortion %4%, DOF %5 mm, residual parallax estimate %6.</p>")
            .arg(r.magnification, 0, 'f', 3)
            .arg(r.lens.nominalWorkingDistanceMm, 0, 'f', 1)
            .arg(telecentricityText)
            .arg(r.lens.distortionPercent, 0, 'f', 3)
            .arg(r.estimatedDofMm, 0, 'f', 2)
            .arg(residualParallaxText);
    }
    text += localizedText("<p><b>推荐理由：</b>%1</p>", "<p><b>Reasons:</b> %1</p>")
        .arg(htmlList(r.score.reasons, localizedText("；", "; ")));
    const QString riskText = (r.score.risks.isEmpty() && r.hardFailures.isEmpty())
        ? localizedText("无主要风险，仍建议结合厂家 MTF/DOF 与现场光源实测确认。",
                        "No major risk; still verify with vendor MTF/DOF data and on-site lighting tests.")
        : riskSummary(r);
    text += localizedText("<p><b>风险提示：</b>%1</p>", "<p><b>Risks:</b> %1</p>").arg(htmlText(riskText));
    m_details->setHtml(text);
}

bool ResultsPage::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        QFrame *card = qobject_cast<QFrame *>(watched);
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (card && card->objectName() == QLatin1String("PlanCard") && mouseEvent->button() == Qt::LeftButton) {
            const int sourceIndex = card->property("sourceIndex").toInt();
            selectRowBySourceIndex(m_table, sourceIndex);
            refreshDetails(sourceIndex);
            selectCard(sourceIndex);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ResultsPage::selectCard(int sourceIndex)
{
    for (QFrame *card : findChildren<QFrame *>()) {
        if (card->objectName() != QLatin1String("PlanCard"))
            continue;
        const bool selected = card->property("sourceIndex").toInt() == sourceIndex;
        if (card->property("selected").toBool() == selected)
            continue;
        card->setProperty("selected", selected);
        card->style()->unpolish(card);
        card->style()->polish(card);
    }
}
