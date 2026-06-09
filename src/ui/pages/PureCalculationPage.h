#ifndef PURECALCULATIONPAGE_H
#define PURECALCULATIONPAGE_H

#include "selection/CalculationAssistant.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFrame;
class QSpinBox;
class QTextEdit;

class PureCalculationPage : public QWidget
{
    Q_OBJECT

public:
    explicit PureCalculationPage(QWidget *parent = nullptr);
    PureCalculationInput input() const;

public slots:
    void refresh();
    void resetDefaults();

private:
    void updateLensParameterVisibility();

    QDoubleSpinBox *m_widthSpin = nullptr;
    QDoubleSpinBox *m_heightSpin = nullptr;
    QDoubleSpinBox *m_marginSpin = nullptr;
    QDoubleSpinBox *m_minFeatureSpin = nullptr;
    QDoubleSpinBox *m_toleranceSpin = nullptr;
    QDoubleSpinBox *m_wdSpin = nullptr;
    QDoubleSpinBox *m_heightVariationSpin = nullptr;
    QDoubleSpinBox *m_speedSpin = nullptr;
    QDoubleSpinBox *m_fpsSpin = nullptr;
    QComboBox *m_detectionCombo = nullptr;
    QComboBox *m_surfaceCombo = nullptr;
    QCheckBox *m_reflectiveCheck = nullptr;
    QSpinBox *m_resolutionXSpin = nullptr;
    QSpinBox *m_resolutionYSpin = nullptr;
    QDoubleSpinBox *m_pixelSizeSpin = nullptr;
    QDoubleSpinBox *m_bitDepthSpin = nullptr;
    QDoubleSpinBox *m_interfaceBandwidthSpin = nullptr;
    QComboBox *m_shutterCombo = nullptr;
    QComboBox *m_lensModeCombo = nullptr;
    QFrame *m_fixedLensGroup = nullptr;
    QFrame *m_telecentricLensGroup = nullptr;
    QDoubleSpinBox *m_focalSpin = nullptr;
    QDoubleSpinBox *m_fNumberSpin = nullptr;
    QDoubleSpinBox *m_minWdSpin = nullptr;
    QDoubleSpinBox *m_distortionSpin = nullptr;
    QDoubleSpinBox *m_imageCircleSpin = nullptr;
    QDoubleSpinBox *m_lensMpSpin = nullptr;
    QDoubleSpinBox *m_teleFNumberSpin = nullptr;
    QDoubleSpinBox *m_teleDistortionSpin = nullptr;
    QDoubleSpinBox *m_teleImageCircleSpin = nullptr;
    QDoubleSpinBox *m_teleLensMpSpin = nullptr;
    QDoubleSpinBox *m_pmagSpin = nullptr;
    QDoubleSpinBox *m_nominalWdSpin = nullptr;
    QDoubleSpinBox *m_wdToleranceSpin = nullptr;
    QDoubleSpinBox *m_dofSpin = nullptr;
    QDoubleSpinBox *m_telecentricitySpin = nullptr;
    QComboBox *m_lightTypeCombo = nullptr;
    QComboBox *m_lightModeCombo = nullptr;
    QDoubleSpinBox *m_lightWidthSpin = nullptr;
    QDoubleSpinBox *m_lightHeightSpin = nullptr;
    QTextEdit *m_output = nullptr;
};

#endif
