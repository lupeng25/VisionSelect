#include "ui/pages/InputPage.h"

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
#include <QVBoxLayout>

using namespace UiHelpers;

InputPage::InputPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(28, 24, 28, 24);
    outer->setSpacing(16);
    outer->addWidget(pageTitle(localizedText("需求输入", "Requirement Input")));

    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *content = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 12, 0);
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
    QPushButton *calculateButton = new QPushButton(localizedText("计算推荐", "Calculate Recommendations"));
    QPushButton *resultButton = new QPushButton(localizedText("查看结果", "View Results"));
    resultButton->setObjectName(QStringLiteral("SecondaryButton"));
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
    outer->addWidget(scroll, 1);
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
}
