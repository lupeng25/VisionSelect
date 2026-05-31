#include "ui/pages/CatalogPage.h"

#include "catalog/CatalogRepository.h"
#include "ui/UiHelpers.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QtGlobal>

using namespace UiHelpers;

namespace {
QString catalogKey(const QString &manufacturer, const QString &model)
{
    return manufacturer.trimmed() + QLatin1Char('\n') + model.trimmed();
}

QString selectedCatalogKey(const QTableWidget *table)
{
    if (!table || table->currentRow() < 0)
        return QString();
    const QTableWidgetItem *manufacturer = table->item(table->currentRow(), 1);
    const QTableWidgetItem *model = table->item(table->currentRow(), 0);
    if (!manufacturer || !model)
        return QString();
    return catalogKey(manufacturer->text(), model->text());
}

void selectCatalogRow(QTableWidget *table, const QString &key, int fallbackRow)
{
    if (!table || table->rowCount() <= 0)
        return;
    if (!key.isEmpty()) {
        for (int row = 0; row < table->rowCount(); ++row) {
            const QTableWidgetItem *manufacturer = table->item(row, 1);
            const QTableWidgetItem *model = table->item(row, 0);
            if (manufacturer && model && catalogKey(manufacturer->text(), model->text()) == key) {
                table->selectRow(row);
                return;
            }
        }
    }
    table->selectRow(qBound(0, fallbackRow, table->rowCount() - 1));
}

QString allManufacturersText()
{
    return localizedText("全部厂家", "All Manufacturers");
}

QString allInterfacesText()
{
    return localizedText("全部接口", "All Interfaces");
}

QString allTypesText()
{
    return localizedText("全部类型", "All Types");
}

QString allMountsText()
{
    return localizedText("全部接口", "All Mounts");
}

QString allModesText()
{
    return localizedText("全部模式", "All Modes");
}

void fillComboPreservingText(QComboBox *combo, const QString &allText, const QStringList &values)
{
    if (!combo)
        return;
    const QString previous = combo->currentText();
    QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem(allText);
    QStringList sorted = values;
    sorted.removeDuplicates();
    sorted.sort(Qt::CaseInsensitive);
    for (const QString &value : sorted) {
        if (!value.trimmed().isEmpty())
            combo->addItem(value);
    }
    const int index = combo->findText(previous, Qt::MatchFixedString);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

bool textMatches(const QString &needle, const QStringList &fields)
{
    if (needle.trimmed().isEmpty())
        return true;
    const QString lowered = needle.trimmed().toLower();
    for (const QString &field : fields) {
        if (field.toLower().contains(lowered))
            return true;
    }
    return false;
}

QVector<int> visibleCatalogIndexes(const QTableWidget *table)
{
    QVector<int> indexes;
    if (!table)
        return indexes;
    for (int row = 0; row < table->rowCount(); ++row) {
        const int index = rowSourceIndex(table, row);
        if (index >= 0)
            indexes.append(index);
    }
    return indexes;
}
}

CatalogPage::CatalogPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);
    layout->addWidget(pageHeader(localizedText("参数库", "Catalog"),
        localizedText("维护相机、镜头和光源目录；筛选、导入、导出均保持原有数据格式。", "Maintain camera, lens, and light catalogs while preserving existing import and export formats.")));

    QTabWidget *tabs = new QTabWidget;

    QWidget *cameraPage = new QWidget;
    QVBoxLayout *cameraLayout = new QVBoxLayout(cameraPage);
    cameraLayout->setContentsMargins(0, 0, 0, 0);
    cameraLayout->setSpacing(8);
    QHBoxLayout *cameraFilters = new QHBoxLayout;
    m_cameraSearchEdit = new QLineEdit;
    m_cameraSearchEdit->setPlaceholderText(localizedText("搜索型号/厂家/接口", "Search model / manufacturer / interface"));
    m_cameraManufacturerFilter = new QComboBox;
    m_cameraInterfaceFilter = new QComboBox;
    QPushButton *clearCameraFilterButton = new QPushButton(localizedText("清空筛选", "Clear Filters"));
    clearCameraFilterButton->setObjectName(QStringLiteral("SecondaryButton"));
    cameraFilters->addWidget(m_cameraSearchEdit, 2);
    cameraFilters->addWidget(m_cameraManufacturerFilter);
    cameraFilters->addWidget(m_cameraInterfaceFilter);
    cameraFilters->addWidget(clearCameraFilterButton);
    cameraLayout->addLayout(cameraFilters);
    QHBoxLayout *cameraActions = new QHBoxLayout;
    QPushButton *addCameraButton = new QPushButton(localizedText("新增相机", "Add Camera"));
    QPushButton *editCameraButton = new QPushButton(localizedText("编辑", "Edit"));
    QPushButton *removeCameraButton = new QPushButton(localizedText("删除", "Delete"));
    QPushButton *importCameraButton = new QPushButton(localizedText("导入 CSV", "Import CSV"));
    QPushButton *exportCameraButton = new QPushButton(localizedText("导出 CSV", "Export CSV"));
    QPushButton *exportFilteredCameraButton = new QPushButton(localizedText("导出筛选", "Export Filtered"));
    QPushButton *resetCameraButton = new QPushButton(localizedText("重置内置", "Reset Built-in"));
    editCameraButton->setObjectName(QStringLiteral("SecondaryButton"));
    removeCameraButton->setObjectName(QStringLiteral("SecondaryButton"));
    importCameraButton->setObjectName(QStringLiteral("SecondaryButton"));
    exportCameraButton->setObjectName(QStringLiteral("SecondaryButton"));
    exportFilteredCameraButton->setObjectName(QStringLiteral("SecondaryButton"));
    resetCameraButton->setObjectName(QStringLiteral("SecondaryButton"));
    cameraActions->addWidget(addCameraButton);
    cameraActions->addWidget(editCameraButton);
    cameraActions->addWidget(removeCameraButton);
    cameraActions->addWidget(importCameraButton);
    cameraActions->addWidget(exportCameraButton);
    cameraActions->addWidget(exportFilteredCameraButton);
    cameraActions->addWidget(resetCameraButton);
    cameraActions->addStretch();
    cameraLayout->addLayout(cameraActions);
    m_cameraTable = new QTableWidget;
    setupTable(m_cameraTable);
    cameraLayout->addWidget(m_cameraTable, 1);
    tabs->addTab(cameraPage, localizedText("相机", "Cameras"));

    QWidget *lensPage = new QWidget;
    QVBoxLayout *lensLayout = new QVBoxLayout(lensPage);
    lensLayout->setContentsMargins(0, 0, 0, 0);
    lensLayout->setSpacing(8);
    QHBoxLayout *lensFilters = new QHBoxLayout;
    m_lensSearchEdit = new QLineEdit;
    m_lensSearchEdit->setPlaceholderText(localizedText("搜索型号/厂家/备注", "Search model / manufacturer / notes"));
    m_lensManufacturerFilter = new QComboBox;
    m_lensTypeFilter = new QComboBox;
    m_lensMountFilter = new QComboBox;
    QPushButton *clearLensFilterButton = new QPushButton(localizedText("清空筛选", "Clear Filters"));
    clearLensFilterButton->setObjectName(QStringLiteral("SecondaryButton"));
    lensFilters->addWidget(m_lensSearchEdit, 2);
    lensFilters->addWidget(m_lensManufacturerFilter);
    lensFilters->addWidget(m_lensTypeFilter);
    lensFilters->addWidget(m_lensMountFilter);
    lensFilters->addWidget(clearLensFilterButton);
    lensLayout->addLayout(lensFilters);
    QHBoxLayout *lensActions = new QHBoxLayout;
    QPushButton *addLensButton = new QPushButton(localizedText("新增镜头", "Add Lens"));
    QPushButton *editLensButton = new QPushButton(localizedText("编辑", "Edit"));
    QPushButton *removeLensButton = new QPushButton(localizedText("删除", "Delete"));
    QPushButton *importLensButton = new QPushButton(localizedText("导入 CSV", "Import CSV"));
    QPushButton *exportLensButton = new QPushButton(localizedText("导出 CSV", "Export CSV"));
    QPushButton *exportFilteredLensButton = new QPushButton(localizedText("导出筛选", "Export Filtered"));
    QPushButton *resetLensButton = new QPushButton(localizedText("重置内置", "Reset Built-in"));
    editLensButton->setObjectName(QStringLiteral("SecondaryButton"));
    removeLensButton->setObjectName(QStringLiteral("SecondaryButton"));
    importLensButton->setObjectName(QStringLiteral("SecondaryButton"));
    exportLensButton->setObjectName(QStringLiteral("SecondaryButton"));
    exportFilteredLensButton->setObjectName(QStringLiteral("SecondaryButton"));
    resetLensButton->setObjectName(QStringLiteral("SecondaryButton"));
    lensActions->addWidget(addLensButton);
    lensActions->addWidget(editLensButton);
    lensActions->addWidget(removeLensButton);
    lensActions->addWidget(importLensButton);
    lensActions->addWidget(exportLensButton);
    lensActions->addWidget(exportFilteredLensButton);
    lensActions->addWidget(resetLensButton);
    lensActions->addStretch();
    lensLayout->addLayout(lensActions);
    m_lensTable = new QTableWidget;
    setupTable(m_lensTable);
    lensLayout->addWidget(m_lensTable, 1);
    tabs->addTab(lensPage, localizedText("镜头", "Lenses"));

    QWidget *lightPage = new QWidget;
    QVBoxLayout *lightLayout = new QVBoxLayout(lightPage);
    lightLayout->setContentsMargins(0, 0, 0, 0);
    lightLayout->setSpacing(8);
    QHBoxLayout *lightFilters = new QHBoxLayout;
    m_lightSearchEdit = new QLineEdit;
    m_lightSearchEdit->setPlaceholderText(localizedText("搜索型号/厂家/场景", "Search model / manufacturer / scenario"));
    m_lightManufacturerFilter = new QComboBox;
    m_lightTypeFilter = new QComboBox;
    m_lightModeFilter = new QComboBox;
    QPushButton *clearLightFilterButton = new QPushButton(localizedText("清空筛选", "Clear Filters"));
    clearLightFilterButton->setObjectName(QStringLiteral("SecondaryButton"));
    lightFilters->addWidget(m_lightSearchEdit, 2);
    lightFilters->addWidget(m_lightManufacturerFilter);
    lightFilters->addWidget(m_lightTypeFilter);
    lightFilters->addWidget(m_lightModeFilter);
    lightFilters->addWidget(clearLightFilterButton);
    lightLayout->addLayout(lightFilters);
    QHBoxLayout *lightActions = new QHBoxLayout;
    QPushButton *addLightButton = new QPushButton(localizedText("新增光源", "Add Light"));
    QPushButton *editLightButton = new QPushButton(localizedText("编辑", "Edit"));
    QPushButton *removeLightButton = new QPushButton(localizedText("删除", "Delete"));
    QPushButton *importLightButton = new QPushButton(localizedText("导入 CSV", "Import CSV"));
    QPushButton *exportLightButton = new QPushButton(localizedText("导出 CSV", "Export CSV"));
    QPushButton *exportFilteredLightButton = new QPushButton(localizedText("导出筛选", "Export Filtered"));
    QPushButton *resetLightButton = new QPushButton(localizedText("重置内置", "Reset Built-in"));
    editLightButton->setObjectName(QStringLiteral("SecondaryButton"));
    removeLightButton->setObjectName(QStringLiteral("SecondaryButton"));
    importLightButton->setObjectName(QStringLiteral("SecondaryButton"));
    exportLightButton->setObjectName(QStringLiteral("SecondaryButton"));
    exportFilteredLightButton->setObjectName(QStringLiteral("SecondaryButton"));
    resetLightButton->setObjectName(QStringLiteral("SecondaryButton"));
    lightActions->addWidget(addLightButton);
    lightActions->addWidget(editLightButton);
    lightActions->addWidget(removeLightButton);
    lightActions->addWidget(importLightButton);
    lightActions->addWidget(exportLightButton);
    lightActions->addWidget(exportFilteredLightButton);
    lightActions->addWidget(resetLightButton);
    lightActions->addStretch();
    lightLayout->addLayout(lightActions);
    m_lightTable = new QTableWidget;
    setupTable(m_lightTable);
    lightLayout->addWidget(m_lightTable, 1);
    tabs->addTab(lightPage, localizedText("光源", "Lights"));

    connect(m_cameraSearchEdit, &QLineEdit::textChanged, this, &CatalogPage::refreshCameraTable);
    connect(m_cameraManufacturerFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CatalogPage::refreshCameraTable);
    connect(m_cameraInterfaceFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CatalogPage::refreshCameraTable);
    connect(clearCameraFilterButton, &QPushButton::clicked, this, &CatalogPage::clearCameraFilters);
    connect(addCameraButton, &QPushButton::clicked, this, &CatalogPage::cameraAddRequested);
    connect(editCameraButton, &QPushButton::clicked, this, &CatalogPage::cameraEditRequested);
    connect(removeCameraButton, &QPushButton::clicked, this, &CatalogPage::cameraRemoveRequested);
    connect(importCameraButton, &QPushButton::clicked, this, &CatalogPage::cameraImportRequested);
    connect(exportCameraButton, &QPushButton::clicked, this, &CatalogPage::cameraExportRequested);
    connect(exportFilteredCameraButton, &QPushButton::clicked, this, &CatalogPage::cameraExportFilteredRequested);
    connect(resetCameraButton, &QPushButton::clicked, this, &CatalogPage::cameraResetRequested);
    connect(m_cameraTable, &QTableWidget::cellDoubleClicked, this, [this](int, int) { emit cameraEditRequested(); });

    connect(m_lensSearchEdit, &QLineEdit::textChanged, this, &CatalogPage::refreshLensTable);
    connect(m_lensManufacturerFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CatalogPage::refreshLensTable);
    connect(m_lensTypeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CatalogPage::refreshLensTable);
    connect(m_lensMountFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CatalogPage::refreshLensTable);
    connect(clearLensFilterButton, &QPushButton::clicked, this, &CatalogPage::clearLensFilters);
    connect(addLensButton, &QPushButton::clicked, this, &CatalogPage::lensAddRequested);
    connect(editLensButton, &QPushButton::clicked, this, &CatalogPage::lensEditRequested);
    connect(removeLensButton, &QPushButton::clicked, this, &CatalogPage::lensRemoveRequested);
    connect(importLensButton, &QPushButton::clicked, this, &CatalogPage::lensImportRequested);
    connect(exportLensButton, &QPushButton::clicked, this, &CatalogPage::lensExportRequested);
    connect(exportFilteredLensButton, &QPushButton::clicked, this, &CatalogPage::lensExportFilteredRequested);
    connect(resetLensButton, &QPushButton::clicked, this, &CatalogPage::lensResetRequested);
    connect(m_lensTable, &QTableWidget::cellDoubleClicked, this, [this](int, int) { emit lensEditRequested(); });

    connect(m_lightSearchEdit, &QLineEdit::textChanged, this, &CatalogPage::refreshLightTable);
    connect(m_lightManufacturerFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CatalogPage::refreshLightTable);
    connect(m_lightTypeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CatalogPage::refreshLightTable);
    connect(m_lightModeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CatalogPage::refreshLightTable);
    connect(clearLightFilterButton, &QPushButton::clicked, this, &CatalogPage::clearLightFilters);
    connect(addLightButton, &QPushButton::clicked, this, &CatalogPage::lightAddRequested);
    connect(editLightButton, &QPushButton::clicked, this, &CatalogPage::lightEditRequested);
    connect(removeLightButton, &QPushButton::clicked, this, &CatalogPage::lightRemoveRequested);
    connect(importLightButton, &QPushButton::clicked, this, &CatalogPage::lightImportRequested);
    connect(exportLightButton, &QPushButton::clicked, this, &CatalogPage::lightExportRequested);
    connect(exportFilteredLightButton, &QPushButton::clicked, this, &CatalogPage::lightExportFilteredRequested);
    connect(resetLightButton, &QPushButton::clicked, this, &CatalogPage::lightResetRequested);
    connect(m_lightTable, &QTableWidget::cellDoubleClicked, this, [this](int, int) { emit lightEditRequested(); });

    layout->addWidget(tabs, 1);
}

