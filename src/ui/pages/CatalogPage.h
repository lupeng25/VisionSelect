#ifndef CATALOGPAGE_H
#define CATALOGPAGE_H

#include "catalog/CatalogRepository.h"

#include <QVector>
#include <QWidget>

class CatalogRepository;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTableView;
class QTimer;
class CatalogTableModel;

class CatalogPage : public QWidget
{
    Q_OBJECT

public:
    explicit CatalogPage(QWidget *parent = nullptr);

    void setCatalog(const CatalogRepository *catalog);
    void refreshCurrentPage();

    int selectedCameraCatalogIndex() const;
    int selectedLensCatalogIndex() const;
    int selectedLightCatalogIndex() const;
    qint64 selectedCameraId() const;
    qint64 selectedLensId() const;
    qint64 selectedLightId() const;
    CatalogQuery currentCameraQuery() const;
    CatalogQuery currentLensQuery() const;
    CatalogQuery currentLightQuery() const;
    QVector<int> visibleCameraCatalogIndexes() const;
    QVector<int> visibleLensCatalogIndexes() const;
    QVector<int> visibleLightCatalogIndexes() const;

signals:
    void cameraAddRequested();
    void cameraEditRequested();
    void cameraRemoveRequested();
    void cameraImportRequested();
    void cameraExportRequested();
    void cameraExportFilteredRequested();
    void cameraResetRequested();

    void lensAddRequested();
    void lensEditRequested();
    void lensRemoveRequested();
    void lensImportRequested();
    void lensExportRequested();
    void lensExportFilteredRequested();
    void lensResetRequested();

    void lightAddRequested();
    void lightEditRequested();
    void lightRemoveRequested();
    void lightImportRequested();
    void lightExportRequested();
    void lightExportFilteredRequested();
    void lightResetRequested();

private:
    static const int kPageSize = 500;

    const CatalogRepository *m_catalog = nullptr;

    QTabWidget *m_tabs = nullptr;
    QTableView *m_cameraTable = nullptr;
    QTableView *m_lensTable = nullptr;
    QTableView *m_lightTable = nullptr;
    CatalogTableModel *m_cameraModel = nullptr;
    CatalogTableModel *m_lensModel = nullptr;
    CatalogTableModel *m_lightModel = nullptr;

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

    QLabel *m_cameraPageLabel = nullptr;
    QLabel *m_lensPageLabel = nullptr;
    QLabel *m_lightPageLabel = nullptr;
    QPushButton *m_cameraPrevButton = nullptr;
    QPushButton *m_cameraNextButton = nullptr;
    QSpinBox *m_cameraPageSpin = nullptr;
    QPushButton *m_lensPrevButton = nullptr;
    QPushButton *m_lensNextButton = nullptr;
    QSpinBox *m_lensPageSpin = nullptr;
    QPushButton *m_lightPrevButton = nullptr;
    QPushButton *m_lightNextButton = nullptr;
    QSpinBox *m_lightPageSpin = nullptr;
    QTimer *m_filterTimer = nullptr;
    int m_cameraOffset = 0;
    int m_lensOffset = 0;
    int m_lightOffset = 0;
    int m_cameraTotal = 0;
    int m_lensTotal = 0;
    int m_lightTotal = 0;

    void refreshCatalogFilterOptions();
    void refreshCameraTable();
    void refreshLensTable();
    void refreshLightTable();
    void scheduleCurrentRefresh();
    void updatePageLabel(CatalogDomain domain);
    qint64 selectedId(const QTableView *view, const CatalogTableModel *model) const;
    int selectedIndex(CatalogDomain domain) const;
    void clearCameraFilters();
    void clearLensFilters();
    void clearLightFilters();
};

#endif
