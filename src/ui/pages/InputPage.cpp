#include "ui/pages/InputPage.h"

#include "selection/CalculationAssistant.h"
#include "ui/UiHelpers.h"

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
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStringList>
#include <QTextEdit>
#include <QVBoxLayout>

using namespace UiHelpers;

namespace {
QLabel *fieldLabel(const QString &text)
{
    QLabel *label = new QLabel(text);
    label->setObjectName(QStringLiteral("EditorFieldLabel"));
    return label;
}

QFrame *outlineSection(const QString &title, const QString &detail, const QString &state)
{
    QFrame *section = new QFrame;
    section->setObjectName(QStringLiteral("OutlineSection"));
    section->setProperty("state", state);
    QVBoxLayout *layout = new QVBoxLayout(section);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(5);

    QHBoxLayout *header = new QHBoxLayout;
    QLabel *titleLabel = new QLabel(title);
    titleLabel->setObjectName(QStringLiteral("OutlineSectionTitle"));
    QLabel *status = new QLabel(state == QLatin1String("warn")
        ? localizedText("△ 需关注", "△ Review") : localizedText("✓ 已完整", "✓ Complete"));
    status->setObjectName(QStringLiteral("OutlineSectionStatus"));
    header->addWidget(titleLabel, 1);
    header->addWidget(status);
    layout->addLayout(header);

    QLabel *detailLabel = new QLabel(detail);
    detailLabel->setObjectName(QStringLiteral("OutlineSectionDetail"));
    detailLabel->setWordWrap(true);
    layout->addWidget(detailLabel);
    return section;
}

QFrame *editorGroup(const QString &number, const QString &title, QGridLayout **grid)
{
    QFrame *group = new QFrame;
    group->setObjectName(QStringLiteral("EditorGroup"));
    QVBoxLayout *layout = new QVBoxLayout(group);
    layout->setContentsMargins(14, 12, 14, 14);
    layout->setSpacing(10);
    QLabel *heading = new QLabel(QStringLiteral("%1  %2").arg(number, title));
    heading->setObjectName(QStringLiteral("EditorGroupTitle"));
    layout->addWidget(heading);
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
    grid->addWidget(fieldLabel(label), row, column);
    control->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    grid->addWidget(control, row, column + 1);
}

QFrame *inspectorRow(const QString &label, QLabel **valueLabel)
{
    QFrame *row = new QFrame;
    row->setObjectName(QStringLiteral("InspectorRow"));
    QHBoxLayout *layout = new QHBoxLayout(row);
    layout->setContentsMargins(10, 7, 8, 7);
    layout->setSpacing(8);
    QLabel *name = new QLabel(label);
    name->setObjectName(QStringLiteral("InspectorLabel"));
    *valueLabel = new QLabel;
    (*valueLabel)->setObjectName(QStringLiteral("InspectorValue"));
    (*valueLabel)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QLabel *state = new QLabel(QString::fromUtf8("●"));
    state->setObjectName(QStringLiteral("InspectorPass"));
    layout->addWidget(name, 1);
    layout->addWidget(*valueLabel);
    layout->addWidget(state);
    return row;
}
}

