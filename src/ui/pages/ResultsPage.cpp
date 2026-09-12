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
#include <QSignalBlocker>
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
        ? localizedText("远心", "Tele")
        : localizedText("普通", "Fixed");
}

QString shortProduct(const QString &manufacturer, const QString &model)
{
    const QString label = productLabel(manufacturer, model);
    return label;
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
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(8);

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
        localizedText("先校核必要条件与待确认资料，再比较候选方案。",
                      "Check requirements and missing specifications before comparing candidates."),
        actionsWidget));

    m_summaryLabel = new QLabel;
    m_summaryLabel->setObjectName(QStringLiteral("SectionTitle"));
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_summaryLabel);

    m_staleBanner = new QFrame;
    m_staleBanner->setObjectName("ResultsOutdatedBanner");
    auto *staleLayout = new QHBoxLayout(m_staleBanner);
    staleLayout->setContentsMargins(8, 4, 8, 4);
    auto *staleText = new QLabel(localizedText("需求已修改，当前仍是上次结果；导出保留该次需求。",
        "Requirements changed. Results and exports still use the previous calculation."));
    staleText->setWordWrap(true);
    staleLayout->addWidget(staleText, 1);
    auto *recalculate = actionButton(localizedText("按新需求重算", "Recalculate"), {}, true);
    connect(recalculate, &QPushButton::clicked, this, &ResultsPage::retryRequested);
    staleLayout->addWidget(recalculate);
    layout->addWidget(m_staleBanner);
    m_staleBanner->hide();

    auto *tableActions = new QHBoxLayout;
    m_countLabel = new QLabel;
    m_countLabel->setWordWrap(true);
    tableActions->addWidget(m_countLabel, 1);
    m_compareButton = actionButton(localizedText("前三方案", "Top candidates"), {}, true);
    m_compareButton->setObjectName("ResultsCompareToggle");
    m_compareButton->setCheckable(true);
    tableActions->addWidget(m_compareButton);
    m_detailsButton = actionButton(localizedText("校核详情", "Check details"), {}, true);
    m_detailsButton->setObjectName("ResultsDetailsToggle");
    m_detailsButton->setCheckable(true);
    tableActions->addWidget(m_detailsButton);
    layout->addLayout(tableActions);

    QFrame *cards = new QFrame;
    m_cards = cards;
    cards->setObjectName(QStringLiteral("SectionCard"));
    m_cardsLayout = new QHBoxLayout(cards);
    m_cardsLayout->setContentsMargins(12, 12, 12, 12);
    m_cardsLayout->setSpacing(12);
    layout->addWidget(cards);
    cards->hide();
    connect(m_compareButton, &QPushButton::toggled, cards, &QWidget::setVisible);

    m_table = new QTableWidget;
    m_table->setObjectName(QStringLiteral("results/table"));
    m_table->setProperty("headerStateKey", "results/table-v2");
    m_table->setAccessibleName(localizedText("推荐方案表", "Recommended plans table"));
    setupTable(m_table);
    m_table->setColumnCount(11);
    m_table->setHorizontalHeaderLabels({
        localizedText("类型", "Type"), localizedText("状态", "Status"), localizedText("相对分", "Score"),
        localizedText("相机", "Camera"), localizedText("镜头", "Lens"), localizedText("光源", "Light"),
        QStringLiteral("FOV(mm)"), localizedText("物方像素", "Obj Pixel"),
        localizedText("倍率/焦距", "Mag/Focal"), QStringLiteral("WD/DOF"),
        localizedText("风险", "Risk")
    });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    const int resultColumnWidths[] = {60, 100, 72, 166, 172, 116, 108, 88, 96, 120};
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
    m_details->setMaximumHeight(260);

    m_splitter = new QSplitter(Qt::Vertical, this);
    m_splitter->setObjectName(QStringLiteral("results/browse-v2"));
    m_splitter->addWidget(m_table);
    m_splitter->addWidget(m_details);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({430, 180});
    m_details->hide();
    connect(m_detailsButton, &QPushButton::toggled, this, [this](bool visible) {
        m_details->setVisible(visible);
        if (visible) { m_splitter->setSizes({480, 210}); refreshDetails(m_selectedSourceIndex); }
    });
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this]() { m_detailsButton->setChecked(true); });
    UiSettings::instance().restoreHeader(QStringLiteral("results/table-v2"), m_table->horizontalHeader());
    // 工程参数保留在校核详情中，默认把横向空间留给型号、成像指标和风险。
    m_table->setColumnHidden(8, true);
    m_table->setColumnHidden(9, true);
    layout->addWidget(m_splitter, 1);
    m_selectionSummary = new QLabel;
    m_selectionSummary->setObjectName("ResultsSelectionSummary");
    m_selectionSummary->setTextFormat(Qt::PlainText);
    m_selectionSummary->setWordWrap(true);
    m_selectionSummary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_selectionSummary);
}

