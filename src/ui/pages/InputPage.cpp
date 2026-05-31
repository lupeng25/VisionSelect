#include "ui/pages/InputPage.h"

#include "selection/CalculationAssistant.h"
#include "ui/UiHelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QVBoxLayout>

using namespace UiHelpers;

InputPage::InputPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(26, 22, 26, 22);
    outer->setSpacing(16);
    outer->addWidget(pageHeader(localizedText("需求建模", "Requirement Modeling"),
        localizedText("录入工件、节拍和安装约束，形成后续选型与报告的统一输入。",
                      "Capture part, cycle, and installation constraints for downstream selection and reporting.")));

    QHBoxLayout *body = new QHBoxLayout;
    body->setSpacing(16);

    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumWidth(520);
    QWidget *content = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 8, 0);
    layout->setSpacing(14);

    QGroupBox *objectBox = new QGroupBox(localizedText("工件与精度", "Part and Accuracy"));
    QFormLayout *objectLayout = new QFormLayout(objectBox);
    objectLayout->setLabelAlignment(Qt::AlignLeft);
    m_widthSpin = makeSpin(0.1, 2000.0, 20.0, QStringLiteral(" mm"));
    m_heightSpin = makeSpin(0.1, 2000.0, 20.0, QStringLiteral(" mm"));
    m_marginSpin = makeSpin(0.0, 200.0, 2.0, QStringLiteral(" mm"));
    m_minFeatureSpin = makeSpin(0.1, 10000.0, 50.0, QStringLiteral(" um"));
    m_toleranceSpin = makeSpin(0.1, 10000.0, 10.0, QStringLiteral(" um"));
    objectLayout->addRow(localizedText("工件宽度", "Part width"), m_widthSpin);
    objectLayout->addRow(localizedText("工件高度", "Part height"), m_heightSpin);
    objectLayout->addRow(localizedText("定位/装夹余量", "Positioning / fixture margin"), m_marginSpin);
    objectLayout->addRow(localizedText("最小特征尺寸", "Minimum feature size"), m_minFeatureSpin);
    objectLayout->addRow(localizedText("允许测量误差", "Allowed measurement error"), m_toleranceSpin);

    QGroupBox *processBox = new QGroupBox(localizedText("工艺与安装", "Process and Installation"));
    QFormLayout *processLayout = new QFormLayout(processBox);
    processLayout->setLabelAlignment(Qt::AlignLeft);
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
    m_surfaceCombo->setCurrentIndex(1);
    m_wdSpin = makeSpin(5.0, 3000.0, 110.0, QStringLiteral(" mm"));
    m_heightVariationSpin = makeSpin(0.0, 200.0, 2.0, QStringLiteral(" mm"));
    m_speedSpin = makeSpin(0.0, 10000.0, 0.0, QStringLiteral(" mm/s"));
    m_fpsSpin = makeSpin(1.0, 1000.0, 20.0, QStringLiteral(" fps"));
    m_reflectiveCheck = new QCheckBox(localizedText("反光/高光表面", "Reflective / glossy surface"));
    m_reflectiveCheck->setChecked(true);
    m_monoCheck = new QCheckBox(localizedText("优先黑白相机", "Prefer monochrome camera"));
    m_monoCheck->setChecked(true);
    m_allowTelecentricCheck = new QCheckBox(localizedText("允许远心镜头", "Allow telecentric lens"));
    m_allowTelecentricCheck->setChecked(true);
    processLayout->addRow(localizedText("检测类型", "Inspection type"), m_detectionCombo);
    processLayout->addRow(localizedText("表面材质", "Surface material"), m_surfaceCombo);
    processLayout->addRow(localizedText("工作距离", "Working distance"), m_wdSpin);
    processLayout->addRow(localizedText("高度波动", "Height variation"), m_heightVariationSpin);
    processLayout->addRow(localizedText("运动速度", "Motion speed"), m_speedSpin);
    processLayout->addRow(localizedText("节拍/帧率", "Cycle / frame rate"), m_fpsSpin);
    processLayout->addRow(QString(), m_reflectiveCheck);
    processLayout->addRow(QString(), m_monoCheck);
    processLayout->addRow(QString(), m_allowTelecentricCheck);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    QPushButton *calculateButton = actionButton(localizedText("计算推荐", "Calculate Recommendations"), QStringLiteral(":/icons/ui/calculate.png"));
    QPushButton *resultButton = actionButton(localizedText("查看结果", "View Results"), QStringLiteral(":/icons/ui/results.png"), true);
    connect(calculateButton, &QPushButton::clicked, this, &InputPage::calculateRequested);
    connect(resultButton, &QPushButton::clicked, this, &InputPage::resultsRequested);
    buttonLayout->addWidget(calculateButton);
    buttonLayout->addWidget(resultButton);
    buttonLayout->addStretch();

    layout->addWidget(objectBox);
    layout->addWidget(processBox);
    layout->addLayout(buttonLayout);
    layout->addStretch();
    scroll->setWidget(content);
    body->addWidget(scroll, 3);

    QFrame *summaryPanel = new QFrame;
    summaryPanel->setObjectName(QStringLiteral("SectionCard"));
    summaryPanel->setMinimumWidth(300);
    summaryPanel->setMaximumWidth(360);
    QVBoxLayout *summaryLayout = new QVBoxLayout(summaryPanel);
    summaryLayout->setContentsMargins(16, 16, 16, 16);
    summaryLayout->setSpacing(12);
    QLabel *summaryTitle = new QLabel(localizedText("需求摘要", "Requirement Summary"));
    summaryTitle->setObjectName(QStringLiteral("SectionTitle"));
    QLabel *summaryHint = new QLabel(localizedText("摘要会随左侧输入实时更新，便于在计算前确认约束量级。",
                                                   "This summary updates with the form so constraints can be checked before calculation."));
    summaryHint->setObjectName(QStringLiteral("PageSubtitle"));
    summaryHint->setWordWrap(true);
    summaryLayout->addWidget(summaryTitle);
    summaryLayout->addWidget(summaryHint);

    const auto addSummaryMetric = [summaryLayout](const QString &label, QLabel **valueLabel, const QString &state) {
        QFrame *card = new QFrame;
        card->setObjectName(QStringLiteral("MetricCard"));
        setWidgetState(card, state);
        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 10, 12, 10);
        cardLayout->setSpacing(4);
        QLabel *metricLabel = new QLabel(label);
        metricLabel->setObjectName(QStringLiteral("MetricLabel"));
        *valueLabel = new QLabel;
        (*valueLabel)->setObjectName(QStringLiteral("MetricValue"));
        (*valueLabel)->setWordWrap(true);
        cardLayout->addWidget(metricLabel);
        cardLayout->addWidget(*valueLabel);
        summaryLayout->addWidget(card);
    };
    addSummaryMetric(localizedText("目标 FOV", "Target FOV"), &m_fovSummaryLabel, QStringLiteral("info"));
    addSummaryMetric(localizedText("目标物方像素", "Target Object Pixel"), &m_pixelSummaryLabel, QStringLiteral("good"));
    addSummaryMetric(localizedText("最低相机建议", "Minimum Camera"), &m_resolutionSummaryLabel, QString());
    addSummaryMetric(localizedText("工艺判断", "Process Verdict"), &m_processSummaryLabel, QStringLiteral("warn"));
    summaryLayout->addStretch();
    body->addWidget(summaryPanel, 1);
    outer->addLayout(body, 1);

    const QList<QDoubleSpinBox *> spins = {
        m_widthSpin, m_heightSpin, m_marginSpin, m_minFeatureSpin, m_toleranceSpin,
        m_wdSpin, m_heightVariationSpin, m_speedSpin, m_fpsSpin
    };
    for (QDoubleSpinBox *spin : spins)
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &InputPage::refreshSummary);
    connect(m_detectionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InputPage::refreshSummary);
    connect(m_surfaceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InputPage::refreshSummary);
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
    request.objectWidthMm = m_widthSpin->value();
    request.objectHeightMm = m_heightSpin->value();
    request.placementMarginMm = m_marginSpin->value();
    request.minFeatureUm = m_minFeatureSpin->value();
    request.measurementToleranceUm = m_toleranceSpin->value();
    request.workingDistanceMm = m_wdSpin->value();
    request.heightVariationMm = m_heightVariationSpin->value();
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
    m_widthSpin->setValue(request.objectWidthMm);
    m_heightSpin->setValue(request.objectHeightMm);
    m_marginSpin->setValue(request.placementMarginMm);
    m_minFeatureSpin->setValue(request.minFeatureUm);
    m_toleranceSpin->setValue(request.measurementToleranceUm);
    m_wdSpin->setValue(request.workingDistanceMm);
    m_heightVariationSpin->setValue(request.heightVariationMm);
    m_speedSpin->setValue(request.motionSpeedMmS);
    m_fpsSpin->setValue(request.requiredFps);
    m_detectionCombo->setCurrentIndex(static_cast<int>(request.detectionType));
    m_surfaceCombo->setCurrentIndex(static_cast<int>(request.surfaceType));
    m_reflectiveCheck->setChecked(request.reflective);
    m_monoCheck->setChecked(request.preferMono);
    m_allowTelecentricCheck->setChecked(request.allowTelecentric);
    refreshSummary();
}
