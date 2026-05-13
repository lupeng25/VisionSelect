#ifndef CALCULATIONPAGE_H
#define CALCULATIONPAGE_H

#include "selection/CalculationAssistant.h"

#include <QWidget>
#include <QVector>

class QLabel;
class QTableWidget;
class QTextEdit;

class CalculationPage : public QWidget
{
    Q_OBJECT

public:
    using CameraEstimate = CameraCalculationEstimate;
    using LensEstimate = LensCalculationEstimate;

    explicit CalculationPage(QWidget *parent = nullptr);

    void setSummary(const QString &text);
    void setCameraEstimates(const QVector<CameraEstimate> &estimates);
    void setLensEstimates(const QVector<LensEstimate> &estimates);
    void setDetails(const QString &text);
    int selectedCameraEstimateRow() const;

signals:
    void inputRequested();
    void recalculateRequested();
    void cameraSelectionChanged(int row);

private:
    QLabel *m_summaryLabel = nullptr;
    QTableWidget *m_cameraTable = nullptr;
    QTableWidget *m_lensTable = nullptr;
    QTextEdit *m_details = nullptr;
    int m_selectedCameraEstimateRow = -1;
};

#endif