ResultsPage::~ResultsPage()
{
    UiSettings::instance().saveHeader(QStringLiteral("results/table-v2"), m_table ? m_table->horizontalHeader() : nullptr);
}

void ResultsPage::setBusy(const SelectionRequest &request)
{
    Q_UNUSED(request)
    m_results.clear();
    m_presentations.clear();
    m_selectedSourceIndex = -1;
    setRequestOutdated(false);
    m_cards->show();
    m_countLabel->clear();
    m_selectionSummary->clear();
    m_detailsButton->setEnabled(false);
    m_compareButton->setEnabled(false);

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
    setRequestOutdated(false);
    m_cards->show();
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
    m_results.clear();
    for (const auto &result : results) m_results.append(localizedResult(result));
    m_resultRequest = request;
    m_presentations = buildResultPresentations(results);
    setRequestOutdated(false);
    m_detailsButton->setEnabled(!results.isEmpty());
    m_compareButton->setEnabled(!results.isEmpty());
    m_cards->setVisible(m_compareButton->isChecked() || results.isEmpty());
    int passed = 0, unknown = 0, failed = 0;
    for (const auto &result : results) {
        if (!result.hardConstraintsPassed) ++failed;
        else if (result.checks.unknown()) ++unknown;
        else ++passed;
    }
    m_countLabel->setText(localizedText("初筛通过 %1 · 待确认 %2 · 不满足 %3", "Passed %1 · Pending %2 · Failed %3")
        .arg(passed).arg(unknown).arg(failed));
    refreshTable(request);
}

