#ifndef INPUTPAGE_H
#define INPUTPAGE_H

#include "core/SelectionTypes.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSplitter;
class QTextEdit;

class InputPage : public QWidget
{
    Q_OBJECT

public:
    explicit InputPage(QWidget *parent = nullptr);
    ~InputPage() override;
    SelectionRequest request() const;
    void setRequest(const SelectionRequest &request);
    void setBusy(bool busy);

signals:
    void runSelectionRequested();

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
    QComboBox *m_motionModeCombo = nullptr;
    QCheckBox *m_reflectiveCheck = nullptr;
    QCheckBox *m_monoCheck = nullptr;
    QCheckBox *m_allowTelecentricCheck = nullptr;
    QTextEdit *m_notesEdit = nullptr;
    QLabel *m_fovSummaryLabel = nullptr;
    QLabel *m_pixelSummaryLabel = nullptr;
    QLabel *m_resolutionSummaryLabel = nullptr;
    QLabel *m_bandwidthSummaryLabel = nullptr;
    QLabel *m_workDistanceSummaryLabel = nullptr;
    QLabel *m_fpsSummaryLabel = nullptr;
    QLabel *m_detectionSummaryLabel = nullptr;
    QLabel *m_surfaceSummaryLabel = nullptr;
    QLabel *m_exposureSummaryLabel = nullptr;
    QLabel *m_processSummaryLabel = nullptr;
    QSplitter *m_splitter = nullptr;
    QPushButton *m_runButton = nullptr;
};

#endif