InputPage::InputPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    QHBoxLayout *body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    QFrame *outline = new QFrame;
    outline->setObjectName(QStringLiteral("RequirementOutline"));
    outline->setFixedWidth(250);
    QVBoxLayout *outlineLayout = new QVBoxLayout(outline);
    outlineLayout->setContentsMargins(12, 14, 12, 12);
    outlineLayout->setSpacing(8);
    QLabel *outlineTitle = new QLabel(localizedText("需求结构", "Requirement Structure"));
    outlineTitle->setObjectName(QStringLiteral("PaneTitle"));
    QLabel *outlineSubtitle = new QLabel(localizedText("按工程约束组织输入，右侧实时校验。",
                                                        "Inputs organized by engineering constraints."));
    outlineSubtitle->setObjectName(QStringLiteral("PaneSubtitle"));
    outlineSubtitle->setWordWrap(true);
    outlineLayout->addWidget(outlineTitle);
    outlineLayout->addWidget(outlineSubtitle);
    outlineLayout->addWidget(outlineSection(localizedText("工件几何", "Part Geometry"),
        localizedText("工件尺寸 · 定位与装夹余量",
                      "Part size · positioning and fixture margin"), QStringLiteral("active")));
    outlineLayout->addWidget(outlineSection(localizedText("精度指标", "Accuracy Targets"),
        localizedText("最小特征 · 允许测量误差",
                      "Minimum feature · allowed measurement error"), QStringLiteral("good")));
    outlineLayout->addWidget(outlineSection(localizedText("成像节拍", "Imaging Cycle"),
        localizedText("工作距离 · 高度波动 · 速度与帧率",
                      "Working distance · variation · speed and frame rate"), QStringLiteral("good")));
    outlineLayout->addWidget(outlineSection(localizedText("工艺环境", "Process Environment"),
        localizedText("检测类型 · 表面材质 · 光学偏好",
                      "Inspection · surface · optical preferences"), QStringLiteral("warn")));
    outlineLayout->addStretch();
    QLabel *notesTitle = new QLabel(localizedText("备注", "Notes"));
    notesTitle->setObjectName(QStringLiteral("OutlineFieldTitle"));
    outlineLayout->addWidget(notesTitle);
    m_notesEdit = new QTextEdit;
    m_notesEdit->setPlaceholderText(localizedText("输入项目或工位备注…", "Add project or station notes…"));
    m_notesEdit->setFixedHeight(68);
    outlineLayout->addWidget(m_notesEdit);
    body->addWidget(outline);

    QFrame *editor = new QFrame;
    editor->setObjectName(QStringLiteral("RequirementEditor"));
    QVBoxLayout *editorLayout = new QVBoxLayout(editor);
    editorLayout->setContentsMargins(16, 14, 16, 10);
    editorLayout->setSpacing(10);

    QHBoxLayout *editorHeader = new QHBoxLayout;
    QVBoxLayout *editorCopy = new QVBoxLayout;
    editorCopy->setSpacing(3);
    QLabel *editorTitle = new QLabel(localizedText("工件与工程约束", "Part and Engineering Constraints"));
    editorTitle->setObjectName(QStringLiteral("PaneTitle"));
    QLabel *editorSubtitle = new QLabel(localizedText("定义计算视场、分辨率、节拍与镜头形式所需的核心输入。",
                                                       "Define the inputs used to derive FOV, sampling, cycle, and lens form."));
    editorSubtitle->setObjectName(QStringLiteral("PaneSubtitle"));
    editorSubtitle->setWordWrap(true);
    editorCopy->addWidget(editorTitle);
    editorCopy->addWidget(editorSubtitle);
    editorHeader->addLayout(editorCopy, 1);
    QPushButton *resetButton = actionButton(localizedText("重置参数", "Reset"), QStringLiteral(":/icons/ui/calculate.png"), true);
    connect(resetButton, &QPushButton::clicked, this, [this]() { setRequest(SelectionRequest()); });
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
    layout->setSpacing(10);

    const SelectionRequest defaultRequest;
    m_widthSpin = makeSpin(0.1, 2000.0, defaultRequest.objectWidthMm, QStringLiteral(" mm"));
    m_heightSpin = makeSpin(0.1, 2000.0, defaultRequest.objectHeightMm, QStringLiteral(" mm"));
    m_marginSpin = makeSpin(0.0, 200.0, defaultRequest.placementMarginMm, QStringLiteral(" mm"));
    m_minFeatureSpin = makeSpin(0.1, 10000.0, defaultRequest.minFeatureUm, QStringLiteral(" um"));
    m_toleranceSpin = makeSpin(0.1, 10000.0, defaultRequest.measurementToleranceUm, QStringLiteral(" um"));
    QGridLayout *geometryGrid = nullptr;
    QFrame *geometryGroup = editorGroup(QStringLiteral("1."), localizedText("工件尺寸", "Part Size"), &geometryGrid);
    addGridField(geometryGrid, 0, 0, localizedText("工件宽度 (X)", "Part width (X)"), m_widthSpin);
    addGridField(geometryGrid, 0, 1, localizedText("工件高度 (Y)", "Part height (Y)"), m_heightSpin);
    addGridField(geometryGrid, 1, 0, localizedText("定位/装夹余量", "Fixture margin"), m_marginSpin);
    QGridLayout *accuracyGrid = nullptr;
    QFrame *accuracyGroup = editorGroup(QStringLiteral("2."), localizedText("精度指标", "Accuracy Targets"), &accuracyGrid);
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
    QFrame *cycleGroup = editorGroup(QStringLiteral("3."), localizedText("成像节拍与安装", "Imaging Cycle and Installation"), &cycleGrid);
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
    QFrame *environmentGroup = editorGroup(QStringLiteral("4."), localizedText("工艺环境与光学偏好", "Process Environment and Optical Preference"), &environmentGrid);
    environmentGrid->addWidget(m_reflectiveCheck, 0, 0, 1, 2);
    environmentGrid->addWidget(m_monoCheck, 0, 2, 1, 2);
    environmentGrid->addWidget(m_allowTelecentricCheck, 1, 0, 1, 2);

    layout->addWidget(geometryGroup);
    layout->addWidget(accuracyGroup);
    layout->addWidget(cycleGroup);
    layout->addWidget(environmentGroup);
    layout->addStretch();
    scroll->setWidget(content);
    editorLayout->addWidget(scroll, 1);
    body->addWidget(editor, 1);

    QFrame *summaryPanel = new QFrame;
    summaryPanel->setObjectName(QStringLiteral("ConstraintInspector"));
    summaryPanel->setFixedWidth(332);
    QVBoxLayout *summaryLayout = new QVBoxLayout(summaryPanel);
    summaryLayout->setContentsMargins(14, 14, 14, 12);
    summaryLayout->setSpacing(8);
    QHBoxLayout *inspectorHeader = new QHBoxLayout;
    QLabel *summaryTitle = new QLabel(localizedText("实时约束", "Live Constraints"));
    summaryTitle->setObjectName(QStringLiteral("PaneTitle"));
    QLabel *passBadge = statusBadge(localizedText("通过", "Pass"), QStringLiteral("good"));
    inspectorHeader->addWidget(summaryTitle, 1);
    inspectorHeader->addWidget(passBadge);
    summaryLayout->addLayout(inspectorHeader);
    QLabel *keyTitle = new QLabel(localizedText("A. 关键目标", "A. Key Targets"));
    keyTitle->setObjectName(QStringLiteral("InspectorSectionTitle"));
    summaryLayout->addWidget(keyTitle);
    summaryLayout->addWidget(inspectorRow(localizedText("目标 FOV", "Target FOV"), &m_fovSummaryLabel));
    summaryLayout->addWidget(inspectorRow(localizedText("目标物方像素", "Object pixel"), &m_pixelSummaryLabel));
    summaryLayout->addWidget(inspectorRow(localizedText("最低相机", "Minimum camera"), &m_resolutionSummaryLabel));
    QLabel *configTitle = new QLabel(localizedText("B. 带宽与节拍", "B. Bandwidth and Cycle"));
    configTitle->setObjectName(QStringLiteral("InspectorSectionTitle"));
    summaryLayout->addWidget(configTitle);
    summaryLayout->addWidget(inspectorRow(localizedText("12 bit 带宽", "12-bit bandwidth"), &m_bandwidthSummaryLabel));
    summaryLayout->addWidget(inspectorRow(localizedText("工作距离", "Working distance"), &m_workDistanceSummaryLabel));
    summaryLayout->addWidget(inspectorRow(localizedText("目标帧率", "Target frame rate"), &m_fpsSummaryLabel));
    QLabel *processTitle = new QLabel(localizedText("C. 工艺输入", "C. Process Inputs"));
    processTitle->setObjectName(QStringLiteral("InspectorSectionTitle"));
    summaryLayout->addWidget(processTitle);
    summaryLayout->addWidget(inspectorRow(localizedText("检测类型", "Inspection type"), &m_detectionSummaryLabel));
    summaryLayout->addWidget(inspectorRow(localizedText("表面材质", "Surface material"), &m_surfaceSummaryLabel));
    summaryLayout->addWidget(inspectorRow(localizedText("曝光上限", "Exposure limit"), &m_exposureSummaryLabel));

    QLabel *verdictTitle = new QLabel(localizedText("D. 工艺判断", "D. Process Verdict"));
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
    body->addWidget(summaryPanel);
    outer->addLayout(body, 1);

    QFrame *commandBar = new QFrame;
    commandBar->setObjectName(QStringLiteral("PageCommandBar"));
    QHBoxLayout *buttonLayout = new QHBoxLayout(commandBar);
    buttonLayout->setContentsMargins(14, 8, 14, 8);
    buttonLayout->setSpacing(10);
    QLabel *commandHint = new QLabel(localizedText("修改输入后，实时约束会立即刷新。",
                                                    "Live constraints refresh immediately when inputs change."));
    commandHint->setObjectName(QStringLiteral("CommandHint"));
    buttonLayout->addWidget(commandHint, 1);
    QPushButton *resultButton = actionButton(localizedText("计算并查看结果", "Calculate and Review"), QStringLiteral(":/icons/ui/results.png"), true);
    QPushButton *calculateButton = actionButton(localizedText("运行选型  F9", "Run Selection  F9"), QStringLiteral(":/icons/ui/calculate.png"));
    connect(calculateButton, &QPushButton::clicked, this, &InputPage::calculateRequested);
    connect(resultButton, &QPushButton::clicked, this, &InputPage::resultsRequested);
    buttonLayout->addWidget(resultButton);
    buttonLayout->addWidget(calculateButton);
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
    refreshSummary();
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
