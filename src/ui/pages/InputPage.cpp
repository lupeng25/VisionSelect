#include "ui/pages/InputPage.h"

#include "selection/CalculationAssistant.h"
#include "ui/UiHelpers.h"
#include "ui/UiSettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSplitter>
#include <QStringList>
#include <QTextEdit>
#include <QVBoxLayout>

using namespace UiHelpers;

namespace {
// 视场示意与输入保持同步，帮助理解工件、余量和有效成像范围。
class FieldOfViewPreview : public QWidget
{
public:
    explicit FieldOfViewPreview(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(148);
        setAccessibleName(localizedText("工件与视场示意", "Part and field of view preview"));
    }

    void setRequirement(const SelectionRequest &request)
    {
        m_request = request;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QColor accent = palette().highlight().color();
        const QRectF area = QRectF(rect()).adjusted(18, 16, -18, -30);
        const double width = m_request.objectWidthMm + 2 * m_request.placementMarginMm;
        const double height = m_request.objectHeightMm + 2 * m_request.placementMarginMm;
        const double scale = qMin(area.width() / qMax(1.0, width), area.height() / qMax(1.0, height));
        const QSizeF size(width * scale, height * scale);
        const QRectF fov(area.center() - QPointF(size.width() / 2, size.height() / 2), size);
        painter.setPen(QPen(accent, 1.2, Qt::DashLine));
        QColor wash(accent);
        wash.setAlpha(12);
        painter.setBrush(wash);
        painter.drawRoundedRect(fov, 5, 5);
        const double inset = m_request.placementMarginMm * scale;
        const QRectF part = fov.adjusted(inset, inset, -inset, -inset);
        wash.setAlpha(40);
        painter.setBrush(wash);
        painter.setPen(QPen(accent, 1.5));
        painter.drawRoundedRect(part, 3, 3);
        const QPointF center = part.center();
        painter.drawLine(center - QPointF(6, 0), center + QPointF(6, 0));
        painter.drawLine(center - QPointF(0, 6), center + QPointF(0, 6));
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(QRectF(0, rect().height() - 25, rect().width(), 20), Qt::AlignCenter,
            localizedText("实线 工件  /  虚线 视场", "Solid: part  /  Dashed: FOV"));
    }

private:
    SelectionRequest m_request;
};

QLabel *fieldLabel(const QString &text, QWidget *buddy = nullptr)
{
    QLabel *label = new QLabel(text);
    label->setObjectName(QStringLiteral("EditorFieldLabel"));
    label->setMinimumWidth(96);
    label->setWordWrap(true);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    if (buddy)
        label->setBuddy(buddy);
    return label;
}

QFrame *editorGroup(const QString &number, const QString &title, QGridLayout **grid)
{
    QFrame *group = new QFrame;
    group->setObjectName(QStringLiteral("EditorGroup"));
    QVBoxLayout *layout = new QVBoxLayout(group);
    layout->setContentsMargins(20, 18, 20, 20);
    layout->setSpacing(16);
    QHBoxLayout *headingLayout = new QHBoxLayout;
    QLabel *badge = new QLabel(number);
    badge->setObjectName(QStringLiteral("SectionNumber"));
    badge->setAlignment(Qt::AlignCenter);
    badge->setFixedSize(28, 28);
    headingLayout->addWidget(badge);
    QLabel *heading = new QLabel(title);
    heading->setObjectName(QStringLiteral("EditorGroupTitle"));
    headingLayout->addWidget(heading, 1);
    layout->addLayout(headingLayout);
    *grid = new QGridLayout;
    (*grid)->setContentsMargins(0, 0, 0, 0);
    (*grid)->setHorizontalSpacing(12);
    (*grid)->setVerticalSpacing(9);
    (*grid)->setColumnStretch(1, 1);
    (*grid)->setColumnStretch(3, 1);
    layout->addLayout(*grid);
    return group;
}

void addGridField(QGridLayout *grid, int row, int pair, const QString &label, QWidget *control)
{
    const int column = pair * 2;
    QWidget *field = new QWidget;
    field->setObjectName(QStringLiteral("ParameterField"));
    QVBoxLayout *fieldLayout = new QVBoxLayout(field);
    fieldLayout->setContentsMargins(0, 0, 0, 0);
    fieldLayout->setSpacing(7);
    fieldLayout->addWidget(fieldLabel(label, control));
    control->setAccessibleName(label);
    control->setMinimumWidth(0);
    if (QComboBox *combo = qobject_cast<QComboBox *>(control)) {
        combo->setMinimumContentsLength(0);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    }
    control->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    fieldLayout->addWidget(control);
    grid->addWidget(field, row, column, 1, 2);
}

bool requestsEqual(const SelectionRequest &left, const SelectionRequest &right)
{
    return left.projectNotes == right.projectNotes
        && left.objectWidthMm == right.objectWidthMm
        && left.objectHeightMm == right.objectHeightMm
        && left.placementMarginMm == right.placementMarginMm
        && left.minFeatureUm == right.minFeatureUm
        && left.measurementToleranceUm == right.measurementToleranceUm
        && left.workingDistanceMm == right.workingDistanceMm
        && left.heightVariationMm == right.heightVariationMm
        && left.motionMode == right.motionMode
        && left.motionSpeedMmS == right.motionSpeedMmS
        && left.requiredFps == right.requiredFps
        && left.detectionType == right.detectionType
        && left.surfaceType == right.surfaceType
        && left.reflective == right.reflective
        && left.preferMono == right.preferMono
        && left.allowTelecentric == right.allowTelecentric;
}

QFrame *inspectorRow(const QString &label, QLabel **valueLabel)
{
    QFrame *row = new QFrame;
    row->setObjectName(QStringLiteral("InspectorRow"));
    QVBoxLayout *layout = new QVBoxLayout(row);
    layout->setContentsMargins(12, 9, 12, 9);
    layout->setSpacing(4);
    QLabel *name = new QLabel(label);
    name->setObjectName(QStringLiteral("InspectorLabel"));
    *valueLabel = new QLabel;
    (*valueLabel)->setObjectName(QStringLiteral("InspectorValue"));
    (*valueLabel)->setWordWrap(true);
    layout->addWidget(name);
    layout->addWidget(*valueLabel);
    return row;
}
}

