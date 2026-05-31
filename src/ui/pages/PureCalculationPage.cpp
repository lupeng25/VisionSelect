#include "ui/pages/PureCalculationPage.h"

#include "core/SelectionTypes.h"
#include "ui/UiHelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

using namespace UiHelpers;

PureCalculationPage::PureCalculationPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(28, 24, 28, 24);
    outer->setSpacing(14);
    outer->addWidget(pageHeader(localizedText("纯计算", "Pure Calculation"),
        localizedText("手动输入相机、镜头和光源参数，用于快速工程校核。", "Manually enter camera, lens, and light parameters for quick engineering checks.")));

    QHBoxLayout *body = new QHBoxLayout;
    body->setSpacing(14);

    QScrollArea *inputScroll = new QScrollArea;
    inputScroll->setObjectName(QStringLiteral("ParameterScroll"));
    inputScroll->setWidgetResizable(true);
    inputScroll->setFrameShape(QFrame::NoFrame);
    inputScroll->setMinimumWidth(430);
    inputScroll->setMaximumWidth(520);
    QWidget *inputPanel = new QWidget;
    inputPanel->setObjectName(QStringLiteral("ParameterInputPanel"));
    QVBoxLayout *inputLayout = new QVBoxLayout(inputPanel);
    inputLayout->setContentsMargins(0, 0, 8, 0);
    inputLayout->setSpacing(12);

    const auto prepareControl = [](QWidget *control) {
        control->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        control->setMinimumWidth(0);
        if (QComboBox *combo = qobject_cast<QComboBox *>(control)) {
            combo->setMinimumContentsLength(0);
            combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        }
    };
    const auto field = [prepareControl](const QString &labelText, QWidget *control) {
        QWidget *holder = new QWidget;
        holder->setObjectName(QStringLiteral("ParameterField"));
        QVBoxLayout *layout = new QVBoxLayout(holder);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(5);
        QLabel *label = new QLabel(labelText);
        label->setObjectName(QStringLiteral("ParameterFieldLabel"));
        label->setToolTip(labelText);
        prepareControl(control);
        layout->addWidget(label);
        layout->addWidget(control);
        return holder;
    };

    struct ParameterGroup {
        QFrame *frame;
        QGridLayout *grid;
    };
    const auto makeGroup = [](const QString &title, const QString &subtitle) -> ParameterGroup {
        QFrame *group = new QFrame;
        group->setObjectName(QStringLiteral("ParameterGroup"));
        QVBoxLayout *layout = new QVBoxLayout(group);
        layout->setContentsMargins(14, 12, 14, 14);
        layout->setSpacing(9);
        QLabel *titleLabel = new QLabel(title);
        titleLabel->setObjectName(QStringLiteral("ParameterGroupTitle"));
        layout->addWidget(titleLabel);
        if (!subtitle.isEmpty()) {
            QLabel *subtitleLabel = new QLabel(subtitle);
            subtitleLabel->setObjectName(QStringLiteral("ParameterGroupSubtitle"));
            subtitleLabel->setWordWrap(true);
            layout->addWidget(subtitleLabel);
        }
        QGridLayout *grid = new QGridLayout;
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(10);
        grid->setVerticalSpacing(9);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);
        layout->addLayout(grid);
        return ParameterGroup{group, grid};
    };

    ParameterGroup requestGroup = makeGroup(
        localizedText("需求", "Requirements"),
        localizedText("定义工件、精度、节拍和工艺约束。", "Define part size, accuracy, takt, and process constraints."));
    m_widthSpin = makeSpin(0.1, 2000.0, 20.0, QStringLiteral(" mm"));
    m_heightSpin = makeSpin(0.1, 2000.0, 20.0, QStringLiteral(" mm"));
    m_marginSpin = makeSpin(0.0, 200.0, 2.0, QStringLiteral(" mm"));
    m_minFeatureSpin = makeSpin(0.1, 10000.0, 50.0, QStringLiteral(" um"));
    m_toleranceSpin = makeSpin(0.1, 10000.0, 10.0, QStringLiteral(" um"));
    m_wdSpin = makeSpin(5.0, 3000.0, 110.0, QStringLiteral(" mm"));
    m_heightVariationSpin = makeSpin(0.0, 200.0, 2.0, QStringLiteral(" mm"));
    m_speedSpin = makeSpin(0.0, 10000.0, 0.0, QStringLiteral(" mm/s"));
    m_fpsSpin = makeSpin(1.0, 1000.0, 20.0, QStringLiteral(" fps"));
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
    m_reflectiveCheck = new QCheckBox(localizedText("反光/高光表面", "Reflective / glossy surface"));
    m_reflectiveCheck->setChecked(true);
    requestGroup.grid->addWidget(field(localizedText("工件宽度", "Part width"), m_widthSpin), 0, 0);
    requestGroup.grid->addWidget(field(localizedText("工件高度", "Part height"), m_heightSpin), 0, 1);
    requestGroup.grid->addWidget(field(localizedText("定位/装夹余量", "Positioning / fixture margin"), m_marginSpin), 1, 0);
    requestGroup.grid->addWidget(field(localizedText("最小特征", "Minimum feature"), m_minFeatureSpin), 1, 1);
    requestGroup.grid->addWidget(field(localizedText("允许测量误差", "Allowed measurement error"), m_toleranceSpin), 2, 0);
    requestGroup.grid->addWidget(field(localizedText("工作距离", "Working distance"), m_wdSpin), 2, 1);
    requestGroup.grid->addWidget(field(localizedText("高度波动", "Height variation"), m_heightVariationSpin), 3, 0);
    requestGroup.grid->addWidget(field(localizedText("运动速度", "Motion speed"), m_speedSpin), 3, 1);
    requestGroup.grid->addWidget(field(localizedText("节拍/帧率", "Cycle / frame rate"), m_fpsSpin), 4, 0);
    requestGroup.grid->addWidget(field(localizedText("检测类型", "Inspection type"), m_detectionCombo), 4, 1);
    requestGroup.grid->addWidget(field(localizedText("表面材质", "Surface material"), m_surfaceCombo), 5, 0);
    requestGroup.grid->addWidget(field(localizedText("表面条件", "Surface condition"), m_reflectiveCheck), 5, 1);

    ParameterGroup cameraGroup = makeGroup(
        localizedText("手动相机参数", "Manual Camera Parameters"),
        localizedText("输入分辨率、像元、位深和接口能力。", "Enter resolution, pixel size, bit depth, and interface capacity."));
    m_resolutionXSpin = dialogIntSpin(1, 200000, 2448);
    m_resolutionYSpin = dialogIntSpin(1, 200000, 2048);
    m_pixelSizeSpin = makeSpin(0.01, 1000.0, 3.45, QStringLiteral(" um"));
    m_bitDepthSpin = makeSpin(1.0, 32.0, 12.0, QStringLiteral(" bit"), 1);
    m_interfaceBandwidthSpin = makeSpin(0.0, 100000.0, 380.0, QStringLiteral(" MB/s"), 1);
    m_shutterCombo = new QComboBox;
    m_shutterCombo->addItems({QStringLiteral("Global"), QStringLiteral("Rolling")});
    cameraGroup.grid->addWidget(field(localizedText("分辨率 X", "Resolution X"), m_resolutionXSpin), 0, 0);
    cameraGroup.grid->addWidget(field(localizedText("分辨率 Y", "Resolution Y"), m_resolutionYSpin), 0, 1);
    cameraGroup.grid->addWidget(field(localizedText("像元", "Pixel size"), m_pixelSizeSpin), 1, 0);
    cameraGroup.grid->addWidget(field(QStringLiteral("bit depth"), m_bitDepthSpin), 1, 1);
    cameraGroup.grid->addWidget(field(localizedText("接口带宽", "Interface bandwidth"), m_interfaceBandwidthSpin), 2, 0);
    cameraGroup.grid->addWidget(field(localizedText("快门", "Shutter"), m_shutterCombo), 2, 1);

    ParameterGroup lensGroup = makeGroup(
        localizedText("手动镜头参数", "Manual Lens Parameters"),
        localizedText("普通镜头与远心镜头参数共用，按模式参与计算。", "Fixed-focal and telecentric parameters are used according to the selected mode."));
    m_lensModeCombo = new QComboBox;
    m_lensModeCombo->addItems({localizedText("普通镜头", "Fixed-focal Lens"), localizedText("远心镜头", "Telecentric Lens")});
    m_focalSpin = makeSpin(0.1, 10000.0, 25.0, QStringLiteral(" mm"), 2);
    m_fNumberSpin = makeSpin(0.0, 1000.0, 4.0, QStringLiteral(" F"), 2);
    m_minWdSpin = makeSpin(0.0, 100000.0, 100.0, QStringLiteral(" mm"), 2);
    m_distortionSpin = makeSpin(0.0, 100.0, 0.05, QStringLiteral(" %"), 3);
    m_imageCircleSpin = makeSpin(0.0, 1000.0, 11.0, QStringLiteral(" mm"), 2);
    m_lensMpSpin = makeSpin(0.0, 1000.0, 5.0, QStringLiteral(" MP"), 2);
    m_pmagSpin = makeSpin(0.001, 1000.0, 0.5, QStringLiteral("x"), 3);
    m_nominalWdSpin = makeSpin(0.0, 100000.0, 110.0, QStringLiteral(" mm"), 2);
    m_wdToleranceSpin = makeSpin(0.0, 10000.0, 5.0, QStringLiteral(" mm"), 2);
    m_dofSpin = makeSpin(0.0, 100000.0, 5.0, QStringLiteral(" mm"), 2);
    m_telecentricitySpin = makeSpin(0.0, 90.0, 0.1, QStringLiteral(" deg"), 3);
    lensGroup.grid->addWidget(field(localizedText("模式", "Mode"), m_lensModeCombo), 0, 0);
    lensGroup.grid->addWidget(field(localizedText("普通焦距", "Fixed focal length"), m_focalSpin), 0, 1);
    lensGroup.grid->addWidget(field(QStringLiteral("F/#"), m_fNumberSpin), 1, 0);
    lensGroup.grid->addWidget(field(localizedText("普通最小 WD", "Fixed-lens min WD"), m_minWdSpin), 1, 1);
    lensGroup.grid->addWidget(field(localizedText("畸变", "Distortion"), m_distortionSpin), 2, 0);
    lensGroup.grid->addWidget(field(localizedText("像面", "Image circle"), m_imageCircleSpin), 2, 1);
    lensGroup.grid->addWidget(field(localizedText("镜头 MP", "Lens MP"), m_lensMpSpin), 3, 0);
    lensGroup.grid->addWidget(field(QStringLiteral("PMAG"), m_pmagSpin), 3, 1);
    lensGroup.grid->addWidget(field(localizedText("远心标称 WD", "Telecentric nominal WD"), m_nominalWdSpin), 4, 0);
    lensGroup.grid->addWidget(field(localizedText("WD 容差", "WD tolerance"), m_wdToleranceSpin), 4, 1);
    lensGroup.grid->addWidget(field(QStringLiteral("DOF"), m_dofSpin), 5, 0);
    lensGroup.grid->addWidget(field(localizedText("远心度", "Telecentricity"), m_telecentricitySpin), 5, 1);

    ParameterGroup lightGroup = makeGroup(
        localizedText("光源约束", "Lighting Constraints"),
        localizedText("输入照明类型、触发模式和有效照明范围。", "Enter light type, trigger mode, and active lighting area."));
    m_lightTypeCombo = new QComboBox;
    m_lightTypeCombo->addItems({lightTypeLabel(LightType::Backlight),
                                lightTypeLabel(LightType::Ring),
                                lightTypeLabel(LightType::Bar),
                                lightTypeLabel(LightType::Coaxial),
                                lightTypeLabel(LightType::Dome),
                                lightTypeLabel(LightType::TelecentricBacklight),
                                lightTypeLabel(LightType::DarkField)});
    m_lightModeCombo = new QComboBox;
    m_lightModeCombo->addItems({QStringLiteral("Continuous"), QStringLiteral("Strobe"), QStringLiteral("Trigger")});
    m_lightWidthSpin = makeSpin(0.0, 100000.0, 100.0, QStringLiteral(" mm"), 1);
    m_lightHeightSpin = makeSpin(0.0, 100000.0, 100.0, QStringLiteral(" mm"), 1);
    lightGroup.grid->addWidget(field(localizedText("光型", "Light type"), m_lightTypeCombo), 0, 0);
    lightGroup.grid->addWidget(field(localizedText("模式", "Mode"), m_lightModeCombo), 0, 1);
    lightGroup.grid->addWidget(field(localizedText("有效宽度", "Active width"), m_lightWidthSpin), 1, 0);
    lightGroup.grid->addWidget(field(localizedText("有效高度", "Active height"), m_lightHeightSpin), 1, 1);

    inputLayout->addWidget(requestGroup.frame);
    inputLayout->addWidget(cameraGroup.frame);
    inputLayout->addWidget(lensGroup.frame);
    inputLayout->addWidget(lightGroup.frame);
    inputLayout->addStretch();
    inputScroll->setWidget(inputPanel);

    QFrame *outputPanel = new QFrame;
    outputPanel->setObjectName(QStringLiteral("CalculationResultPanel"));
    QVBoxLayout *outputLayout = new QVBoxLayout(outputPanel);
    outputLayout->setContentsMargins(16, 14, 16, 16);
    outputLayout->setSpacing(12);
    QHBoxLayout *actions = new QHBoxLayout;
    actions->setContentsMargins(0, 0, 0, 0);
    QWidget *resultHeader = new QWidget(outputPanel);
    resultHeader->setObjectName(QStringLiteral("CalculationResultHeader"));
    QVBoxLayout *resultHeaderLayout = new QVBoxLayout(resultHeader);
    resultHeaderLayout->setContentsMargins(0, 0, 0, 0);
    resultHeaderLayout->setSpacing(3);
    QLabel *resultTitle = new QLabel(localizedText("计算结果", "Calculation Result"));
    resultTitle->setObjectName(QStringLiteral("CalculationResultTitle"));
    QLabel *resultSubtitle = new QLabel(localizedText(
        "根据当前手动参数输出需求、相机、镜头和光源校核结论。",
        "Review requirement, camera, lens, and lighting estimates from the current manual parameters."));
    resultSubtitle->setObjectName(QStringLiteral("CalculationResultSubtitle"));
    resultSubtitle->setWordWrap(true);
    resultHeaderLayout->addWidget(resultTitle);
    resultHeaderLayout->addWidget(resultSubtitle);
    actions->addWidget(resultHeader, 1);
    QPushButton *calculateButton = actionButton(localizedText("计算", "Calculate"), QStringLiteral(":/icons/ui/calculate.png"));
    QPushButton *resetButton = actionButton(localizedText("恢复默认", "Reset Defaults"), QStringLiteral(":/icons/ui/info.png"), true);
    actions->addWidget(resetButton);
    actions->addWidget(calculateButton);
    outputLayout->addLayout(actions);

    m_output = new QTextEdit;
    m_output->setObjectName(QStringLiteral("CalculationResultText"));
    m_output->setReadOnly(true);
    outputLayout->addWidget(m_output, 1);

    body->addWidget(inputScroll);
    body->addWidget(outputPanel, 1);
    outer->addLayout(body, 1);

    connect(calculateButton, &QPushButton::clicked, this, &PureCalculationPage::refresh);
    connect(resetButton, &QPushButton::clicked, this, &PureCalculationPage::resetDefaults);
}

