#include "ui/pages/PureCalculationPage.h"

#include "core/SelectionTypes.h"
#include "ui/UiHelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
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
    outer->addWidget(pageTitle(QString::fromUtf8("纯计算")));

    QHBoxLayout *body = new QHBoxLayout;
    body->setSpacing(14);

    QScrollArea *inputScroll = new QScrollArea;
    inputScroll->setWidgetResizable(true);
    inputScroll->setFrameShape(QFrame::NoFrame);
    inputScroll->setMinimumWidth(380);
    inputScroll->setMaximumWidth(470);
    QWidget *inputPanel = new QWidget;
    QVBoxLayout *inputLayout = new QVBoxLayout(inputPanel);
    inputLayout->setContentsMargins(0, 0, 8, 0);
    inputLayout->setSpacing(10);

    QGroupBox *requestBox = new QGroupBox(QString::fromUtf8("需求"));
    QFormLayout *requestLayout = new QFormLayout(requestBox);
    requestLayout->setLabelAlignment(Qt::AlignLeft);
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
    m_reflectiveCheck = new QCheckBox(QString::fromUtf8("反光/高光表面"));
    m_reflectiveCheck->setChecked(true);
    requestLayout->addRow(QString::fromUtf8("工件宽度"), m_widthSpin);
    requestLayout->addRow(QString::fromUtf8("工件高度"), m_heightSpin);
    requestLayout->addRow(QString::fromUtf8("定位/装夹余量"), m_marginSpin);
    requestLayout->addRow(QString::fromUtf8("最小特征"), m_minFeatureSpin);
    requestLayout->addRow(QString::fromUtf8("允许测量误差"), m_toleranceSpin);
    requestLayout->addRow(QString::fromUtf8("工作距离"), m_wdSpin);
    requestLayout->addRow(QString::fromUtf8("高度波动"), m_heightVariationSpin);
    requestLayout->addRow(QString::fromUtf8("运动速度"), m_speedSpin);
    requestLayout->addRow(QString::fromUtf8("节拍/帧率"), m_fpsSpin);
    requestLayout->addRow(QString::fromUtf8("检测类型"), m_detectionCombo);
    requestLayout->addRow(QString::fromUtf8("表面材质"), m_surfaceCombo);
    requestLayout->addRow(QString(), m_reflectiveCheck);

    QGroupBox *cameraBox = new QGroupBox(QString::fromUtf8("手动相机参数"));
    QFormLayout *cameraLayout = new QFormLayout(cameraBox);
    cameraLayout->setLabelAlignment(Qt::AlignLeft);
    m_resolutionXSpin = dialogIntSpin(1, 200000, 2448);
    m_resolutionYSpin = dialogIntSpin(1, 200000, 2048);
    m_pixelSizeSpin = makeSpin(0.01, 1000.0, 3.45, QStringLiteral(" um"));
    m_bitDepthSpin = makeSpin(1.0, 32.0, 12.0, QStringLiteral(" bit"), 1);
    m_interfaceBandwidthSpin = makeSpin(0.0, 100000.0, 380.0, QStringLiteral(" MB/s"), 1);
    m_shutterCombo = new QComboBox;
    m_shutterCombo->addItems({QStringLiteral("Global"), QStringLiteral("Rolling")});
    cameraLayout->addRow(QString::fromUtf8("分辨率 X"), m_resolutionXSpin);
    cameraLayout->addRow(QString::fromUtf8("分辨率 Y"), m_resolutionYSpin);
    cameraLayout->addRow(QString::fromUtf8("像元"), m_pixelSizeSpin);
    cameraLayout->addRow(QString::fromUtf8("bit depth"), m_bitDepthSpin);
    cameraLayout->addRow(QString::fromUtf8("接口带宽"), m_interfaceBandwidthSpin);
    cameraLayout->addRow(QString::fromUtf8("快门"), m_shutterCombo);

    QGroupBox *lensBox = new QGroupBox(QString::fromUtf8("手动镜头参数"));
    QFormLayout *lensLayout = new QFormLayout(lensBox);
    lensLayout->setLabelAlignment(Qt::AlignLeft);
    m_lensModeCombo = new QComboBox;
    m_lensModeCombo->addItems({QString::fromUtf8("普通镜头"), QString::fromUtf8("远心镜头")});
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
    lensLayout->addRow(QString::fromUtf8("模式"), m_lensModeCombo);
    lensLayout->addRow(QString::fromUtf8("普通焦距"), m_focalSpin);
    lensLayout->addRow(QStringLiteral("F/#"), m_fNumberSpin);
    lensLayout->addRow(QString::fromUtf8("普通最小 WD"), m_minWdSpin);
    lensLayout->addRow(QString::fromUtf8("畸变"), m_distortionSpin);
    lensLayout->addRow(QString::fromUtf8("像面"), m_imageCircleSpin);
    lensLayout->addRow(QString::fromUtf8("镜头 MP"), m_lensMpSpin);
    lensLayout->addRow(QStringLiteral("PMAG"), m_pmagSpin);
    lensLayout->addRow(QString::fromUtf8("远心标称 WD"), m_nominalWdSpin);
    lensLayout->addRow(QString::fromUtf8("WD 容差"), m_wdToleranceSpin);
    lensLayout->addRow(QStringLiteral("DOF"), m_dofSpin);
    lensLayout->addRow(QString::fromUtf8("远心度"), m_telecentricitySpin);

    QGroupBox *lightBox = new QGroupBox(QString::fromUtf8("光源约束"));
    QFormLayout *lightLayout = new QFormLayout(lightBox);
    lightLayout->setLabelAlignment(Qt::AlignLeft);
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
    lightLayout->addRow(QString::fromUtf8("光型"), m_lightTypeCombo);
    lightLayout->addRow(QString::fromUtf8("模式"), m_lightModeCombo);
    lightLayout->addRow(QString::fromUtf8("有效宽度"), m_lightWidthSpin);
    lightLayout->addRow(QString::fromUtf8("有效高度"), m_lightHeightSpin);

    inputLayout->addWidget(requestBox);
    inputLayout->addWidget(cameraBox);
    inputLayout->addWidget(lensBox);
    inputLayout->addWidget(lightBox);
    inputLayout->addStretch();
    inputScroll->setWidget(inputPanel);

    QVBoxLayout *outputLayout = new QVBoxLayout;
    QHBoxLayout *actions = new QHBoxLayout;
    QPushButton *calculateButton = new QPushButton(QString::fromUtf8("计算"));
    QPushButton *resetButton = new QPushButton(QString::fromUtf8("恢复默认"));
    resetButton->setObjectName(QStringLiteral("SecondaryButton"));
    actions->addWidget(calculateButton);
    actions->addWidget(resetButton);
    actions->addStretch();
    outputLayout->addLayout(actions);

    m_output = new QTextEdit;
    m_output->setReadOnly(true);
    outputLayout->addWidget(m_output, 1);

    body->addWidget(inputScroll);
    body->addLayout(outputLayout, 1);
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
        ? QString::fromUtf8("无主要风险")
        : r.risks.join(QString::fromUtf8("；"));
    const QString reasons = r.reasons.isEmpty()
        ? QString::fromUtf8("按当前手动参数计算")
        : r.reasons.join(QString::fromUtf8("；"));
    const QString exposure = r.requirement.hasMotionConstraint
        ? QStringLiteral("%1 us").arg(r.requirement.maxExposureUsForOnePixelBlur, 0, 'f', 1)
        : QString::fromUtf8("无运动约束");

    QString html;
    html += QString::fromUtf8("<h3>纯计算结果</h3>");
    html += QString::fromUtf8("<h4>需求估算</h4>");
    html += QString::fromUtf8("<p>需求 FOV：<b>%1 x %2 mm</b>；目标物方像素：<b>%3 um/px</b>；最低分辨率：<b>%4 x %5</b>（%6 MP）；12 bit 原始带宽：<b>%7 MB/s</b>；曝光上限：<b>%8</b>。</p>")
        .arg(r.requirement.requiredFovWidthMm, 0, 'f', 2)
        .arg(r.requirement.requiredFovHeightMm, 0, 'f', 2)
        .arg(r.requirement.targetObjectPixelUm, 0, 'f', 2)
        .arg(r.requirement.requiredResolutionX)
        .arg(r.requirement.requiredResolutionY)
        .arg(r.requirement.requiredMegapixels, 0, 'f', 2)
        .arg(r.requirement.requiredBandwidthMBps12Bit, 0, 'f', 1)
        .arg(exposure);

    html += QString::fromUtf8("<h4>相机估算</h4>");
    html += QString::fromUtf8("<p>传感器：<b>%1 x %2 mm</b>，对角线 %3 mm；按需求 FOV 的物方像素：<b>%4 um/px</b>；单帧数据：%5 MB；吞吐：%6 MB/s；接口带宽：%7 MB/s；利用率：<b>%8%</b>；原始存储：<b>%9 GB/h</b>。</p>")
        .arg(r.sensorWidthMm, 0, 'f', 2)
        .arg(r.sensorHeightMm, 0, 'f', 2)
        .arg(r.sensorDiagonalMm, 0, 'f', 2)
        .arg(r.cameraObjectPixelSizeUm, 0, 'f', 2)
        .arg(r.framePayloadMB, 0, 'f', 2)
        .arg(r.bandwidthRequiredMBps, 0, 'f', 1)
        .arg(r.interfaceCapacityMBps, 0, 'f', 1)
        .arg(r.bandwidthUtilizationPercent, 0, 'f', 0)
        .arg(r.storagePerHourGB, 0, 'f', 0);

    html += QString::fromUtf8("<h4>镜头估算</h4>");
    html += QString::fromUtf8("<p>%1</p>").arg(r.lensFormulaSummary);
    if (pureInput.telecentricMode) {
        html += QString::fromUtf8("<p>远心 PMAG：<b>%1x</b>；实际 FOV：<b>%2 x %3 mm</b>；物方像素：<b>%4 um/px</b>；残余远心误差：<b>%5 um</b>；DOF：<b>%6 mm</b>；畸变边缘误差：<b>%7 um</b>。</p>")
            .arg(r.magnification, 0, 'f', 3)
            .arg(r.effectiveFovWidthMm, 0, 'f', 2)
            .arg(r.effectiveFovHeightMm, 0, 'f', 2)
            .arg(r.lensObjectPixelSizeUm, 0, 'f', 2)
            .arg(r.residualTelecentricErrorUm, 0, 'f', 2)
            .arg(r.estimatedDofMm, 0, 'f', 2)
            .arg(r.distortionErrorUm, 0, 'f', 2);
    } else {
        html += QString::fromUtf8("<p>目标焦距：<b>%1 mm</b>；输入焦距下实际 FOV：<b>%2 x %3 mm</b>；倍率：<b>%4x</b>；物方像素：<b>%5 um/px</b>；估算 DOF：<b>%6 mm</b>；畸变边缘误差：<b>%7 um</b>。</p>")
            .arg(r.targetFixedFocalLengthMm, 0, 'f', 2)
            .arg(r.effectiveFovWidthMm, 0, 'f', 2)
            .arg(r.effectiveFovHeightMm, 0, 'f', 2)
            .arg(r.magnification, 0, 'f', 3)
            .arg(r.lensObjectPixelSizeUm, 0, 'f', 2)
            .arg(r.estimatedDofMm, 0, 'f', 2)
            .arg(r.distortionErrorUm, 0, 'f', 2);
    }

    html += QString::fromUtf8("<h4>光源估算</h4>");
    html += QString::fromUtf8("<p>建议有效照明面积：<b>%1 x %2 mm</b>；当前光源覆盖余量：<b>%3%</b>。</p>")
        .arg(r.suggestedLightWidthMm, 0, 'f', 1)
        .arg(r.suggestedLightHeightMm, 0, 'f', 1)
        .arg(r.lightCoverageMarginPercent, 0, 'f', 0);
    html += QString::fromUtf8("<h4>判断</h4>");
    html += QString::fromUtf8("<p><b>依据：</b>%1</p>").arg(reasons);
    html += QString::fromUtf8("<p><b>风险：</b>%1</p>").arg(risks);
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
