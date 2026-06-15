#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "catalog/CatalogRepository.h"
#include "core/SelectionTypes.h"
#include "selection/CalculationAssistant.h"
#include "ui/pages/CalculationPage.h"
#include "ui/pages/CatalogPage.h"

#include <QFutureWatcher>
#include <QMainWindow>
#include <QString>
#include <QVector>

class QComboBox;
class InputPage;
class QLabel;
class PureCalculationPage;
class QPushButton;
class ResultsPage;
class QStackedWidget;
class ThreeDCameraPage;

struct SelectionJobResult {
    SelectionRequest request;
    QVector<SelectionResult> results;
    QString error;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    CatalogRepository m_catalog;
    QVector<SelectionResult> m_results;
    SelectionRequest m_request;
    QFutureWatcher<SelectionJobResult> *m_selectionWatcher = nullptr;
    bool m_hasPendingSelectionRequest = false;
    SelectionRequest m_pendingSelectionRequest;

    QStackedWidget *m_pages = nullptr;
    QVector<QPushButton *> m_navButtons;

    QLabel *m_summaryLabel = nullptr;
    QLabel *m_brandSubtitleLabel = nullptr;
    QLabel *m_brandBadgeLabel = nullptr;
    QLabel *m_navTitleLabel = nullptr;
    QVector<QLabel *> m_navSectionLabels;
    QLabel *m_summaryTitleLabel = nullptr;
    QComboBox *m_languageCombo = nullptr;
    QPushButton *m_licenseButton = nullptr;
    QLabel *m_summaryStatusLabel = nullptr;
    QLabel *m_cameraCountLabel = nullptr;
    QLabel *m_lensCountLabel = nullptr;
    QLabel *m_lightCountLabel = nullptr;
    QLabel *m_languageLabel = nullptr;
    bool m_catalogPageInitialized = false;

    CalculationPage *m_calculationPage = nullptr;
    CatalogPage *m_catalogPage = nullptr;
    ResultsPage *m_resultsPage = nullptr;
    InputPage *m_inputPage = nullptr;
    PureCalculationPage *m_pureCalculationPage = nullptr;
    ThreeDCameraPage *m_threeDCameraPage = nullptr;

    QVector<CameraCalculationEstimate> m_assistantCameraEstimates;
    QVector<LensCalculationEstimate> m_assistantLensEstimates;
    int m_assistantSelectedCameraRow = -1;

    void buildUi();
    QWidget *createSidebar();
    QStringList navigationLabels() const;
    void retranslateUi();
    void rebuildPagesForLanguage();
    void syncLanguageCombo();
    void showLicenseInfo();
    void refreshSidebarSummary();

    void ensurePureCalculationPage();
    void ensureCalculationPage();
    void ensureResultsPage();
    void ensureCatalogPage();
    void ensureThreeDCameraPage();
    void ensureCatalogPageInitialized();
    void setActivePage(int index);
    void calculate();
    void startSelectionCalculation(const SelectionRequest &request);
    void finishSelectionCalculation();
    bool selectionCalculationRunning() const;
    void refreshCalculationAssistant();
    void refreshAssistantLensTable();
    void refreshCatalogTables();
    void handleCatalogMutation();
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
