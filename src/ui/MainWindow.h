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
class QSpinBox;
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
    QTextEdit *m_pureCalculationOutput = nullptr;
    QTableWidget *m_resultTable = nullptr;
    QTextEdit *m_resultDetails = nullptr;
    QTableWidget *m_compareTable = nullptr;
    QTextEdit *m_compareDetails = nullptr;
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

    QDoubleSpinBox *m_pureWidthSpin = nullptr;
    QDoubleSpinBox *m_pureHeightSpin = nullptr;
    QDoubleSpinBox *m_pureMarginSpin = nullptr;
    QDoubleSpinBox *m_pureMinFeatureSpin = nullptr;
    QDoubleSpinBox *m_pureToleranceSpin = nullptr;
    QDoubleSpinBox *m_pureWdSpin = nullptr;
    QDoubleSpinBox *m_pureHeightVariationSpin = nullptr;
    QDoubleSpinBox *m_pureSpeedSpin = nullptr;
    QDoubleSpinBox *m_pureFpsSpin = nullptr;
    QComboBox *m_pureDetectionCombo = nullptr;
    QComboBox *m_pureSurfaceCombo = nullptr;
    QCheckBox *m_pureReflectiveCheck = nullptr;
    QSpinBox *m_pureResolutionXSpin = nullptr;
    QSpinBox *m_pureResolutionYSpin = nullptr;
    QDoubleSpinBox *m_purePixelSizeSpin = nullptr;
    QDoubleSpinBox *m_pureBitDepthSpin = nullptr;
    QDoubleSpinBox *m_pureInterfaceBandwidthSpin = nullptr;
    QComboBox *m_pureShutterCombo = nullptr;
    QComboBox *m_pureLensModeCombo = nullptr;
    QDoubleSpinBox *m_pureFocalSpin = nullptr;
    QDoubleSpinBox *m_pureFNumberSpin = nullptr;
    QDoubleSpinBox *m_pureMinWdSpin = nullptr;
    QDoubleSpinBox *m_pureDistortionSpin = nullptr;
    QDoubleSpinBox *m_pureImageCircleSpin = nullptr;
    QDoubleSpinBox *m_pureLensMpSpin = nullptr;
    QDoubleSpinBox *m_purePmagSpin = nullptr;
    QDoubleSpinBox *m_pureNominalWdSpin = nullptr;
    QDoubleSpinBox *m_pureWdToleranceSpin = nullptr;
    QDoubleSpinBox *m_pureDofSpin = nullptr;
    QDoubleSpinBox *m_pureTelecentricitySpin = nullptr;
    QComboBox *m_pureLightTypeCombo = nullptr;
    QComboBox *m_pureLightModeCombo = nullptr;
    QDoubleSpinBox *m_pureLightWidthSpin = nullptr;
    QDoubleSpinBox *m_pureLightHeightSpin = nullptr;

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
    QWidget *createPureCalculationPage();
    QWidget *createCalculationPage();
    QWidget *createResultsPage();
    QWidget *createComparisonPage();
    QWidget *createCatalogPage();
    QWidget *createReportPage();
    QDoubleSpinBox *makeSpin(double min, double max, double value, const QString &suffix, int decimals = 2);

    void setActivePage(int index);
    void calculate();
    SelectionRequest readRequest() const;
    PureCalculationInput readPureCalculationInput() const;
    void refreshPureCalculation();
    void refreshCalculationAssistant();
    void refreshAssistantLensTable();
    void refreshResultTable();
    void refreshResultDetails(int row);
    void refreshComparisonPage();
    void refreshComparisonDetails(int row);
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