InputPage::InputPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(24, 24, 24, 0);
    outer->setSpacing(18);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setObjectName(QStringLiteral("InputWorkspaceSplitter"));
    m_splitter->setChildrenCollapsible(true);

    m_notesEdit = new QTextEdit;
    m_notesEdit->setAccessibleName(localizedText("项目备注", "Project notes"));
    m_notesEdit->setPlaceholderText(localizedText("记录项目背景、工位要求或特殊限制…", "Project context, station requirements, or special constraints…"));
    m_notesEdit->setFixedHeight(76);

    QFrame *editor = new QFrame;
    editor->setObjectName(QStringLiteral("RequirementEditor"));
    QVBoxLayout *editorLayout = new QVBoxLayout(editor);
    editorLayout->setContentsMargins(0, 0, 16, 0);
    editorLayout->setSpacing(18);

    QHBoxLayout *editorHeader = new QHBoxLayout;
    QVBoxLayout *editorCopy = new QVBoxLayout;
    editorCopy->setSpacing(3);
    QLabel *editorTitle = new QLabel(localizedText("定义你的成像任务", "Define your imaging task"));
    editorTitle->setObjectName(QStringLiteral("PageTitle"));
    QLabel *editorSubtitle = new QLabel(localizedText("从工件出发，找到合适的相机、镜头与光源。",
                                                       "Start with the part. Find the right camera, lens, and light."));
    editorSubtitle->setObjectName(QStringLiteral("PaneSubtitle"));
    editorSubtitle->setWordWrap(true);
    QLabel *eyebrow = new QLabel(localizedText("二维选型  /  需求建模", "2D SELECTION  /  REQUIREMENTS"));
    eyebrow->setObjectName(QStringLiteral("PageEyebrow"));
    editorCopy->addWidget(eyebrow);
    editorCopy->addSpacing(5);
    editorCopy->addWidget(editorTitle);
    editorCopy->addWidget(editorSubtitle);
    editorHeader->addLayout(editorCopy, 1);
    QPushButton *resetButton = actionButton(localizedText("重置参数", "Reset"), QStringLiteral("reset"), true);
    resetButton->setAccessibleDescription(localizedText("恢复默认需求参数", "Restore the default requirement values"));
    connect(resetButton, &QPushButton::clicked, this, [this]() {
        const SelectionRequest defaults;
        if (requestsEqual(request(), defaults))
            return;
        if (QMessageBox::question(this,
                                  localizedText("重置参数", "Reset Parameters"),
                                  localizedText("确定恢复默认参数吗？当前输入和备注将被清除。",
                                                "Restore the defaults? Current inputs and notes will be cleared."))
            == QMessageBox::Yes) {
            setRequest(defaults);
        }
    });
    QPushButton *summaryButton = actionButton(localizedText("约束摘要", "Summary"), QString(), true);
    summaryButton->setObjectName(QStringLiteral("SummaryToggleButton"));
    summaryButton->setCheckable(true);
    summaryButton->setChecked(true);
    summaryButton->setAccessibleName(localizedText("显示或隐藏实时约束摘要", "Show or hide live constraints"));
    connect(summaryButton, &QPushButton::clicked, this, [this](bool checked) {
        if (m_splitter->count() == 2)
            m_splitter->widget(1)->setVisible(checked);
    });
    editorHeader->addWidget(summaryButton, 0, Qt::AlignTop);
    editorHeader->addWidget(resetButton, 0, Qt::AlignTop);
    editorLayout->addLayout(editorHeader);

    QScrollArea *scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("RequirementScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *content = new QWidget;
    content->setObjectName(QStringLiteral("RequirementContent"));
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 4, 0);
    layout->setSpacing(14);

    const SelectionRequest defaultRequest;
    m_widthSpin = makeSpin(0.1, 2000.0, defaultRequest.objectWidthMm, QStringLiteral(" mm"));
    m_heightSpin = makeSpin(0.1, 2000.0, defaultRequest.objectHeightMm, QStringLiteral(" mm"));
    m_marginSpin = makeSpin(0.0, 200.0, defaultRequest.placementMarginMm, QStringLiteral(" mm"));
    m_minFeatureSpin = makeSpin(0.1, 10000.0, defaultRequest.minFeatureUm, QStringLiteral(" um"));
    m_toleranceSpin = makeSpin(0.1, 10000.0, defaultRequest.measurementToleranceUm, QStringLiteral(" um"));
    QGridLayout *geometryGrid = nullptr;
    QFrame *geometryGroup = editorGroup(QStringLiteral("01"), localizedText("工件尺寸", "Part Size"), &geometryGrid);
    addGridField(geometryGrid, 0, 0, localizedText("工件宽度 (X)", "Part width (X)"), m_widthSpin);
    addGridField(geometryGrid, 0, 1, localizedText("工件高度 (Y)", "Part height (Y)"), m_heightSpin);
    addGridField(geometryGrid, 0, 2, localizedText("定位/装夹余量", "Fixture margin"), m_marginSpin);
    geometryGrid->setColumnStretch(5, 1);
    QGridLayout *accuracyGrid = nullptr;
    QFrame *accuracyGroup = editorGroup(QStringLiteral("02"), localizedText("精度指标", "Accuracy Targets"), &accuracyGrid);
    addGridField(accuracyGrid, 0, 0, localizedText("最小特征尺寸", "Minimum feature"), m_minFeatureSpin);
    addGridField(accuracyGrid, 0, 1, localizedText("允许测量误差", "Allowed error"), m_toleranceSpin);

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
    m_detectionCombo->setCurrentIndex(static_cast<int>(defaultRequest.detectionType));
    m_surfaceCombo->setCurrentIndex(static_cast<int>(defaultRequest.surfaceType));
    m_wdSpin = makeSpin(5.0, 3000.0, defaultRequest.workingDistanceMm, QStringLiteral(" mm"));
    m_heightVariationSpin = makeSpin(0.0, 200.0, defaultRequest.heightVariationMm, QStringLiteral(" mm"));
    m_speedSpin = makeSpin(0.0, 10000.0, defaultRequest.motionSpeedMmS, QStringLiteral(" mm/s"));
    m_fpsSpin = makeSpin(1.0, 1000.0, defaultRequest.requiredFps, QStringLiteral(" fps"));
    m_motionModeCombo = new QComboBox;
    m_motionModeCombo->addItems({motionModeLabel(MotionMode::Static),
                                 motionModeLabel(MotionMode::StopAndGo),
                                 motionModeLabel(MotionMode::Continuous)});
    m_motionModeCombo->setCurrentIndex(static_cast<int>(defaultRequest.motionMode));

    QGridLayout *cycleGrid = nullptr;
    QFrame *cycleGroup = editorGroup(QStringLiteral("03"), localizedText("成像节拍与安装", "Imaging Cycle and Installation"), &cycleGrid);
    addGridField(cycleGrid, 0, 0, localizedText("检测类型", "Inspection type"), m_detectionCombo);
    addGridField(cycleGrid, 0, 1, localizedText("表面材质", "Surface material"), m_surfaceCombo);
    addGridField(cycleGrid, 1, 0, localizedText("工作距离 (WD)", "Working distance (WD)"), m_wdSpin);
    addGridField(cycleGrid, 1, 1, localizedText("高度波动", "Height variation"), m_heightVariationSpin);
    addGridField(cycleGrid, 2, 0, localizedText("运动模式", "Motion mode"), m_motionModeCombo);
    addGridField(cycleGrid, 2, 1, localizedText("运动速度", "Motion speed"), m_speedSpin);
    addGridField(cycleGrid, 3, 0, localizedText("节拍 / 帧率", "Cycle / frame rate"), m_fpsSpin);

    m_reflectiveCheck = new QCheckBox(localizedText("反光/高光表面", "Reflective / glossy surface"));
    m_reflectiveCheck->setChecked(defaultRequest.reflective);
    m_monoCheck = new QCheckBox(localizedText("优先黑白相机", "Prefer monochrome camera"));
    m_monoCheck->setChecked(defaultRequest.preferMono);
    m_allowTelecentricCheck = new QCheckBox(localizedText("允许远心镜头", "Allow telecentric lens"));
    m_allowTelecentricCheck->setChecked(defaultRequest.allowTelecentric);
    QGridLayout *environmentGrid = nullptr;
    QFrame *environmentGroup = editorGroup(QStringLiteral("04"), localizedText("工艺环境与光学偏好", "Process Environment and Optical Preference"), &environmentGrid);
    environmentGrid->addWidget(m_reflectiveCheck, 0, 0, 1, 2);
    environmentGrid->addWidget(m_monoCheck, 0, 2, 1, 2);
    environmentGrid->addWidget(m_allowTelecentricCheck, 1, 0, 1, 2);

    layout->addWidget(geometryGroup);
    layout->addWidget(accuracyGroup);
    layout->addWidget(cycleGroup);
    layout->addWidget(environmentGroup);
    QGridLayout *notesGrid = nullptr;
    QFrame *notesGroup = editorGroup(QStringLiteral("05"), localizedText("项目备注", "Project notes"), &notesGrid);
    notesGrid->addWidget(m_notesEdit, 0, 0, 1, 4);
    layout->addWidget(notesGroup);
    layout->addStretch();
    scroll->setWidget(content);
    editorLayout->addWidget(scroll, 1);
    editor->setMinimumWidth(510);
    m_splitter->addWidget(editor);
    m_splitter->setCollapsible(0, false);

    QFrame *summaryPanel = new QFrame;
    summaryPanel->setObjectName(QStringLiteral("ConstraintInspector"));
    summaryPanel->setMinimumWidth(276);
    QVBoxLayout *summaryLayout = new QVBoxLayout(summaryPanel);
    summaryLayout->setContentsMargins(18, 20, 18, 18);
    summaryLayout->setSpacing(8);
    QHBoxLayout *inspectorHeader = new QHBoxLayout;
    QLabel *summaryTitle = new QLabel(localizedText("成像目标", "Imaging targets"));
    summaryTitle->setObjectName(QStringLiteral("PaneTitle"));
    QLabel *passBadge = statusBadge(localizedText("实时估算", "Live estimate"), QStringLiteral("success"));
    inspectorHeader->addWidget(summaryTitle, 1);
    inspectorHeader->addWidget(passBadge);
    summaryLayout->addLayout(inspectorHeader);
    FieldOfViewPreview *preview = new FieldOfViewPreview;
    preview->setObjectName(QStringLiteral("FieldOfViewPreview"));
    summaryLayout->addWidget(preview);
    const auto updatePreview = [this, preview]() { preview->setRequirement(request()); };
    for (QDoubleSpinBox *spin : {m_widthSpin, m_heightSpin, m_marginSpin})
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), preview, updatePreview);
    updatePreview();
    QLabel *keyTitle = new QLabel(localizedText("核心指标", "Key targets"));
    keyTitle->setObjectName(QStringLiteral("InspectorSectionTitle"));
    summaryLayout->addWidget(keyTitle);
    summaryLayout->addWidget(inspectorRow(localizedText("目标 FOV", "Target FOV"), &m_fovSummaryLabel));
    summaryLayout->addWidget(inspectorRow(localizedText("目标物方像素", "Object pixel"), &m_pixelSummaryLabel));
    summaryLayout->addWidget(inspectorRow(localizedText("最低相机", "Minimum camera"), &m_resolutionSummaryLabel));
    QLabel *configTitle = new QLabel(localizedText("带宽与节拍", "Bandwidth and cycle"));
    configTitle->setObjectName(QStringLiteral("InspectorSectionTitle"));
    summaryLayout->addWidget(configTitle);
    summaryLayout->addWidget(inspectorRow(localizedText("12 bit 带宽", "12-bit bandwidth"), &m_bandwidthSummaryLabel));
    summaryLayout->addWidget(inspectorRow(localizedText("工作距离", "Working distance"), &m_workDistanceSummaryLabel));
    summaryLayout->addWidget(inspectorRow(localizedText("目标帧率", "Target frame rate"), &m_fpsSummaryLabel));
    QLabel *processTitle = new QLabel(localizedText("工艺输入", "Process inputs"));
    processTitle->setObjectName(QStringLiteral("InspectorSectionTitle"));
    summaryLayout->addWidget(processTitle);
    summaryLayout->addWidget(inspectorRow(localizedText("检测类型", "Inspection type"), &m_detectionSummaryLabel));
    summaryLayout->addWidget(inspectorRow(localizedText("表面材质", "Surface material"), &m_surfaceSummaryLabel));
    summaryLayout->addWidget(inspectorRow(localizedText("曝光上限", "Exposure limit"), &m_exposureSummaryLabel));

    QLabel *verdictTitle = new QLabel(localizedText("工程提示", "Engineering notes"));
    verdictTitle->setObjectName(QStringLiteral("InspectorSectionTitle"));
    summaryLayout->addWidget(verdictTitle);
    QFrame *risk = new QFrame;
    risk->setObjectName(QStringLiteral("InspectorRisk"));
    QVBoxLayout *riskLayout = new QVBoxLayout(risk);
    riskLayout->setContentsMargins(12, 10, 12, 10);
    riskLayout->setSpacing(5);
    QLabel *riskLabel = new QLabel(localizedText("建议关注", "Review Recommended"));
    riskLabel->setObjectName(QStringLiteral("InspectorRiskLabel"));
    m_processSummaryLabel = new QLabel;
    m_processSummaryLabel->setObjectName(QStringLiteral("InspectorRiskValue"));
    m_processSummaryLabel->setWordWrap(true);
    riskLayout->addWidget(riskLabel);
    riskLayout->addWidget(m_processSummaryLabel);
    summaryLayout->addWidget(risk);
    summaryLayout->addStretch();
    QScrollArea *summaryScroll = new QScrollArea;
    summaryScroll->setObjectName(QStringLiteral("ConstraintScroll"));
    summaryScroll->setWidgetResizable(true);
    summaryScroll->setFrameShape(QFrame::NoFrame);
    summaryScroll->setMinimumWidth(298);
    summaryScroll->setWidget(summaryPanel);
    m_splitter->addWidget(summaryScroll);
    m_splitter->setCollapsible(1, false);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
    m_splitter->setSizes({760, 304});
    UiSettings::instance().restoreSplitter(QStringLiteral("input/workspace-v2"), m_splitter);
    outer->addWidget(m_splitter, 1);

    QFrame *commandBar = new QFrame;
    commandBar->setObjectName(QStringLiteral("PageCommandBar"));
    QHBoxLayout *buttonLayout = new QHBoxLayout(commandBar);
    buttonLayout->setContentsMargins(0, 14, 0, 14);
    buttonLayout->setSpacing(10);
    QLabel *commandHint = new QLabel(localizedText("修改输入后，实时约束会立即刷新。",
                                                    "Live constraints refresh immediately when inputs change."));
    commandHint->setObjectName(QStringLiteral("CommandHint"));
    buttonLayout->addWidget(commandHint, 1);
    m_runButton = actionButton(localizedText("生成选型方案    F9", "Generate solutions    F9"),
                               QStringLiteral(":/icons/ui/results.png"));
    m_runButton->setObjectName(QStringLiteral("RunSelectionButton"));
    m_runButton->setMinimumWidth(218);
    m_runButton->setAccessibleDescription(localizedText("执行选型并跳转到结果页",
                                                        "Run selection and open the results page"));
    connect(m_runButton, &QPushButton::clicked, this, &InputPage::runSelectionRequested);
    buttonLayout->addWidget(m_runButton);
    outer->addWidget(commandBar);

    const QList<QDoubleSpinBox *> spins = {
        m_widthSpin, m_heightSpin, m_marginSpin, m_minFeatureSpin, m_toleranceSpin,
        m_wdSpin, m_heightVariationSpin, m_speedSpin, m_fpsSpin
    };
    for (QDoubleSpinBox *spin : spins)
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &InputPage::refreshSummary);
    connect(m_detectionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InputPage::refreshSummary);
    connect(m_surfaceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InputPage::refreshSummary);
    connect(m_motionModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InputPage::refreshSummary);
    connect(m_reflectiveCheck, &QCheckBox::toggled, this, &InputPage::refreshSummary);
    connect(m_monoCheck, &QCheckBox::toggled, this, &InputPage::refreshSummary);
    connect(m_allowTelecentricCheck, &QCheckBox::toggled, this, &InputPage::refreshSummary);

    QWidget::setTabOrder(m_widthSpin, m_heightSpin);
    QWidget::setTabOrder(m_heightSpin, m_marginSpin);
    QWidget::setTabOrder(m_marginSpin, m_minFeatureSpin);
    QWidget::setTabOrder(m_minFeatureSpin, m_toleranceSpin);
    QWidget::setTabOrder(m_toleranceSpin, m_detectionCombo);
    QWidget::setTabOrder(m_detectionCombo, m_surfaceCombo);
    QWidget::setTabOrder(m_surfaceCombo, m_wdSpin);
    QWidget::setTabOrder(m_wdSpin, m_heightVariationSpin);
    QWidget::setTabOrder(m_heightVariationSpin, m_motionModeCombo);
    QWidget::setTabOrder(m_motionModeCombo, m_speedSpin);
    QWidget::setTabOrder(m_speedSpin, m_fpsSpin);
    QWidget::setTabOrder(m_fpsSpin, m_reflectiveCheck);
    QWidget::setTabOrder(m_allowTelecentricCheck, m_notesEdit);
    QWidget::setTabOrder(m_notesEdit, m_runButton);
    refreshSummary();
}

