#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "catalog/CatalogRepository.h"
#include "core/SelectionTypes.h"
#include "selection/CalculationAssistant.h"

#include <QMainWindow>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTextEdit;

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
    QLabel *m_assistantSummaryLabel = nullptr;
    QLabel *m_resultSummaryLabel = nullptr;
    QTableWidget *m_assistantCameraTable = nullptr;
    QTableWidget *m_assistantLensTable = nullptr;
    QTextEdit *m_assistantDetails = nullptr;
    QTableWidget *m_resultTable = nullptr;
    QTextEdit *m_resultDetails = nullptr;
    QTableWidget *m_cameraTable = nullptr;
    QTableWidget *m_lensTable = nullptr;
    QTableWidget *m_lightTable = nullptr;
    QTextEdit *m_reportPreview = nullptr;

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

    QVector<CameraCalculationEstimate> m_assistantCameraEstimates;
    QVector<LensCalculationEstimate> m_assistantLensEstimates;
    int m_assistantSelectedCameraRow = -1;

    QLineEdit *m_cameraSearchEdit = nullptr;
    QComboBox *m_cameraManufacturerFilter = nullptr;
    QComboBox *m_cameraInterfaceFilter = nullptr;
    QLineEdit *m_lensSearchEdit = nullptr;
    QComboBox *m_lensManufacturerFilter = nullptr;
    QComboBox *m_lensTypeFilter = nullptr;
    QComboBox *m_lensMountFilter = nullptr;
    QLineEdit *m_lightSearchEdit = nullptr;
    QComboBox *m_lightManufacturerFilter = nullptr;
    QComboBox *m_lightTypeFilter = nullptr;
    QComboBox *m_lightModeFilter = nullptr;
    QVector<int> m_cameraRowMap;
    QVector<int> m_lensRowMap;
    QVector<int> m_lightRowMap;

    void buildUi();
    QWidget *createSidebar();
    QWidget *createInputPage();
    QWidget *createCalculationPage();
    QWidget *createResultsPage();
    QWidget *createCatalogPage();
    QWidget *createReportPage();
    QDoubleSpinBox *makeSpin(double min, double max, double value, const QString &suffix, int decimals = 2);

    void setActivePage(int index);
    void calculate();
    SelectionRequest readRequest() const;
    void refreshCalculationAssistant();
    void refreshAssistantLensTable();
    void refreshResultTable();
    void refreshResultDetails(int row);
    void refreshCatalogTables();
    void refreshCatalogFilterOptions();
    void refreshCameraTable();
    void refreshLensTable();
    void refreshLightTable();
    void refreshReportPreview();
    void importCameras();
    void importLenses();
    void importLights();
    void exportCameras();
    void exportLenses();
    void exportLights();
    void exportFilteredCameras();
    void exportFilteredLenses();
    void exportFilteredLights();
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
    void clearCameraFilters();
    void clearLensFilters();
    void clearLightFilters();
    int selectedCameraCatalogIndex() const;
    int selectedLensCatalogIndex() const;
    int selectedLightCatalogIndex() const;
    QVector<int> visibleCameraCatalogIndexes() const;
    QVector<int> visibleLensCatalogIndexes() const;
    QVector<int> visibleLightCatalogIndexes() const;
    void exportPdf();
    void showError(const QString &message);
};

#endif