void CatalogPage::setCatalog(const CatalogRepository *catalog)
{
    m_catalog = catalog;
    refreshCatalogFilterOptions();
    refreshCameraTable();
    refreshLensTable();
    refreshLightTable();
}

void CatalogPage::refreshCatalogFilterOptions()
{
    QStringList cameraManufacturers;
    QStringList cameraInterfaces;
    if (m_catalog) {
        for (const CameraSpec &camera : m_catalog->cameras()) {
            cameraManufacturers.append(camera.manufacturer);
            cameraInterfaces.append(camera.interfaceType);
        }
    }
    fillComboPreservingText(m_cameraManufacturerFilter, allManufacturersText(), cameraManufacturers);
    fillComboPreservingText(m_cameraInterfaceFilter, allInterfacesText(), cameraInterfaces);

    QStringList lensManufacturers;
    QStringList lensTypes;
    QStringList lensMounts;
    if (m_catalog) {
        for (const LensSpec &lens : m_catalog->lenses()) {
            lensManufacturers.append(lens.manufacturer);
            lensTypes.append(lens.typeLabel());
            lensMounts.append(lens.lensMount);
        }
    }
    fillComboPreservingText(m_lensManufacturerFilter, allManufacturersText(), lensManufacturers);
    fillComboPreservingText(m_lensTypeFilter, allTypesText(), lensTypes);
    fillComboPreservingText(m_lensMountFilter, allMountsText(), lensMounts);

    QStringList lightManufacturers;
    QStringList lightTypes;
    QStringList lightModes;
    if (m_catalog) {
        for (const LightSpec &light : m_catalog->lights()) {
            lightManufacturers.append(light.manufacturer);
            lightTypes.append(light.typeLabel());
            lightModes.append(light.mode);
        }
    }
    fillComboPreservingText(m_lightManufacturerFilter, allManufacturersText(), lightManufacturers);
    fillComboPreservingText(m_lightTypeFilter, allTypesText(), lightTypes);
    fillComboPreservingText(m_lightModeFilter, allModesText(), lightModes);
}

