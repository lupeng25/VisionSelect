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
#include <optional>

class QComboBox;
class QCloseEvent;
class QEvent;
class QFrame;
class InputPage;
class QLabel;
class PureCalculationPage;
class QPushButton;
class QResizeEvent;
class ResultsPage;
class QStackedWidget;
class ThreeDCameraPage;
class QToolButton;
class QWidget;

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
    const SelectionJobResult &selectionSnapshot() const { return m_selection; }

protected:
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    CatalogRepository m_catalog;
    SelectionJobResult m_selection;
    QFutureWatcher<SelectionJobResult> *m_selectionWatcher = nullptr;
    bool m_hasPendingSelectionRequest = false;
    bool m_selectionCompleted = false;
    SelectionRequest m_pendingSelectionRequest;

    QStackedWidget *m_pages = nullptr;
    int m_activePageIndex = -1;
    QVector<QPushButton *> m_navButtons;
    QFrame *m_sidebar = nullptr;
    QWidget *m_topBar = nullptr;
    QToolButton *m_sidebarToggleButton = nullptr;
    QToolButton *m_interfaceButton = nullptr;
    QToolButton *m_maximizeButton = nullptr;
    bool m_sidebarForcedCollapsed = false;

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
    QLabel *m_topProductLabel = nullptr;
    QLabel *m_topPageLabel = nullptr;
    QVector<QLabel *> m_workflowStepLabels;
    bool m_catalogPageInitialized = false;

    CalculationPage *m_calculationPage = nullptr;
    CatalogPage *m_catalogPage = nullptr;
    ResultsPage *m_resultsPage = nullptr;
    InputPage *m_inputPage = nullptr;
    PureCalculationPage *m_pureCalculationPage = nullptr;
    ThreeDCameraPage *m_threeDCameraPage = nullptr;

    QVector<CameraCalculationEstimate> m_assistantCameraEstimates;
    QVector<LensCalculationEstimate> m_assistantLensEstimates;
    QVector<LensSpec> m_assistantLensCandidates;
    std::optional<SelectionRequest> m_assistantRequest;
    int m_assistantSelectedCameraRow = -1;

    void buildUi();
    void updateWindowMask();
    void updateWindowControlState();
    void updateSidebarLayout();
    void applyDensity();
    void runSelectionAndShowResults();
    void restorePersistentState(QWidget *root);
    void savePersistentState(QWidget *root) const;
    QWidget *createTopBar();
    QWidget *createSidebar();
    QWidget *createStatusBar();
    QStringList navigationLabels() const;
    QStringList railNavigationLabels() const;
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
    void refreshCalculationAssistant(bool force = false);
    void refreshAssistantLensTable();
    void refreshCatalogTables();
    void handleCatalogMutation();
    void importCatalog(CatalogDomain domain);
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
