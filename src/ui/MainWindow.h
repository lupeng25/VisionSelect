#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "catalog/CatalogRepository.h"
#include "core/SelectionTypes.h"
#include "selection/CalculationAssistant.h"
#include "ui/pages/CalculationPage.h"
#include "ui/pages/CatalogPage.h"

#include <QMainWindow>
#include <QVector>

class ComparisonPage;
class InputPage;
class QLabel;
class PureCalculationPage;
class QPushButton;
class ReportPage;
class ResultsPage;
class QStackedWidget;
class ThreeDCameraPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    CatalogRepository m_catalog;
    QVector<SelectionResult> m_results;
    SelectionRequest m_request;

    QStackedWidget *m_pages = nullptr;
    QVector<QPushButton *> m_navButtons;

    QLabel *m_summaryLabel = nullptr;
    bool m_catalogPageInitialized = false;

    CalculationPage *m_calculationPage = nullptr;
    CatalogPage *m_catalogPage = nullptr;
    ResultsPage *m_resultsPage = nullptr;
    ComparisonPage *m_comparisonPage = nullptr;
    InputPage *m_inputPage = nullptr;
    PureCalculationPage *m_pureCalculationPage = nullptr;
    ReportPage *m_reportPage = nullptr;
    ThreeDCameraPage *m_threeDCameraPage = nullptr;

    QVector<CameraCalculationEstimate> m_assistantCameraEstimates;
    QVector<LensCalculationEstimate> m_assistantLensEstimates;
    int m_assistantSelectedCameraRow = -1;

    void buildUi();
    QWidget *createSidebar();

    void ensurePureCalculationPage();
    void ensureCalculationPage();
    void ensureResultsPage();
    void ensureComparisonPage();
    void ensureCatalogPage();
    void ensureReportPage();
    void ensureThreeDCameraPage();
    void ensureCatalogPageInitialized();
    void setActivePage(int index);
    void calculate();
    void refreshCalculationAssistant();
    void refreshAssistantLensTable();
    void refreshCatalogTables();
    void importCameras();
    void importLenses();
    void importLights();
    void exportCameras();
    void exportLenses();
    void exportLights();
    void exportFilteredCameras();
    void exportFilteredLenses();
    void exportFilteredLights();
    void exportBomCsv();
    void addCamera();
    void editCamera();
    void removeCamera();
    void addLens();
    void editLens();
    void removeLens();
    void addLight();
    void editLight();
    void removeLight();
    void resetCameras();
    void resetLenses();
    void resetLights();
    void exportReportPdf();
    void showError(const QString &message);
};

#endif