void CatalogPage::refreshCameraTable()
{
    if (!m_cameraTable)
        return;
    const QString previousKey = selectedCatalogKey(m_cameraTable);
    const int previousRow = m_cameraTable->currentRow();
    m_cameraTable->setSortingEnabled(false);
    m_cameraTable->setColumnCount(10);
    m_cameraTable->setHorizontalHeaderLabels({localizedText("型号", "Model"), localizedText("厂家", "Manufacturer"), localizedText("分辨率", "Resolution"), localizedText("像元", "Pixel"),
        localizedText("传感器", "Sensor"), localizedText("靶面", "Format"), localizedText("快门", "Shutter"), QStringLiteral("fps"),
        localizedText("接口", "Interface"), localizedText("镜头口", "Lens Mount")});
    m_cameraTable->setRowCount(0);
    const QString search = m_cameraSearchEdit ? m_cameraSearchEdit->text() : QString();
    const QString manufacturer = m_cameraManufacturerFilter && m_cameraManufacturerFilter->currentIndex() > 0
        ? m_cameraManufacturerFilter->currentText() : QString();
    const QString interfaceType = m_cameraInterfaceFilter && m_cameraInterfaceFilter->currentIndex() > 0
        ? m_cameraInterfaceFilter->currentText() : QString();
    if (m_catalog) {
        for (int i = 0; i < m_catalog->cameras().size(); ++i) {
            const CameraSpec &c = m_catalog->cameras().at(i);
            if (!manufacturer.isEmpty() && c.manufacturer != manufacturer)
                continue;
            if (!interfaceType.isEmpty() && c.interfaceType != interfaceType)
                continue;
            if (!textMatches(search, {c.model, c.manufacturer, c.interfaceType, c.lensMount, c.sensorFormat}))
                continue;
            const int row = m_cameraTable->rowCount();
            m_cameraTable->insertRow(row);
            m_cameraTable->setItem(row, 0, indexedItem(c.model, i));
            m_cameraTable->setItem(row, 1, item(c.manufacturer));
            m_cameraTable->setItem(row, 2, item(QStringLiteral("%1 x %2").arg(c.resolutionX).arg(c.resolutionY)));
            m_cameraTable->setItem(row, 3, item(QStringLiteral("%1 um").arg(c.pixelSizeUm, 0, 'f', 2)));
            m_cameraTable->setItem(row, 4, item(QStringLiteral("%1 x %2 mm").arg(c.sensorWidthMm(), 0, 'f', 2).arg(c.sensorHeightMm(), 0, 'f', 2)));
            m_cameraTable->setItem(row, 5, item(c.sensorFormat));
            m_cameraTable->setItem(row, 6, item(c.shutterType));
            m_cameraTable->setItem(row, 7, item(number(c.maxFps, 1)));
            m_cameraTable->setItem(row, 8, item(c.interfaceType));
            m_cameraTable->setItem(row, 9, item(c.lensMount));
        }
    }
    m_cameraTable->setSortingEnabled(true);
    selectCatalogRow(m_cameraTable, previousKey, previousRow);
}

