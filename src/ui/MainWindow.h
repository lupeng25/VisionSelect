#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "catalog/CatalogRepository.h"
#include "core/SelectionTypes.h"

#include <QMainWindow>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
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
    QLabel *m_resultSummaryLabel = nullptr;
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

    void buildUi();
    QWidget *createSidebar();
    QWidget *createInputPage();
    QWidget *createResultsPage();
    QWidget *createCatalogPage();
    QWidget *createReportPage();
    QDoubleSpinBox *makeSpin(double min, double max, double value, const QString &suffix, int decimals = 2);

    void setActivePage(int index);
    void calculate();
    SelectionRequest readRequest() const;
    void refreshResultTable();
    void refreshResultDetails(int row);
    void refreshCatalogTables();
    void refreshReportPreview();
    void importCameras();
    void importLenses();
    void importLights();
    void exportPdf();
    void showError(const QString &message);
};

#endif
