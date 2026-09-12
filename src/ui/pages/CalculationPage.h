#ifndef CALCULATIONPAGE_H
#define CALCULATIONPAGE_H

#include "selection/CalculationAssistant.h"

#include <QWidget>
#include <QVector>

class QLabel;
class QTableWidget;
class QTextEdit;
class QSplitter;
class QPushButton;

class CalculationPage : public QWidget
{
    Q_OBJECT

public:
    using CameraEstimate = CameraCalculationEstimate;
    using LensEstimate = LensCalculationEstimate;

    explicit CalculationPage(QWidget *parent = nullptr);
    ~CalculationPage() override;

    void setSummary(const QString &text);
    void setCameraEstimates(const QVector<CameraEstimate> &estimates, const CameraSpec *initialCamera = nullptr);
    void setLensEstimates(const QVector<LensEstimate> &estimates);
    void setDetails(const QString &text);
    int selectedCameraEstimateRow() const;

signals:
    void inputRequested();
    void recalculateRequested();
    void cameraSelectionChanged(int row);

private:
    void refreshCameraSummary();
    void refreshLensDetails();
    QVector<CameraEstimate> m_cameras;
    QVector<LensEstimate> m_lenses;
    QString m_requirementDetails;
    QLabel *m_cameraSummary = nullptr;
    QLabel *m_lensSummary = nullptr;
    QLabel *m_selectionSummary = nullptr;
    QPushButton *m_detailsButton = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QTableWidget *m_cameraTable = nullptr;
    QTableWidget *m_lensTable = nullptr;
    QTextEdit *m_details = nullptr;
    QSplitter *m_splitter = nullptr;
    int m_selectedCameraEstimateRow = -1;
};

#endif