InputPage::~InputPage()
{
    UiSettings::instance().saveSplitter(QStringLiteral("input/workspace-v2"), m_splitter);
}

void InputPage::setBusy(bool busy)
{
    if (m_runButton)
        m_runButton->setEnabled(!busy);
}

void InputPage::refreshSummary()
{
    if (!m_fovSummaryLabel || !m_widthSpin)
        return;

    const SelectionRequest current = request();
    const RequirementEstimate estimate = CalculationAssistant::estimateRequirement(current);
    m_fovSummaryLabel->setText(QStringLiteral("%1 x %2 mm")
        .arg(estimate.requiredFovWidthMm, 0, 'f', 2)
        .arg(estimate.requiredFovHeightMm, 0, 'f', 2));
    m_pixelSummaryLabel->setText(QStringLiteral("%1 um/px")
        .arg(estimate.targetObjectPixelUm, 0, 'f', 2));
    m_resolutionSummaryLabel->setText(QStringLiteral("%1 x %2 (%3 MP)")
        .arg(estimate.requiredResolutionX)
        .arg(estimate.requiredResolutionY)
        .arg(estimate.requiredMegapixels, 0, 'f', 2));
    if (m_bandwidthSummaryLabel)
        m_bandwidthSummaryLabel->setText(QStringLiteral("%1 MB/s").arg(estimate.requiredBandwidthMBps12Bit, 0, 'f', 1));
    if (m_workDistanceSummaryLabel)
        m_workDistanceSummaryLabel->setText(QStringLiteral("%1 mm").arg(current.workingDistanceMm, 0, 'f', 1));
    if (m_fpsSummaryLabel)
        m_fpsSummaryLabel->setText(QStringLiteral("%1 fps").arg(current.requiredFps, 0, 'f', 1));
    if (m_detectionSummaryLabel)
        m_detectionSummaryLabel->setText(detectionTypeLabel(current.detectionType));
    if (m_surfaceSummaryLabel)
        m_surfaceSummaryLabel->setText(surfaceTypeLabel(current.surfaceType));
    if (m_exposureSummaryLabel) {
        m_exposureSummaryLabel->setText(estimate.hasMotionConstraint
            ? QStringLiteral("%1 us").arg(estimate.maxExposureUsForOnePixelBlur, 0, 'f', 1)
            : localizedText("无运动约束", "No motion constraint"));
    }

    QStringList process;
    process.append(detectionTypeLabel(current.detectionType));
    process.append(surfaceTypeLabel(current.surfaceType));
    if (estimate.telecentricPreferred)
        process.append(localizedText("优先评估远心镜头", "Evaluate telecentric lenses"));
    if (estimate.hasMotionConstraint)
        process.append(QStringLiteral("%1 us").arg(estimate.maxExposureUsForOnePixelBlur, 0, 'f', 1));
    m_processSummaryLabel->setText(process.join(localizedText("；", "; ")));
}