PureCalculationInput PureCalculationPage::input() const
{
    PureCalculationInput input;
    SelectionRequest &request = input.request;
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
    request.preferMono = true;
    request.allowTelecentric = true;

    CameraSpec &camera = input.camera;
    camera.model = QStringLiteral("ManualCamera");
    camera.manufacturer = QStringLiteral("Manual");
    camera.resolutionX = m_resolutionXSpin->value();
    camera.resolutionY = m_resolutionYSpin->value();
    camera.pixelSizeUm = m_pixelSizeSpin->value();
    camera.colorMode = QStringLiteral("Mono");
    camera.shutterType = m_shutterCombo->currentText();
    camera.maxFps = request.requiredFps;
    camera.interfaceType = QStringLiteral("Manual");
    camera.bandwidthMBps = m_interfaceBandwidthSpin->value();
    camera.bitDepth = m_bitDepthSpin->value();
    camera.lensMount = QStringLiteral("C");

    LensSpec &lens = input.lens;
    input.telecentricMode = m_lensModeCombo->currentIndex() == 1;
    lens.model = input.telecentricMode ? QStringLiteral("ManualTelecentricLens") : QStringLiteral("ManualFixedLens");
    lens.manufacturer = QStringLiteral("Manual");
    lens.lensType = input.telecentricMode ? LensType::ObjectTelecentric : LensType::FixedFocal;
    lens.lensMount = QStringLiteral("C");
    lens.focalLengthMm = m_focalSpin->value();
    lens.minWorkingDistanceMm = m_minWdSpin->value();
    lens.distortionPercent = m_distortionSpin->value();
    lens.imageCircleMm = m_imageCircleSpin->value();
    lens.megapixelRating = m_lensMpSpin->value();
    lens.pmag = m_pmagSpin->value();
    lens.nominalWorkingDistanceMm = m_nominalWdSpin->value();
    lens.workingDistanceToleranceMm = m_wdToleranceSpin->value();
    lens.maxSensorDiagonalMm = m_imageCircleSpin->value();
    lens.telecentricityDeg = m_telecentricitySpin->value();
    lens.dofMm = m_dofSpin->value();
    lens.fNumber = m_fNumberSpin->value();

    LightSpec &light = input.light;
    light.model = QStringLiteral("ManualLight");
    light.manufacturer = QStringLiteral("Manual");
    switch (m_lightTypeCombo->currentIndex()) {
    case 0: light.lightType = LightType::Backlight; break;
    case 1: light.lightType = LightType::Ring; break;
    case 2: light.lightType = LightType::Bar; break;
    case 3: light.lightType = LightType::Coaxial; break;
    case 4: light.lightType = LightType::Dome; break;
    case 5: light.lightType = LightType::TelecentricBacklight; break;
    case 6: light.lightType = LightType::DarkField; break;
    default: light.lightType = LightType::Ring; break;
    }
    light.mode = m_lightModeCombo->currentText();
    light.color = QStringLiteral("White");
    light.activeWidthMm = m_lightWidthSpin->value();
    light.activeHeightMm = m_lightHeightSpin->value();
    return input;
}

