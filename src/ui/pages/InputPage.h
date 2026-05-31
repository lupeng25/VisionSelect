#ifndef INPUTPAGE_H
#define INPUTPAGE_H

#include "core/SelectionTypes.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;

class InputPage : public QWidget
{
    Q_OBJECT

public:
    explicit InputPage(QWidget *parent = nullptr);
    SelectionRequest request() const;
    void setRequest(const SelectionRequest &request);

signals:
    void calculateRequested();
    void resultsRequested();

private:
    void refreshSummary();

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
    QCheckBox *m_monoCheck = nullptr;
    QCheckBox *m_allowTelecentricCheck = nullptr;
    QLabel *m_fovSummaryLabel = nullptr;
    QLabel *m_pixelSummaryLabel = nullptr;
    QLabel *m_resolutionSummaryLabel = nullptr;
    QLabel *m_processSummaryLabel = nullptr;
};

#endif
