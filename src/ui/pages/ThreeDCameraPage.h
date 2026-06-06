#ifndef THREEDCAMERAPAGE_H
#define THREEDCAMERAPAGE_H

#include "three_d/ThreeDCalculation.h"
#include "three_d/ThreeDCameraMatcher.h"
#include "three_d/ThreeDCameraRepository.h"

#include <QWidget>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLayout;
class QSpinBox;
class QTableWidget;
class QTextBrowser;
class QTextEdit;

class ThreeDCameraPage : public QWidget
{
    Q_OBJECT

public:
    explicit ThreeDCameraPage(QWidget *parent = nullptr);
    void activate();

private:
    ThreeDCameraRepository m_repository;
    QVector<ThreeDCameraMatch> m_matches;
    bool m_loaded = false;
    bool m_resultsInitialized = false;
    int m_selectedMatchIndex = -1;

    QLabel *m_summaryLabel = nullptr;
    QComboBox *m_brandCombo = nullptr;
    QComboBox *m_technologyCombo = nullptr;
    QComboBox *m_interfaceCombo = nullptr;
    QComboBox *m_ipCombo = nullptr;
    QComboBox *m_materialCombo = nullptr;
    QDoubleSpinBox *m_xCoverageSpin = nullptr;
    QDoubleSpinBox *m_yCoverageSpin = nullptr;
    QDoubleSpinBox *m_zRangeSpin = nullptr;
    QDoubleSpinBox *m_workingDistanceSpin = nullptr;
    QDoubleSpinBox *m_zRepeatabilitySpin = nullptr;
    QDoubleSpinBox *m_speedSpin = nullptr;
    QCheckBox *m_noMotionCheck = nullptr;
    QCheckBox *m_encoderCheck = nullptr;
    QTableWidget *m_table = nullptr;
    QTextBrowser *m_details = nullptr;
    QLabel *m_samplingCameraLabel = nullptr;
    QDoubleSpinBox *m_scanDistanceSpin = nullptr;
    QDoubleSpinBox *m_profileIntervalSpin = nullptr;
    QDoubleSpinBox *m_axisTravelSpin = nullptr;
    QSpinBox *m_pulseCountSpin = nullptr;
    QSpinBox *m_refinementPointsSpin = nullptr;
    QDoubleSpinBox *m_samplingRateSpin = nullptr;
    QDoubleSpinBox *m_safetyFactorSpin = nullptr;
    QDoubleSpinBox *m_xPitchOverrideSpin = nullptr;
    QTextEdit *m_samplingOutput = nullptr;

    void buildFilters(QLayout *parentLayout);
    void buildSamplingPanel(QLayout *parentLayout);
    bool ensureLoaded();
    void populateFilterOptions();
    ThreeDCameraRequirement requirement() const;
    ThreeDMotionSamplingInput samplingInput() const;
    const ThreeDCameraSpec *selectedCameraSpec() const;
    void refresh();
    void refreshSampling();
    void resetSamplingDefaults();
    void clearFilters();
    void fillTable();
    void showDetailsForRow(int row);
    void updateSamplingFromSelected(int row);
};

#endif