void ResultsPage::setRequestOutdated(bool outdated)
{
    m_staleBanner->setVisible(outdated);
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
        const QString state = !presentation.compatible ? QStringLiteral("error")
            : presentation.needsConfirmation ? QStringLiteral("warning") : QStringLiteral("success");
        setWidgetState(card, state);
        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(14, 12, 14, 12);
        cardLayout->setSpacing(7);

        QHBoxLayout *top = new QHBoxLayout;
        QLabel *rank = new QLabel(QStringLiteral("#%1  %2").arg(i + 1).arg(lensCategory(r)));
        rank->setObjectName(QStringLiteral("MetricLabel"));
        top->addWidget(rank, 1);
        top->addWidget(statusBadge(compatibilityText(r), state));
        cardLayout->addLayout(top);

        const QString matchText = !presentation.compatible
            ? localizedText("不兼容", "Incompatible")
            : (presentation.matchAvailable
                ? QStringLiteral("%1%").arg(presentation.relativeMatchPercent)
                : QStringLiteral("—"));
        QLabel *score = new QLabel(matchText);
        score->setObjectName(QStringLiteral("MetricDetail"));
        score->setWordWrap(true);
        score->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        score->setToolTip(localizedText("算法原始分：%1；相对匹配度仅用于本批候选比较。",
                                        "Raw algorithm score: %1. Relative match is only comparable within this batch.")
                              .arg(r.score.score, 0, 'f', 1));
        cardLayout->addWidget(score);
        QLabel *scoreLabel = new QLabel(presentation.matchAvailable
            ? localizedText("相对匹配度 · 本批候选", "Relative match · current candidates")
            : localizedText("暂无有效匹配度", "No valid match score"));
        scoreLabel->setObjectName(QStringLiteral("MetricLabel"));
        scoreLabel->setWordWrap(true);
        cardLayout->addWidget(scoreLabel);

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
    if (!m_results.isEmpty() && m_results.first().catalogCameras > 0) {
        const auto &r = m_results.first();
        m_summaryLabel->setText(m_summaryLabel->text() + localizedText("\n已比较相机 %1/%2、镜头 %3/%4；相对分仅比较本批候选。", "\nCompared cameras %1/%2, lenses %3/%4; scores compare this batch only.")
            .arg(r.searchedCameras).arg(r.catalogCameras).arg(r.searchedLenses).arg(r.catalogLenses));
    }

    const QSignalBlocker blocker(m_table);
    m_table->setSortingEnabled(false);
    m_table->setRowCount(m_results.size());
    for (int row = 0; row < m_results.size(); ++row) {
        const SelectionResult &r = m_results.at(row);
        const ResultPresentation presentation = m_presentations.value(row);
        m_table->setItem(row, 0, indexedItem(lensCategory(r), row));
        m_table->setItem(row, 1, numericItem(compatibilityText(r), !r.hardConstraintsPassed ? 2.0 : r.checks.unknown() ? 1.0 : 0.0));
        decorateCandidateStatus(m_table->item(row, 1), r.checks, r.hardConstraintsPassed);
        const QString matchText = !presentation.compatible
            ? localizedText("不兼容", "Incompatible")
            : (presentation.matchAvailable
                ? QStringLiteral("%1%").arg(presentation.relativeMatchPercent)
                : QStringLiteral("—"));
        QTableWidgetItem *matchItem = numericItem(matchText, presentation.matchAvailable
            ? std::optional<double>(presentation.relativeMatchPercent) : std::nullopt);
        matchItem->setToolTip(localizedText("算法原始分：%1", "Raw algorithm score: %1").arg(r.score.score, 0, 'f', 1));
        m_table->setItem(row, 2, matchItem);
        m_table->setItem(row, 3, item(productLabel(r.camera.manufacturer, r.camera.model)));
        m_table->setItem(row, 4, item(productLabel(r.lens.manufacturer, r.lens.model)));
        m_table->setItem(row, 5, item(productLabel(r.light.manufacturer, r.light.model)));
        m_table->setItem(row, 6, numericItem(QStringLiteral("%1 x %2")
            .arg(r.effectiveFovWidthMm, 0, 'f', 1)
            .arg(r.effectiveFovHeightMm, 0, 'f', 1), r.effectiveFovWidthMm));
        m_table->setItem(row, 7, numericItem(QStringLiteral("%1 um").arg(r.objectPixelSizeUm, 0, 'f', 2), r.objectPixelSizeUm));
        m_table->setItem(row, 8, numericItem(r.isTelecentric()
            ? QStringLiteral("%1x").arg(r.magnification, 0, 'f', 3)
            : QStringLiteral("%1 mm").arg(r.lens.focalLengthMm, 0, 'f', 1), r.isTelecentric() ? r.magnification : 1000000.0 + r.lens.focalLengthMm));
        const double wd = r.isTelecentric() ? r.lens.nominalWorkingDistanceMm : r.lens.minWorkingDistanceMm;
        const QString wdText = wd > 0.0 ? number(wd, 0) : localizedText("未知", "Unknown");
        const QString dofText = r.estimatedDofMm > 0.0 ? number(r.estimatedDofMm, 2) : localizedText("未知", "Unknown");
        m_table->setItem(row, 9, numericItem(QStringLiteral("%1 %2 / DOF %3")
            .arg(r.isTelecentric() ? QStringLiteral("WD") : QStringLiteral("min WD"), wdText, dofText),
            wd > 0.0 ? std::optional(wd) : std::nullopt));
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
        m_selectionSummary->clear();
        m_selectedSourceIndex = -1;
    }
}