void PureCalculationPage::refresh()
{
    if (!m_output || !m_widthSpin)
        return;

    const PureCalculationInput pureInput = input();
    const PureCalculationResult r = CalculationAssistant::estimatePure(pureInput);
    const QString risks = r.risks.isEmpty()
        ? localizedText("无主要风险", "No major risk")
        : r.risks.join(localizedText("；", "; "));
    const QString reasons = r.reasons.isEmpty()
        ? localizedText("按当前手动参数计算", "Calculated from current manual parameters")
        : r.reasons.join(localizedText("；", "; "));
    const QString exposure = r.requirement.hasMotionConstraint
        ? QStringLiteral("%1 us").arg(r.requirement.maxExposureUsForOnePixelBlur, 0, 'f', 1)
        : localizedText("无运动约束", "No motion constraint");

    QString html;
    html += localizedText("<h3>纯计算结果</h3>", "<h3>Pure Calculation Result</h3>");
    html += localizedText("<h4>需求估算</h4>", "<h4>Requirement Estimate</h4>");
    html += localizedText("<p>需求 FOV：<b>%1 x %2 mm</b>；目标物方像素：<b>%3 um/px</b>；最低分辨率：<b>%4 x %5</b>（%6 MP）；12 bit 原始带宽：<b>%7 MB/s</b>；曝光上限：<b>%8</b>。</p>",
                          "<p>Required FOV: <b>%1 x %2 mm</b>; target object pixel: <b>%3 um/px</b>; minimum resolution: <b>%4 x %5</b> (%6 MP); 12-bit raw bandwidth: <b>%7 MB/s</b>; exposure limit: <b>%8</b>.</p>")
        .arg(r.requirement.requiredFovWidthMm, 0, 'f', 2)
        .arg(r.requirement.requiredFovHeightMm, 0, 'f', 2)
        .arg(r.requirement.targetObjectPixelUm, 0, 'f', 2)
        .arg(r.requirement.requiredResolutionX)
        .arg(r.requirement.requiredResolutionY)
        .arg(r.requirement.requiredMegapixels, 0, 'f', 2)
        .arg(r.requirement.requiredBandwidthMBps12Bit, 0, 'f', 1)
        .arg(exposure);

    html += localizedText("<h4>相机估算</h4>", "<h4>Camera Estimate</h4>");
    html += localizedText("<p>传感器：<b>%1 x %2 mm</b>，对角线 %3 mm；按需求 FOV 的物方像素：<b>%4 um/px</b>；单帧数据：%5 MB；吞吐：%6 MB/s；接口带宽：%7 MB/s；利用率：<b>%8%</b>；原始存储：<b>%9 GB/h</b>。</p>",
                          "<p>Sensor: <b>%1 x %2 mm</b>, diagonal %3 mm; object pixel at required FOV: <b>%4 um/px</b>; frame payload: %5 MB; throughput: %6 MB/s; interface bandwidth: %7 MB/s; utilization: <b>%8%</b>; raw storage: <b>%9 GB/h</b>.</p>")
        .arg(r.sensorWidthMm, 0, 'f', 2)
        .arg(r.sensorHeightMm, 0, 'f', 2)
        .arg(r.sensorDiagonalMm, 0, 'f', 2)
        .arg(r.cameraObjectPixelSizeUm, 0, 'f', 2)
        .arg(r.framePayloadMB, 0, 'f', 2)
        .arg(r.bandwidthRequiredMBps, 0, 'f', 1)
        .arg(r.interfaceCapacityMBps, 0, 'f', 1)
        .arg(r.bandwidthUtilizationPercent, 0, 'f', 0)
        .arg(r.storagePerHourGB, 0, 'f', 0);

    html += localizedText("<h4>镜头估算</h4>", "<h4>Lens Estimate</h4>");
    html += QStringLiteral("<p>%1</p>").arg(r.lensFormulaSummary);
    if (pureInput.telecentricMode) {
        html += localizedText("<p>远心 PMAG：<b>%1x</b>；实际 FOV：<b>%2 x %3 mm</b>；物方像素：<b>%4 um/px</b>；残余远心误差：<b>%5 um</b>；DOF：<b>%6 mm</b>；畸变边缘误差：<b>%7 um</b>。</p>",
                              "<p>Telecentric PMAG: <b>%1x</b>; actual FOV: <b>%2 x %3 mm</b>; object pixel: <b>%4 um/px</b>; residual telecentric error: <b>%5 um</b>; DOF: <b>%6 mm</b>; edge distortion error: <b>%7 um</b>.</p>")
            .arg(r.magnification, 0, 'f', 3)
            .arg(r.effectiveFovWidthMm, 0, 'f', 2)
            .arg(r.effectiveFovHeightMm, 0, 'f', 2)
            .arg(r.lensObjectPixelSizeUm, 0, 'f', 2)
            .arg(r.residualTelecentricErrorUm, 0, 'f', 2)
            .arg(r.estimatedDofMm, 0, 'f', 2)
            .arg(r.distortionErrorUm, 0, 'f', 2);
    } else {
        html += localizedText("<p>目标焦距：<b>%1 mm</b>；输入焦距下实际 FOV：<b>%2 x %3 mm</b>；倍率：<b>%4x</b>；物方像素：<b>%5 um/px</b>；估算 DOF：<b>%6 mm</b>；畸变边缘误差：<b>%7 um</b>。</p>",
                              "<p>Target focal length: <b>%1 mm</b>; actual FOV with entered focal length: <b>%2 x %3 mm</b>; magnification: <b>%4x</b>; object pixel: <b>%5 um/px</b>; estimated DOF: <b>%6 mm</b>; edge distortion error: <b>%7 um</b>.</p>")
            .arg(r.targetFixedFocalLengthMm, 0, 'f', 2)
            .arg(r.effectiveFovWidthMm, 0, 'f', 2)
            .arg(r.effectiveFovHeightMm, 0, 'f', 2)
            .arg(r.magnification, 0, 'f', 3)
            .arg(r.lensObjectPixelSizeUm, 0, 'f', 2)
            .arg(r.estimatedDofMm, 0, 'f', 2)
            .arg(r.distortionErrorUm, 0, 'f', 2);
    }

    html += localizedText("<h4>光源估算</h4>", "<h4>Lighting Estimate</h4>");
    html += localizedText("<p>建议有效照明面积：<b>%1 x %2 mm</b>；当前光源覆盖余量：<b>%3%</b>。</p>",
                          "<p>Recommended active lighting area: <b>%1 x %2 mm</b>; current light coverage margin: <b>%3%</b>.</p>")
        .arg(r.suggestedLightWidthMm, 0, 'f', 1)
        .arg(r.suggestedLightHeightMm, 0, 'f', 1)
        .arg(r.lightCoverageMarginPercent, 0, 'f', 0);
    html += localizedText("<h4>判断</h4>", "<h4>Verdict</h4>");
    html += localizedText("<p><b>依据：</b>%1</p>", "<p><b>Reasons:</b> %1</p>").arg(reasons);
    html += localizedText("<p><b>风险：</b>%1</p>", "<p><b>Risks:</b> %1</p>").arg(risks);
    m_output->setHtml(html);
}