void CatalogPage::refreshLensTable()
{
    if (!m_lensTable)
        return;
    const QString previousKey = selectedCatalogKey(m_lensTable);
    const int previousRow = m_lensTable->currentRow();
    m_lensTable->setSortingEnabled(false);
    m_lensTable->setColumnCount(12);
    m_lensTable->setHorizontalHeaderLabels({localizedText("型号", "Model"), localizedText("厂家", "Manufacturer"), localizedText("类型", "Type"), localizedText("接口", "Mount"),
        localizedText("焦距", "Focal"), QStringLiteral("PMAG"), QStringLiteral("WD"), localizedText("像圈", "Image Circle"),
        localizedText("远心度", "Telecentricity"), localizedText("畸变", "Distortion"), QStringLiteral("DOF"), localizedText("同轴", "Coaxial")});
    m_lensTable->setRowCount(0);
    const QString search = m_lensSearchEdit ? m_lensSearchEdit->text() : QString();
    const QString manufacturer = m_lensManufacturerFilter && m_lensManufacturerFilter->currentIndex() > 0
        ? m_lensManufacturerFilter->currentText() : QString();
    const QString type = m_lensTypeFilter && m_lensTypeFilter->currentIndex() > 0
        ? m_lensTypeFilter->currentText() : QString();
    const QString mount = m_lensMountFilter && m_lensMountFilter->currentIndex() > 0
        ? m_lensMountFilter->currentText() : QString();
    if (m_catalog) {
        for (int i = 0; i < m_catalog->lenses().size(); ++i) {
            const LensSpec &l = m_catalog->lenses().at(i);
            if (!manufacturer.isEmpty() && l.manufacturer != manufacturer)
                continue;
            if (!type.isEmpty() && l.typeLabel() != type)
                continue;
            if (!mount.isEmpty() && l.lensMount != mount)
                continue;
            if (!textMatches(search, {l.model, l.manufacturer, l.typeLabel(), l.lensMount, l.notes}))
                continue;
            const int row = m_lensTable->rowCount();
            m_lensTable->insertRow(row);
            m_lensTable->setItem(row, 0, indexedItem(l.model, i));
            m_lensTable->setItem(row, 1, item(l.manufacturer));
            m_lensTable->setItem(row, 2, item(l.typeLabel()));
            m_lensTable->setItem(row, 3, item(l.lensMount));
            m_lensTable->setItem(row, 4, item(l.isTelecentric() ? QStringLiteral("-") : QStringLiteral("%1 mm").arg(l.focalLengthMm, 0, 'f', 1)));
            m_lensTable->setItem(row, 5, item(l.isTelecentric() ? QStringLiteral("%1x").arg(l.pmag, 0, 'f', 3) : QStringLiteral("-")));
            m_lensTable->setItem(row, 6, item(l.isTelecentric() ? QStringLiteral("%1 mm").arg(l.nominalWorkingDistanceMm, 0, 'f', 1) : QStringLiteral(">=%1 mm").arg(l.minWorkingDistanceMm, 0, 'f', 1)));
            m_lensTable->setItem(row, 7, item(QStringLiteral("%1 mm").arg(l.imageCircleMm, 0, 'f', 1)));
            m_lensTable->setItem(row, 8, item(l.isTelecentric() ? QStringLiteral("%1 deg").arg(l.telecentricityDeg, 0, 'f', 3) : QStringLiteral("-")));
            m_lensTable->setItem(row, 9, item(QStringLiteral("%1%").arg(l.distortionPercent, 0, 'f', 3)));
            m_lensTable->setItem(row, 10, item(l.isTelecentric() ? QStringLiteral("%1 mm").arg(l.dofMm, 0, 'f', 1) : QStringLiteral("-")));
            m_lensTable->setItem(row, 11, item(boolLabel(l.coaxialIllumination)));
        }
    }
    m_lensTable->setSortingEnabled(true);
    selectCatalogRow(m_lensTable, previousKey, previousRow);
}