void ResultsPage::refreshDetails(int row)
{
    if (!m_details || row < 0 || row >= m_results.size())
        return;

    const SelectionResult &r = m_results.at(row);
    m_selectedSourceIndex = row;
    const ResultPresentation presentation = m_presentations.value(row);
    const QStringList issues = candidateCheckMessages(r.checks, CandidateCheckState::Failed)
        + candidateCheckMessages(r.checks, CandidateCheckState::Unknown);
    m_selectionSummary->setText(compatibilityText(r) + QStringLiteral("  |  ")
        + productLabel(r.camera.manufacturer, r.camera.model) + QStringLiteral(" + ")
        + productLabel(r.lens.manufacturer, r.lens.model) + QStringLiteral("\n")
        + (issues.isEmpty() ? (presentation.riskItems.isEmpty() ? localizedText("逐项初筛通过，可展开工程详情。", "Screening passed. Open details for engineering checks.")
             : presentation.riskItems.first()) : issues.join(QStringLiteral("；"))));
    if (m_details->isHidden()) return;
    QString text;
    text += QStringLiteral("<h3>") + htmlText(r.schemeTitle) + localizedText("：", ": ")
        + htmlText(productLabel(r.camera.manufacturer, r.camera.model)) + QStringLiteral(" + ")
        + htmlText(productLabel(r.lens.manufacturer, r.lens.model)) + QStringLiteral(" + ")
        + htmlText(productLabel(r.light.manufacturer, r.light.model)) + QStringLiteral("</h3>");
    text += localizedText("<p><b>适配状态：</b>%1</p>", "<p><b>Compatibility:</b> %1</p>").arg(htmlText(compatibilityText(r)));
    const double catalogWd = r.isTelecentric() ? r.lens.nominalWorkingDistanceMm : r.lens.minWorkingDistanceMm;
    text += localizedText("<p><b>安装与成像：</b>本次 WD %1 mm；目录 %2 %3；高度波动 %4 mm；DOF %5；%6。</p>",
        "<p><b>Mounting and imaging:</b> requested WD %1 mm; catalog %2 %3; height variation %4 mm; DOF %5; %6.</p>")
        .arg(m_resultRequest.workingDistanceMm, 0, 'f', 1)
        .arg(r.isTelecentric() ? localizedText("标称 WD", "nominal WD") : localizedText("最小 WD", "minimum WD"))
        .arg(catalogWd > 0.0 ? QStringLiteral("%1 mm").arg(catalogWd, 0, 'f', 1) : localizedText("未知", "unknown"))
        .arg(m_resultRequest.heightVariationMm, 0, 'f', 2)
        .arg(r.estimatedDofMm > 0.0 ? QStringLiteral("%1 mm").arg(r.estimatedDofMm, 0, 'f', 2) : localizedText("未知", "unknown"))
        .arg(r.isTelecentric() ? QStringLiteral("PMAG %1x").arg(r.magnification, 0, 'f', 3)
                              : localizedText("焦距 %1 mm", "focal length %1 mm").arg(r.lens.focalLengthMm, 0, 'f', 1));
    text += candidateChecksHtml(r.checks);
    text += localizedText("<p><b>风险提示：</b>%1</p>", "<p><b>Risks:</b> %1</p>")
        .arg(htmlList(presentation.riskItems, localizedText("；", "; ")));
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
    text += localizedText("<p><b>接口/存储：</b>单帧 %1 MB；接口容量 %2 MB/s；带宽利用率 %3%；原始存储约 %4 GB/h；镜头 MP 利用率 %5%。</p>",
                          "<p><b>Interface / storage:</b> frame %1 MB; interface capacity %2 MB/s; bandwidth utilization %3%; raw storage about %4 GB/h; lens MP utilization %5%.</p>")
        .arg(r.framePayloadMB, 0, 'f', 2)
        .arg(r.interfaceCapacityMBps, 0, 'f', 1)
        .arg(r.bandwidthUtilizationPercent, 0, 'f', 0)
        .arg(r.storagePerHourGB, 0, 'f', 0)
        .arg(r.lensMegapixelUtilizationPercent, 0, 'f', 0);
    text += localizedText("<p><b>接口剩余量：</b>%1（容量减去估算占用；负值表示不足）。</p>",
                          "<p><b>Interface headroom:</b> %1 (capacity minus estimated use; negative means insufficient).</p>")
        .arg(r.interfaceCapacityMBps > 0.0 ? QStringLiteral("%1 MB/s").arg(r.interfaceCapacityMBps - r.bandwidthRequiredMBps, 0, 'f', 1)
                                          : localizedText("待确认", "Needs confirmation"));
    text += localizedText("<p><b>公式：</b>%1</p>", "<p><b>Formula:</b> %1</p>").arg(htmlText(r.formulaSummary));
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
