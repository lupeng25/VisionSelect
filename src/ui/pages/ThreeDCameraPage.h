#ifndef THREEDCAMERAPAGE_H
#define THREEDCAMERAPAGE_H

#include "three_d/ThreeDCameraMatcher.h"
#include "three_d/ThreeDCameraRepository.h"

#include <QWidget>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLayout;
class QTableWidget;
class QTextBrowser;

class ThreeDCameraPage : public QWidget
{
    Q_OBJECT

public:
    explicit ThreeDCameraPage(QWidget *parent = nullptr);

private:
    ThreeDCameraRepository m_repository;
    QVector<ThreeDCameraMatch> m_matches;

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

    void buildFilters(QLayout *parentLayout);
    void populateFilterOptions();
    ThreeDCameraRequirement requirement() const;
    void refresh();
    void clearFilters();
    void fillTable();
    void showDetailsForRow(int row);
};

#endif