SelectionRequest InputPage::request() const
{
    SelectionRequest request;
    request.projectNotes = m_notesEdit ? m_notesEdit->toPlainText().trimmed() : QString();
    request.objectWidthMm = m_widthSpin->value();
    request.objectHeightMm = m_heightSpin->value();
    request.placementMarginMm = m_marginSpin->value();
    request.minFeatureUm = m_minFeatureSpin->value();
    request.measurementToleranceUm = m_toleranceSpin->value();
    request.workingDistanceMm = m_wdSpin->value();
    request.heightVariationMm = m_heightVariationSpin->value();
    request.motionMode = motionModeFromIndex(m_motionModeCombo->currentIndex());
    request.motionSpeedMmS = m_speedSpin->value();
    request.requiredFps = m_fpsSpin->value();
    request.detectionType = detectionTypeFromIndex(m_detectionCombo->currentIndex());
    request.surfaceType = surfaceTypeFromIndex(m_surfaceCombo->currentIndex());
    request.reflective = m_reflectiveCheck->isChecked();
    request.preferMono = m_monoCheck->isChecked();
    request.allowTelecentric = m_allowTelecentricCheck->isChecked();
    return request;
}

void InputPage::setRequest(const SelectionRequest &request)
{
    if (m_notesEdit)
        m_notesEdit->setPlainText(request.projectNotes);
    m_widthSpin->setValue(request.objectWidthMm);
    m_heightSpin->setValue(request.objectHeightMm);
    m_marginSpin->setValue(request.placementMarginMm);
    m_minFeatureSpin->setValue(request.minFeatureUm);
    m_toleranceSpin->setValue(request.measurementToleranceUm);
    m_wdSpin->setValue(request.workingDistanceMm);
    m_heightVariationSpin->setValue(request.heightVariationMm);
    m_motionModeCombo->setCurrentIndex(static_cast<int>(request.motionMode));
    m_speedSpin->setValue(request.motionSpeedMmS);
    m_fpsSpin->setValue(request.requiredFps);
    m_detectionCombo->setCurrentIndex(static_cast<int>(request.detectionType));
    m_surfaceCombo->setCurrentIndex(static_cast<int>(request.surfaceType));
    m_reflectiveCheck->setChecked(request.reflective);
    m_monoCheck->setChecked(request.preferMono);
    m_allowTelecentricCheck->setChecked(request.allowTelecentric);
    refreshSummary();
}
