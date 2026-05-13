#ifndef CATALOGPAGE_H
#define CATALOGPAGE_H

#include <QVector>
#include <QWidget>

class CatalogRepository;
class QComboBox;
class QLineEdit;
class QTableWidget;

class CatalogPage : public QWidget
{
    Q_OBJECT

public:
    explicit CatalogPage(QWidget *parent = nullptr);

    void setCatalog(const CatalogRepository *catalog);

    int selectedCameraCatalogIndex() const;
    int selectedLensCatalogIndex() const;
    int selectedLightCatalogIndex() const;
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
    const CatalogRepository *m_catalog = nullptr;

    QTableWidget *m_cameraTable = nullptr;
    QTableWidget *m_lensTable = nullptr;
    QTableWidget *m_lightTable = nullptr;

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

    void refreshCatalogFilterOptions();
    void refreshCameraTable();
    void refreshLensTable();
    void refreshLightTable();
    void clearCameraFilters();
    void clearLensFilters();
    void clearLightFilters();
};

#endif
