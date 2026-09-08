#include "ui/pages/PureCalculationPage.h"

#include "ui/FovDiagram.h"
#include "ui/ParameterNumberField.h"
#include "ui/ParameterUi.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTabBar>
#include <QTableWidget>
#include <QTextBrowser>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>

using namespace UiHelpers;
using namespace ParameterUi;

namespace {
class TaskStack : public QStackedWidget
{
public:
    QSize sizeHint() const override { return currentWidget() ? currentWidget()->sizeHint() : QSize(0, 0); }
    QSize minimumSizeHint() const override { return QSize(0, 0); }
};
}

PureCalculationPage::PureCalculationPage(QWidget *parent) : QWidget(parent)
{
    m_loading = true;
    setObjectName(QStringLiteral("ParameterWorkbench"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 8, 16, 0);
    root->setSpacing(12);
    auto *actions = new QWidget;
    auto *actionLayout = new QHBoxLayout(actions);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    auto *example = actionButton(localizedText("载入示例", "Load example"), QString(), true);
    example->setObjectName(QStringLiteral("WorkbenchExample"));
    auto *more = new QToolButton;
    more->setText(localizedText("更多", "More"));
    more->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(more);
    menu->addAction(localizedText("清空当前任务", "Clear current task"), this, &PureCalculationPage::clearCurrentTask);
    menu->addAction(localizedText("复制参数与结果", "Copy inputs and results"), this, &PureCalculationPage::copyReport);
    more->setMenu(menu);
    actionLayout->addWidget(example);
    actionLayout->addWidget(more);
    root->addWidget(pageHeader(localizedText("视觉参数工作台", "Vision parameter workbench"),
        localizedText("选择要解决的问题，填写已知量，逐项核对结果。", "Choose a task, enter known values, and inspect the results."), actions));
    connect(example, &QPushButton::clicked, this, &PureCalculationPage::resetDefaults);

    m_tasks = new QTabBar;
    m_tasks->setObjectName(QStringLiteral("WorkbenchTasks"));
    m_tasks->setAccessibleName(localizedText("计算任务", "Calculation task"));
    m_tasks->setExpanding(false);
    m_tasks->setUsesScrollButtons(true);
    m_tasks->setElideMode(Qt::ElideNone);
    for (const QString &label : taskLabels()) m_tasks->addTab(label);
    for (auto *button : m_tasks->findChildren<QToolButton *>()) {
        const bool left = button->arrowType() == Qt::LeftArrow;
        if (!left && button->arrowType() != Qt::RightArrow) continue;
        button->setArrowType(Qt::NoArrow);
        button->setText(left ? QStringLiteral("‹") : QStringLiteral("›"));
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setAccessibleName(left ? localizedText("前面的任务", "Earlier tasks") : localizedText("后面的任务", "Later tasks"));
    }
    root->addWidget(m_tasks);

    m_cameraPanel = new QFrame;
    m_cameraPanel->setObjectName(QStringLiteral("ParameterGroup"));
    auto *cameraLayout = new QVBoxLayout(m_cameraPanel);
    auto *cameraHeading = new QHBoxLayout;
    m_cameraSource = new QLabel;
    m_cameraSource->setTextFormat(Qt::PlainText);
    m_cameraSource->setWordWrap(true);
    m_cameraSource->setObjectName(QStringLiteral("WorkbenchCameraSource"));
    m_importCamera = actionButton(localizedText("从产品库选相机", "Select camera"), QString(), true);
    m_importCamera->setObjectName(QStringLiteral("WorkbenchImportCamera"));
    cameraHeading->addWidget(m_cameraSource, 1);
    cameraHeading->addWidget(m_importCamera);
    cameraLayout->addLayout(cameraHeading);
    auto *cameraGrid = new QGridLayout;
    cameraGrid->setHorizontalSpacing(14);
    addNumber(cameraGrid, "camera.nx", localizedText("有效分辨率 X", "Effective resolution X"), "px", 0, 0, true);
    addNumber(cameraGrid, "camera.ny", localizedText("有效分辨率 Y", "Effective resolution Y"), "px", 0, 1, true);
    addNumber(cameraGrid, "camera.pixel", localizedText("像元尺寸", "Pixel pitch"), "μm", 0, 2);
    cameraLayout->addLayout(cameraGrid);
    root->addWidget(m_cameraPanel);

    m_scroll = new QScrollArea;
    m_scroll->setObjectName(QStringLiteral("WorkbenchScroll"));
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget;
    content->setObjectName(QStringLiteral("ParameterInputPanel"));
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 6, 0);
    m_columns = new QBoxLayout(QBoxLayout::LeftToRight);
    m_columns->setSpacing(16);
    auto *inputFrame = new QFrame;
    inputFrame->setObjectName(QStringLiteral("ParameterGroup"));
    inputFrame->setMinimumWidth(0);
    auto *inputLayout = new QVBoxLayout(inputFrame);
    inputLayout->setSpacing(12);
    m_taskSource = new QLabel;
    m_taskSource->setTextFormat(Qt::PlainText);
    m_taskSource->setWordWrap(true);
    inputLayout->addWidget(m_taskSource);
    auto *lensActions = new QHBoxLayout;
    m_importLens = actionButton(localizedText("选用已有镜头", "Select existing lens"), QString(), true);
    m_matchLens = actionButton(localizedText("按当前条件找镜头", "Find matching lenses"), QString(), true);
    m_importLens->setObjectName(QStringLiteral("WorkbenchImportLens"));
    m_matchLens->setObjectName(QStringLiteral("WorkbenchMatchLens"));
    lensActions->addWidget(m_importLens);
    lensActions->addWidget(m_matchLens);
    inputLayout->addLayout(lensActions);
    m_inputs = new TaskStack;
    m_inputs->setObjectName(QStringLiteral("ParameterInputPanel"));
    m_inputs->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    buildPanels();
    inputLayout->addWidget(m_inputs);
    inputLayout->addStretch();

    auto *outputFrame = new QFrame;
    outputFrame->setObjectName(QStringLiteral("ParameterGroup"));
    outputFrame->setMinimumWidth(0);
    auto *outputLayout = new QVBoxLayout(outputFrame);
    outputLayout->setSpacing(12);
    m_resultStatus = new QLabel;
    m_resultStatus->setObjectName(QStringLiteral("WorkbenchResultStatus"));
    m_resultStatus->setWordWrap(true);
    outputLayout->addWidget(m_resultStatus);
    auto *metrics = new QGridLayout;
    for (int i = 0; i < 3; ++i) {
        auto *card = new QFrame;
        card->setObjectName(QStringLiteral("WorkbenchMetric"));
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(10, 10, 10, 10);
        m_metricLabels[i] = new QLabel;
        m_metricLabels[i]->setWordWrap(true);
        m_metricValues[i] = new QLabel(QStringLiteral("—"));
        m_metricValues[i]->setTextFormat(Qt::PlainText);
        m_metricValues[i]->setObjectName(QStringLiteral("WorkbenchMetricValue"));
        m_metricValues[i]->setWordWrap(true);
        m_metricValues[i]->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(m_metricLabels[i]);
        layout->addWidget(m_metricValues[i]);
        metrics->addWidget(card, 0, i);
        metrics->setColumnStretch(i, 1);
    }
    outputLayout->addLayout(metrics);
    m_diagram = new FovDiagram;
    outputLayout->addWidget(m_diagram);
    m_results = new QTableWidget;
    m_results->setObjectName(QStringLiteral("WorkbenchResults"));
    m_results->setAccessibleName(localizedText("计算结果与逐项校核", "Calculation results and checks"));
    setupTable(m_results);
    m_results->setWordWrap(true);
    m_results->setMinimumWidth(0);
    m_results->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_results->verticalHeader()->hide();
    outputLayout->addWidget(m_results);
    m_resultNote = new QLabel;
    m_resultNote->setObjectName(QStringLiteral("WorkbenchModelNote"));
    m_resultNote->setWordWrap(true);
    m_resultNote->setTextInteractionFlags(Qt::TextSelectableByMouse);
    outputLayout->addWidget(m_resultNote);
    auto *resultActions = new QHBoxLayout;
    m_applyCheck = actionButton(localizedText("用于方案校核", "Use in system check"), QString(), true);
    m_applyCheck->setObjectName(QStringLiteral("WorkbenchApplyCheck"));
    auto *copy = actionButton(localizedText("复制", "Copy"), QString(), true);
    auto *saveA = actionButton(localizedText("保存 A", "Save A"), QString(), true);
    auto *saveB = actionButton(localizedText("保存 B", "Save B"), QString(), true);
    saveA->setObjectName(QStringLiteral("WorkbenchSaveA"));
    saveB->setObjectName(QStringLiteral("WorkbenchSaveB"));
    resultActions->addWidget(m_applyCheck);
    resultActions->addStretch();
    resultActions->addWidget(copy);
    resultActions->addWidget(saveA);
    resultActions->addWidget(saveB);
    outputLayout->addLayout(resultActions);
    outputLayout->addStretch();
    m_columns->addWidget(inputFrame, 4);
    m_columns->addWidget(outputFrame, 6);
    contentLayout->addLayout(m_columns);
    m_compareToggle = new QCheckBox(localizedText("展开 A/B 独立快照对照", "Show independent A/B snapshots"));
    m_compareToggle->setObjectName(QStringLiteral("WorkbenchCompareToggle"));
    contentLayout->addWidget(m_compareToggle);
    m_compare = new QTextBrowser;
    m_compare->setObjectName(QStringLiteral("WorkbenchComparison"));
    m_compare->setMinimumHeight(300);
    m_compare->setVisible(false);
    contentLayout->addWidget(m_compare);
    contentLayout->addStretch();
    m_scroll->setWidget(content);
    root->addWidget(m_scroll, 1);

    connect(m_tasks, &QTabBar::currentChanged, this, [this](int index) {
        m_inputs->setCurrentIndex(index);
        m_scroll->verticalScrollBar()->setValue(0);
        refresh();
    });
    connect(m_importCamera, &QPushButton::clicked, this, &PureCalculationPage::chooseCamera);
    connect(m_importLens, &QPushButton::clicked, this, [this]() { chooseLens(false); });
    connect(m_matchLens, &QPushButton::clicked, this, [this]() { chooseLens(true); });
    connect(m_applyCheck, &QPushButton::clicked, this, &PureCalculationPage::applyToCheck);
    connect(copy, &QPushButton::clicked, this, &PureCalculationPage::copyReport);
    connect(saveA, &QPushButton::clicked, this, [this]() { saveSnapshot(0); });
    connect(saveB, &QPushButton::clicked, this, [this]() { saveSnapshot(1); });
    connect(m_compareToggle, &QCheckBox::toggled, m_compare, &QWidget::setVisible);
    connect(m_results, &QTableWidget::itemSelectionChanged, this, [this]() {
        if (m_comparisonKind != QLatin1String("check")) return;
        const int row = m_results->currentRow();
        if (row >= 0 && m_results->item(row, 0))
            m_resultNote->setText(m_results->item(row, 0)->data(Qt::UserRole).toString());
    });
    connect(m_results, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (row < 0 || row >= m_comparisonValues.size()) return;
        m_loading = true;
        if (m_comparisonKind == QLatin1String("focal")) {
            const auto previous = Parameters::optics(opticsInput());
            setNumber("optics.distance", previous.distanceMm);
            setNumber("optics.focal", m_comparisonValues.at(row));
            setChoice("optics.solve", "fov");
        } else if (m_comparisonKind == QLatin1String("mag")) {
            setNumber("tele.mag", m_comparisonValues.at(row));
            setChoice("tele.solve", "fov");
        }
        setSource(task(), "manual");
        m_loading = false;
        refresh();
    });
    m_loading = false;
    setCatalog(nullptr);
    refresh();
}

void PureCalculationPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_columns->setDirection(width() < 1000 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    m_results->resizeRowsToContents();
    QTimer::singleShot(0, m_tasks, [this]() {
        // 滚动按钮可能直到首次溢出时才创建，不能只在构造期间设置。
        for (auto *button : m_tasks->findChildren<QToolButton *>()) {
            const bool left = button->arrowType() == Qt::LeftArrow;
            if (!left && button->arrowType() != Qt::RightArrow) continue;
            button->setArrowType(Qt::NoArrow);
            button->setToolButtonStyle(Qt::ToolButtonTextOnly);
            button->setText(left ? QStringLiteral("‹") : QStringLiteral("›"));
            button->setAccessibleName(left ? localizedText("前面的任务", "Earlier tasks") : localizedText("后面的任务", "Later tasks"));
        }
    });
}
QString PureCalculationPage::task() const { return taskKeys.value(m_tasks->currentIndex(), taskKeys.first()); }
void PureCalculationPage::setTask(const QString &task)
{
    const int index = taskKeys.indexOf(task);
    if (index >= 0) m_tasks->setCurrentIndex(index);
}
void PureCalculationPage::fieldChanged(const QString &key)
{
    if (m_loading) return;
    const QString scope = key.section(QLatin1Char('.'), 0, 0);
    if (scope == QLatin1String("camera") && cameraSignature() != m_lastCameraSignature)
        invalidateMeasuredFov();
    auto source = m_provenance.value(scope).toObject();
    if ((key == QLatin1String("check.actualWidth") || key == QLatin1String("check.actualHeight"))
        && usable(number("check.actualWidth")) && usable(number("check.actualHeight")))
        source.remove("measurementStale");
    if (!source.value("type").toString().isEmpty()) {
        source.insert("edited", true);
        m_provenance.insert(scope, source);
    } else setSource(scope, "manual");
    refresh();
}
QString PureCalculationPage::cameraSignature() const
{
    return valueText(number("camera.nx")) + QLatin1Char('/') + valueText(number("camera.ny"))
        + QLatin1Char('/') + valueText(number("camera.pixel"));
}
void PureCalculationPage::invalidateMeasuredFov()
{
    if (choice("check.geometry") != QLatin1String("measured")) return;
    const bool wasLoading = m_loading;
    m_loading = true;
    setNumber("check.actualWidth", {}); setNumber("check.actualHeight", {});
    auto source = m_provenance.value("check").toObject();
    source.insert("measurementStale", true);
    m_provenance.insert("check", source);
    m_loading = wasLoading;
}
void PureCalculationPage::clearLensSpecifications()
{
    for (const auto &key : {"check.imageCircle", "check.minDistance", "check.nominalDistance", "check.distanceTolerance", "check.dof", "check.telecentricity"})
        setNumber(key, {});
    m_texts.value("check.lensMount")->clear();
    m_flags.value("check.dofConfirmed")->setChecked(false);
}
void PureCalculationPage::setSource(const QString &scope, const QString &type, const QString &label)
{
    m_provenance.insert(scope, QJsonObject{{"type", type}, {"label", label}, {"edited", false}});
}
QString PureCalculationPage::sourceText(const QString &scope) const
{
    const auto source = m_provenance.value(scope).toObject();
    const auto type = source.value("type").toString();
    QString text;
    if (type == QLatin1String("catalog")) text = localizedText("产品库：", "Catalog: ") + source.value("label").toString();
    else if (type == QLatin1String("example")) text = localizedText("示例参数 · 尚未选择实物", "Example values · no hardware selected");
    else if (type == QLatin1String("derived")) {
        const QString origin = source.value("label").toString();
        text = localizedText("来自计算任务：", "From calculation: ") + taskLabels().value(taskKeys.indexOf(origin), origin);
    }
    else if (type == QLatin1String("manual")) text = localizedText("手动参数", "Manual parameters");
    else text = localizedText("尚未填写参数", "No parameters entered");
    if (source.value("edited").toBool()) text += localizedText(" · 已手动修改", " · manually edited");
    if (source.value("measurementStale").toBool())
        text += localizedText(" · 相机已变化，请重新填写实测视场", " · camera changed; re-enter measured FOV");
    return text;
}
ParameterWorkspaceState PureCalculationPage::workspaceState() const
{
    ParameterWorkspaceState state;
    state.task = task();
    for (auto it = m_fields.cbegin(); it != m_fields.cend(); ++it) state.fields.insert(it.key(), it.value()->state());
    for (auto it = m_choices.cbegin(); it != m_choices.cend(); ++it) state.choices.insert(it.key(), it.value()->currentData().toString());
    for (auto it = m_flags.cbegin(); it != m_flags.cend(); ++it) state.flags.insert(it.key(), it.value()->isChecked());
    for (auto it = m_texts.cbegin(); it != m_texts.cend(); ++it) state.texts.insert(it.key(), it.value()->text());
    state.texts.insert("camera.mount", m_cameraMount);
    state.flags.insert("compare.expanded", m_compareToggle->isChecked());
    state.provenance = m_provenance;
    state.snapshots = m_snapshots;
    return state;
}
void PureCalculationPage::restoreWorkspaceState(const ParameterWorkspaceState &state)
{
    m_loading = true;
    for (auto it = m_fields.begin(); it != m_fields.end(); ++it) it.value()->restoreState(state.fields.value(it.key()).toObject());
    for (auto it = m_choices.begin(); it != m_choices.end(); ++it) setChoice(it.key(), state.choices.value(it.key()).toString());
    for (auto it = m_flags.begin(); it != m_flags.end(); ++it) it.value()->setChecked(state.flags.value(it.key()).toBool());
    for (auto it = m_texts.begin(); it != m_texts.end(); ++it) it.value()->setText(state.texts.value(it.key()).toString());
    m_cameraMount = state.texts.value("camera.mount").toString();
    m_provenance = state.provenance;
    m_snapshots = state.snapshots;
    setTask(state.task);
    m_compareToggle->setChecked(state.flags.value("compare.expanded").toBool());
    m_loading = false;
    refresh();
    updateSnapshots();
}
void PureCalculationPage::clearCurrentTask()
{
    m_loading = true;
    const QString prefix = task() + QLatin1Char('.');
    for (auto it = m_fields.begin(); it != m_fields.end(); ++it) if (it.key().startsWith(prefix)) it.value()->setValue({});
    for (auto it = m_choices.begin(); it != m_choices.end(); ++it) if (it.key().startsWith(prefix)) it.value()->setCurrentIndex(0);
    for (auto it = m_flags.begin(); it != m_flags.end(); ++it) if (it.key().startsWith(prefix)) it.value()->setChecked(false);
    for (auto it = m_texts.begin(); it != m_texts.end(); ++it) if (it.key().startsWith(prefix)) it.value()->clear();
    m_provenance.remove(task());
    m_loading = false;
    refresh();
}
void PureCalculationPage::copyReport() { QApplication::clipboard()->setText(m_report); }
void PureCalculationPage::saveSnapshot(int slot)
{
    if (slot < 0 || slot > 1) return;
    auto state = workspaceState();
    state.snapshots = {};
    const QJsonObject snapshot{{"time", QDateTime::currentDateTime().toString(Qt::ISODate)},
                               {"task", task()}, {"state", state.toJson()}, {"report", m_report}};
    while (m_snapshots.size() < 2) m_snapshots.append(QJsonObject());
    m_snapshots.replace(slot, snapshot);
    updateSnapshots();
    m_compareToggle->setChecked(true);
}
void PureCalculationPage::updateSnapshots()
{
    QString html = QStringLiteral("<table width='100%' cellpadding='8'><tr><th>A</th><th>B</th></tr><tr>");
    for (int i = 0; i < 2; ++i) {
        const auto snapshot = i < m_snapshots.size() ? m_snapshots.at(i).toObject() : QJsonObject();
        const QString report = snapshot.isEmpty() ? localizedText("尚未保存", "Not saved")
            : snapshot.value("time").toString() + QLatin1Char('\n') + snapshot.value("report").toString();
        html += QStringLiteral("<td width='50%' valign='top'>%1</td>").arg(report.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>")));
    }
    m_compare->setHtml(html + QStringLiteral("</tr></table>"));
}