void CatalogPage::refreshLightTable()
{
    if (!m_lightTable)
        return;
    const QString previousKey = selectedCatalogKey(m_lightTable);
    const int previousRow = m_lightTable->currentRow();
    m_lightTable->setSortingEnabled(false);
    m_lightTable->setColumnCount(8);
    m_lightTable->setHorizontalHeaderLabels({localizedText("型号", "Model"), localizedText("厂家", "Manufacturer"), localizedText("类型", "Type"), localizedText("颜色", "Color"),
        localizedText("波长", "Wavelength"), localizedText("模式", "Mode"), localizedText("有效面积", "Active Area"), localizedText("适用场景", "Best For")});
    m_lightTable->setRowCount(0);
    const QString search = m_lightSearchEdit ? m_lightSearchEdit->text() : QString();
    const QString manufacturer = m_lightManufacturerFilter && m_lightManufacturerFilter->currentIndex() > 0
        ? m_lightManufacturerFilter->currentText() : QString();
    const QString type = m_lightTypeFilter && m_lightTypeFilter->currentIndex() > 0
        ? m_lightTypeFilter->currentText() : QString();
    const QString mode = m_lightModeFilter && m_lightModeFilter->currentIndex() > 0
        ? m_lightModeFilter->currentText() : QString();
    if (m_catalog) {
        for (int i = 0; i < m_catalog->lights().size(); ++i) {
            const LightSpec &l = m_catalog->lights().at(i);
            if (!manufacturer.isEmpty() && l.manufacturer != manufacturer)
                continue;
            if (!type.isEmpty() && l.typeLabel() != type)
                continue;
            if (!mode.isEmpty() && l.mode != mode)
                continue;
            if (!textMatches(search, {l.model, l.manufacturer, l.typeLabel(), l.color, l.mode, l.bestFor}))
                continue;
            const int row = m_lightTable->rowCount();
            m_lightTable->insertRow(row);
            m_lightTable->setItem(row, 0, indexedItem(l.model, i));
            m_lightTable->setItem(row, 1, item(l.manufacturer));
            m_lightTable->setItem(row, 2, item(l.typeLabel()));
            m_lightTable->setItem(row, 3, item(l.color));
            m_lightTable->setItem(row, 4, item(l.wavelengthNm > 0 ? QStringLiteral("%1 nm").arg(l.wavelengthNm) : localizedText("宽谱", "Broadband")));
            m_lightTable->setItem(row, 5, item(l.mode));
            m_lightTable->setItem(row, 6, item(QStringLiteral("%1 x %2 mm").arg(l.activeWidthMm, 0, 'f', 0).arg(l.activeHeightMm, 0, 'f', 0)));
            m_lightTable->setItem(row, 7, item(l.bestFor));
        }
    }
    m_lightTable->setSortingEnabled(true);
    selectCatalogRow(m_lightTable, previousKey, previousRow);
}