void PureCalculationPage::resetDefaults()
{
    m_widthSpin->setValue(20.0);
    m_heightSpin->setValue(20.0);
    m_marginSpin->setValue(2.0);
    m_minFeatureSpin->setValue(50.0);
    m_toleranceSpin->setValue(10.0);
    m_wdSpin->setValue(110.0);
    m_heightVariationSpin->setValue(2.0);
    m_speedSpin->setValue(0.0);
    m_fpsSpin->setValue(20.0);
    m_detectionCombo->setCurrentIndex(0);
    m_surfaceCombo->setCurrentIndex(1);
    m_reflectiveCheck->setChecked(true);
    m_resolutionXSpin->setValue(2448);
    m_resolutionYSpin->setValue(2048);
    m_pixelSizeSpin->setValue(3.45);
    m_bitDepthSpin->setValue(12.0);
    m_interfaceBandwidthSpin->setValue(380.0);
    m_shutterCombo->setCurrentIndex(0);
    m_lensModeCombo->setCurrentIndex(0);
    m_focalSpin->setValue(25.0);
    m_fNumberSpin->setValue(4.0);
    m_minWdSpin->setValue(100.0);
    m_distortionSpin->setValue(0.05);
    m_imageCircleSpin->setValue(11.0);
    m_lensMpSpin->setValue(5.0);
    m_pmagSpin->setValue(0.5);
    m_nominalWdSpin->setValue(110.0);
    m_wdToleranceSpin->setValue(5.0);
    m_dofSpin->setValue(5.0);
    m_telecentricitySpin->setValue(0.1);
    m_lightTypeCombo->setCurrentIndex(1);
    m_lightModeCombo->setCurrentIndex(0);
    m_lightWidthSpin->setValue(100.0);
    m_lightHeightSpin->setValue(100.0);
    refresh();
}
