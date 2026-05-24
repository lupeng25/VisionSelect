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
    outer->addWidget(pageTitle(QString::fromUtf8("\351\234\200\346\261\202\350\276\223\345\205\245")));

    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *content = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 12, 0);
    layout->setSpacing(14);

    QGroupBox *objectBox = new QGroupBox(QString::fromUtf8("\345\267\245\344\273\266\344\270\216\347\262\276\345\272\246"));
    QFormLayout *objectLayout = new QFormLayout(objectBox);
    objectLayout->setLabelAlignment(Qt::AlignLeft);
    m_widthSpin = makeSpin(0.1, 2000.0, 20.0, QStringLiteral(" mm"));
    m_heightSpin = makeSpin(0.1, 2000.0, 20.0, QStringLiteral(" mm"));
    m_marginSpin = makeSpin(0.0, 200.0, 2.0, QStringLiteral(" mm"));
    m_minFeatureSpin = makeSpin(0.1, 10000.0, 50.0, QStringLiteral(" um"));
    m_toleranceSpin = makeSpin(0.1, 10000.0, 10.0, QStringLiteral(" um"));
    objectLayout->addRow(QString::fromUtf8("\345\267\245\344\273\266\345\256\275\345\272\246"), m_widthSpin);
    objectLayout->addRow(QString::fromUtf8("\345\267\245\344\273\266\351\253\230\345\272\246"), m_heightSpin);
    objectLayout->addRow(QString::fromUtf8("\345\256\232\344\275\215/\350\243\205\345\244\271\344\275\231\351\207\217"), m_marginSpin);
    objectLayout->addRow(QString::fromUtf8("\346\234\200\345\260\217\347\211\271\345\276\201\345\260\272\345\257\270"), m_minFeatureSpin);
    objectLayout->addRow(QString::fromUtf8("\345\205\201\350\256\270\346\265\213\351\207\217\350\257\257\345\267\256"), m_toleranceSpin);

    QGroupBox *processBox = new QGroupBox(QString::fromUtf8("\345\267\245\350\211\272\344\270\216\345\256\211\350\243\205"));
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
    m_reflectiveCheck = new QCheckBox(QString::fromUtf8("\345\217\215\345\205\211/\351\253\230\345\205\211\350\241\250\351\235\242"));
    m_reflectiveCheck->setChecked(true);
    m_monoCheck = new QCheckBox(QString::fromUtf8("\344\274\230\345\205\210\351\273\221\347\231\275\347\233\270\346\234\272"));
    m_monoCheck->setChecked(true);
    m_allowTelecentricCheck = new QCheckBox(QString::fromUtf8("\345\205\201\350\256\270\350\277\234\345\277\203\351\225\234\345\244\264"));
    m_allowTelecentricCheck->setChecked(true);
    processLayout->addRow(QString::fromUtf8("\346\243\200\346\265\213\347\261\273\345\236\213"), m_detectionCombo);
    processLayout->addRow(QString::fromUtf8("\350\241\250\351\235\242\346\235\220\350\264\250"), m_surfaceCombo);
    processLayout->addRow(QString::fromUtf8("\345\267\245\344\275\234\350\267\235\347\246\273"), m_wdSpin);
    processLayout->addRow(QString::fromUtf8("\351\253\230\345\272\246\346\263\242\345\212\250"), m_heightVariationSpin);
    processLayout->addRow(QString::fromUtf8("\350\277\220\345\212\250\351\200\237\345\272\246"), m_speedSpin);
    processLayout->addRow(QString::fromUtf8("\350\212\202\346\213\215/\345\270\247\347\216\207"), m_fpsSpin);
    processLayout->addRow(QString(), m_reflectiveCheck);
    processLayout->addRow(QString(), m_monoCheck);
    processLayout->addRow(QString(), m_allowTelecentricCheck);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    QPushButton *calculateButton = new QPushButton(QString::fromUtf8("\350\256\241\347\256\227\346\216\250\350\215\220"));
    QPushButton *resultButton = new QPushButton(QString::fromUtf8("\346\237\245\347\234\213\347\273\223\346\236\234"));
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