void CatalogPage::clearCameraFilters()
{
    if (m_cameraSearchEdit)
        m_cameraSearchEdit->clear();
    if (m_cameraManufacturerFilter)
        m_cameraManufacturerFilter->setCurrentIndex(0);
    if (m_cameraInterfaceFilter)
        m_cameraInterfaceFilter->setCurrentIndex(0);
    refreshCameraTable();
}

void CatalogPage::clearLensFilters()
{
    if (m_lensSearchEdit)
        m_lensSearchEdit->clear();
    if (m_lensManufacturerFilter)
        m_lensManufacturerFilter->setCurrentIndex(0);
    if (m_lensTypeFilter)
        m_lensTypeFilter->setCurrentIndex(0);
    if (m_lensMountFilter)
        m_lensMountFilter->setCurrentIndex(0);
    refreshLensTable();
}

void CatalogPage::clearLightFilters()
{
    if (m_lightSearchEdit)
        m_lightSearchEdit->clear();
    if (m_lightManufacturerFilter)
        m_lightManufacturerFilter->setCurrentIndex(0);
    if (m_lightTypeFilter)
        m_lightTypeFilter->setCurrentIndex(0);
    if (m_lightModeFilter)
        m_lightModeFilter->setCurrentIndex(0);
    refreshLightTable();
}

int CatalogPage::selectedCameraCatalogIndex() const
{
    if (!m_cameraTable)
        return -1;
    return rowSourceIndex(m_cameraTable, m_cameraTable->currentRow());
}

int CatalogPage::selectedLensCatalogIndex() const
{
    if (!m_lensTable)
        return -1;
    return rowSourceIndex(m_lensTable, m_lensTable->currentRow());
}

int CatalogPage::selectedLightCatalogIndex() const
{
    if (!m_lightTable)
        return -1;
    return rowSourceIndex(m_lightTable, m_lightTable->currentRow());
}

QVector<int> CatalogPage::visibleCameraCatalogIndexes() const
{
    return visibleCatalogIndexes(m_cameraTable);
}

QVector<int> CatalogPage::visibleLensCatalogIndexes() const
{
    return visibleCatalogIndexes(m_lensTable);
}

QVector<int> CatalogPage::visibleLightCatalogIndexes() const
{
    return visibleCatalogIndexes(m_lightTable);
}
