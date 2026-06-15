#include "ui/pages/CatalogPage.h"

#include "ui/UiHelpers.h"

#include <QAbstractTableModel>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QTabWidget>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

using namespace UiHelpers;

namespace {
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

void fillComboPreservingText(QComboBox *combo, const QString &allText, QStringList values)
{
    if (!combo)
        return;
    const QString previous = combo->currentText();
    QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem(allText);
    values.removeDuplicates();
    values.sort(Qt::CaseInsensitive);
    for (const QString &value : values) {
        if (!value.trimmed().isEmpty())
            combo->addItem(value);
    }
    const int index = combo->findText(previous, Qt::MatchFixedString);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

void setupTableView(QTableView *view)
{
    view->setAlternatingRowColors(true);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setSortingEnabled(false);
    view->verticalHeader()->setVisible(false);
    view->verticalHeader()->setDefaultSectionSize(34);
    view->horizontalHeader()->setStretchLastSection(true);
    view->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    view->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->setTextElideMode(Qt::ElideRight);
    view->setWordWrap(false);
}

QString pageText(int total, int offset, int shown)
{
    if (total <= 0)
        return localizedText("共 0 条", "0 items");
    const int first = offset + 1;
    const int last = qMin(total, offset + shown);
    return localizedText("共 %1 条，当前 %2-%3 条", "%1 items, showing %2-%3")
        .arg(total)
        .arg(first)
        .arg(last);
}

QString catalogErrorText(const QString &error)
{
    return localizedText("加载失败：%1", "Load failed: %1").arg(error);
}

int totalPages(int total, int pageSize)
{
    return qMax(1, (qMax(0, total) + pageSize - 1) / pageSize);
}

void setupPageSpin(QSpinBox *spin)
{
    spin->setKeyboardTracking(false);
    spin->setMinimum(1);
    spin->setMaximum(1);
    spin->setFixedWidth(120);
}

void updatePageSpin(QSpinBox *spin, int total, int offset, int pageSize)
{
    if (!spin)
        return;
    const int pages = totalPages(total, pageSize);
    const int current = qBound(1, offset / pageSize + 1, pages);
    QSignalBlocker blocker(spin);
    spin->setRange(1, pages);
    spin->setValue(current);
    spin->setPrefix(QStringLiteral("# "));
    spin->setSuffix(QStringLiteral(" / %1").arg(pages));
}

QPushButton *secondaryButton(const QString &text)
{
    QPushButton *button = new QPushButton(text);
    button->setObjectName(QStringLiteral("SecondaryButton"));
    return button;
}
}

class CatalogTableModel : public QAbstractTableModel
{
public:
    explicit CatalogTableModel(CatalogDomain domain, QObject *parent = nullptr)
        : QAbstractTableModel(parent), m_domain(domain)
    {
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        if (parent.isValid())
            return 0;
        return m_ids.size();
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override
    {
        if (parent.isValid())
            return 0;
        if (m_domain == CatalogDomain::Camera)
            return 10;
        if (m_domain == CatalogDomain::Lens)
            return 12;
        return 8;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
            return QVariant();
        if (m_domain == CatalogDomain::Camera) {
            const QStringList headers = {localizedText("型号", "Model"), localizedText("厂家", "Manufacturer"), localizedText("分辨率", "Resolution"),
                localizedText("像元", "Pixel"), localizedText("传感器", "Sensor"), localizedText("靶面", "Format"),
                localizedText("快门", "Shutter"), QStringLiteral("fps"), localizedText("接口", "Interface"), localizedText("镜头口", "Lens Mount")};
            return headers.value(section);
        }
        if (m_domain == CatalogDomain::Lens) {
            const QStringList headers = {localizedText("型号", "Model"), localizedText("厂家", "Manufacturer"), localizedText("类型", "Type"),
                localizedText("接口", "Mount"), localizedText("焦距", "Focal"), QStringLiteral("PMAG"), QStringLiteral("WD"),
                localizedText("像圈", "Image Circle"), localizedText("远心度", "Telecentricity"), localizedText("畸变", "Distortion"),
                QStringLiteral("DOF"), localizedText("同轴", "Coaxial")};
            return headers.value(section);
        }
        const QStringList headers = {localizedText("型号", "Model"), localizedText("厂家", "Manufacturer"), localizedText("类型", "Type"),
            localizedText("颜色", "Color"), localizedText("波长", "Wavelength"), localizedText("模式", "Mode"),
            localizedText("有效面积", "Active Area"), localizedText("适用场景", "Best For")};
        return headers.value(section);
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_ids.size())
            return QVariant();
        if (role == Qt::UserRole)
            return m_ids.at(index.row());
        if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
            return QVariant();
        const QString text = cellText(index.row(), index.column());
        return text;
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        return QAbstractTableModel::flags(index) & ~Qt::ItemIsEditable;
    }

    qint64 idAt(int row) const
    {
        return row >= 0 && row < m_ids.size() ? m_ids.at(row) : -1;
    }

    void setCameras(const QVector<qint64> &ids, const QVector<CameraSpec> &items)
    {
        beginResetModel();
        m_ids = ids;
        m_cameras = items;
        m_lenses.clear();
        m_lights.clear();
        endResetModel();
    }

    void setLenses(const QVector<qint64> &ids, const QVector<LensSpec> &items)
    {
        beginResetModel();
        m_ids = ids;
        m_lenses = items;
        m_cameras.clear();
        m_lights.clear();
        endResetModel();
    }

    void setLights(const QVector<qint64> &ids, const QVector<LightSpec> &items)
    {
        beginResetModel();
        m_ids = ids;
        m_lights = items;
        m_cameras.clear();
        m_lenses.clear();
        endResetModel();
    }

private:
    QString cellText(int row, int column) const
    {
        if (m_domain == CatalogDomain::Camera) {
            const CameraSpec &c = m_cameras.at(row);
            switch (column) {
            case 0: return c.model;
            case 1: return c.manufacturer;
            case 2: return QStringLiteral("%1 x %2").arg(c.resolutionX).arg(c.resolutionY);
            case 3: return QStringLiteral("%1 um").arg(c.pixelSizeUm, 0, 'f', 2);
            case 4: return QStringLiteral("%1 x %2 mm").arg(c.sensorWidthMm(), 0, 'f', 2).arg(c.sensorHeightMm(), 0, 'f', 2);
            case 5: return c.sensorFormat;
            case 6: return c.shutterType;
            case 7: return number(c.maxFps, 1);
            case 8: return c.interfaceType;
            case 9: return c.lensMount;
            }
        } else if (m_domain == CatalogDomain::Lens) {
            const LensSpec &l = m_lenses.at(row);
            switch (column) {
            case 0: return l.model;
            case 1: return l.manufacturer;
            case 2: return l.typeLabel();
            case 3: return l.lensMount;
            case 4: return l.isTelecentric() ? QStringLiteral("-") : QStringLiteral("%1 mm").arg(l.focalLengthMm, 0, 'f', 1);
            case 5: return l.isTelecentric() ? QStringLiteral("%1x").arg(l.pmag, 0, 'f', 3) : QStringLiteral("-");
            case 6: return l.isTelecentric() ? QStringLiteral("%1 mm").arg(l.nominalWorkingDistanceMm, 0, 'f', 1) : QStringLiteral(">=%1 mm").arg(l.minWorkingDistanceMm, 0, 'f', 1);
            case 7: return QStringLiteral("%1 mm").arg(l.imageCircleMm, 0, 'f', 1);
            case 8: return l.isTelecentric() ? QStringLiteral("%1 deg").arg(l.telecentricityDeg, 0, 'f', 3) : QStringLiteral("-");
            case 9: return QStringLiteral("%1%").arg(l.distortionPercent, 0, 'f', 3);
            case 10: return l.isTelecentric() ? QStringLiteral("%1 mm").arg(l.dofMm, 0, 'f', 1) : QStringLiteral("-");
            case 11: return boolLabel(l.coaxialIllumination);
            }
        } else {
            const LightSpec &l = m_lights.at(row);
            switch (column) {
            case 0: return l.model;
            case 1: return l.manufacturer;
            case 2: return l.typeLabel();
            case 3: return l.color;
            case 4: return l.wavelengthNm > 0 ? QStringLiteral("%1 nm").arg(l.wavelengthNm) : localizedText("宽谱", "Broadband");
            case 5: return l.mode;
            case 6: return QStringLiteral("%1 x %2 mm").arg(l.activeWidthMm, 0, 'f', 0).arg(l.activeHeightMm, 0, 'f', 0);
            case 7: return l.bestFor;
            }
        }
        return QString();
    }

    CatalogDomain m_domain;
    QVector<qint64> m_ids;
    QVector<CameraSpec> m_cameras;
    QVector<LensSpec> m_lenses;
    QVector<LightSpec> m_lights;
};

CatalogPage::CatalogPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);
    layout->addWidget(pageHeader(localizedText("参数库", "Catalog"),
        localizedText("维护相机、镜头和光源目录；筛选、导入、导出均保持原有数据格式。", "Maintain camera, lens, and light catalogs while preserving existing import and export formats.")));

    m_filterTimer = new QTimer(this);
    m_filterTimer->setSingleShot(true);
    m_filterTimer->setInterval(250);
    connect(m_filterTimer, &QTimer::timeout, this, &CatalogPage::refreshCurrentPage);

    m_tabs = new QTabWidget;

    QWidget *cameraPage = new QWidget;
    QVBoxLayout *cameraLayout = new QVBoxLayout(cameraPage);
    cameraLayout->setContentsMargins(0, 0, 0, 0);
    cameraLayout->setSpacing(8);
    QHBoxLayout *cameraFilters = new QHBoxLayout;
    m_cameraSearchEdit = new QLineEdit;
    m_cameraSearchEdit->setPlaceholderText(localizedText("搜索型号/厂家/接口", "Search model / manufacturer / interface"));
    m_cameraManufacturerFilter = new QComboBox;
    m_cameraInterfaceFilter = new QComboBox;
    QPushButton *clearCameraFilterButton = secondaryButton(localizedText("清空筛选", "Clear Filters"));
    cameraFilters->addWidget(m_cameraSearchEdit, 2);
    cameraFilters->addWidget(m_cameraManufacturerFilter);
    cameraFilters->addWidget(m_cameraInterfaceFilter);
    cameraFilters->addWidget(clearCameraFilterButton);
    cameraLayout->addLayout(cameraFilters);
    QHBoxLayout *cameraActions = new QHBoxLayout;
    QPushButton *addCameraButton = new QPushButton(localizedText("新增相机", "Add Camera"));
    QPushButton *editCameraButton = secondaryButton(localizedText("编辑", "Edit"));
    QPushButton *removeCameraButton = secondaryButton(localizedText("删除", "Delete"));
    QPushButton *importCameraButton = secondaryButton(localizedText("导入 CSV", "Import CSV"));
    QPushButton *exportCameraButton = secondaryButton(localizedText("导出 CSV", "Export CSV"));
    QPushButton *exportFilteredCameraButton = secondaryButton(localizedText("导出筛选", "Export Filtered"));
    QPushButton *resetCameraButton = secondaryButton(localizedText("重置内置", "Reset Built-in"));
    cameraActions->addWidget(addCameraButton);
    cameraActions->addWidget(editCameraButton);
    cameraActions->addWidget(removeCameraButton);
    cameraActions->addWidget(importCameraButton);
    cameraActions->addWidget(exportCameraButton);
    cameraActions->addWidget(exportFilteredCameraButton);
    cameraActions->addWidget(resetCameraButton);
    cameraActions->addStretch();
    cameraLayout->addLayout(cameraActions);
    m_cameraModel = new CatalogTableModel(CatalogDomain::Camera, this);
    m_cameraTable = new QTableView;
    setupTableView(m_cameraTable);
    m_cameraTable->setModel(m_cameraModel);
    cameraLayout->addWidget(m_cameraTable, 1);
    QHBoxLayout *cameraPager = new QHBoxLayout;
    m_cameraPrevButton = secondaryButton(localizedText("上一页", "Previous"));
    m_cameraNextButton = secondaryButton(localizedText("下一页", "Next"));
    m_cameraPageSpin = new QSpinBox;
    setupPageSpin(m_cameraPageSpin);
    m_cameraPageLabel = new QLabel;
    cameraPager->addWidget(m_cameraPageLabel);
    cameraPager->addStretch();
    cameraPager->addWidget(m_cameraPageSpin);
    cameraPager->addWidget(m_cameraPrevButton);
    cameraPager->addWidget(m_cameraNextButton);
    cameraLayout->addLayout(cameraPager);
    m_tabs->addTab(cameraPage, localizedText("相机", "Cameras"));

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
    QPushButton *clearLensFilterButton = secondaryButton(localizedText("清空筛选", "Clear Filters"));
    lensFilters->addWidget(m_lensSearchEdit, 2);
    lensFilters->addWidget(m_lensManufacturerFilter);
    lensFilters->addWidget(m_lensTypeFilter);
    lensFilters->addWidget(m_lensMountFilter);
    lensFilters->addWidget(clearLensFilterButton);
    lensLayout->addLayout(lensFilters);
    QHBoxLayout *lensActions = new QHBoxLayout;
    QPushButton *addLensButton = new QPushButton(localizedText("新增镜头", "Add Lens"));
    QPushButton *editLensButton = secondaryButton(localizedText("编辑", "Edit"));
    QPushButton *removeLensButton = secondaryButton(localizedText("删除", "Delete"));
    QPushButton *importLensButton = secondaryButton(localizedText("导入 CSV", "Import CSV"));
    QPushButton *exportLensButton = secondaryButton(localizedText("导出 CSV", "Export CSV"));
    QPushButton *exportFilteredLensButton = secondaryButton(localizedText("导出筛选", "Export Filtered"));
    QPushButton *resetLensButton = secondaryButton(localizedText("重置内置", "Reset Built-in"));
    lensActions->addWidget(addLensButton);
    lensActions->addWidget(editLensButton);
    lensActions->addWidget(removeLensButton);
    lensActions->addWidget(importLensButton);
    lensActions->addWidget(exportLensButton);
    lensActions->addWidget(exportFilteredLensButton);
    lensActions->addWidget(resetLensButton);
    lensActions->addStretch();
    lensLayout->addLayout(lensActions);
    m_lensModel = new CatalogTableModel(CatalogDomain::Lens, this);
    m_lensTable = new QTableView;
    setupTableView(m_lensTable);
    m_lensTable->setModel(m_lensModel);
    lensLayout->addWidget(m_lensTable, 1);
    QHBoxLayout *lensPager = new QHBoxLayout;
    m_lensPrevButton = secondaryButton(localizedText("上一页", "Previous"));
    m_lensNextButton = secondaryButton(localizedText("下一页", "Next"));
    m_lensPageSpin = new QSpinBox;
    setupPageSpin(m_lensPageSpin);
    m_lensPageLabel = new QLabel;
    lensPager->addWidget(m_lensPageLabel);
    lensPager->addStretch();
    lensPager->addWidget(m_lensPageSpin);
    lensPager->addWidget(m_lensPrevButton);
    lensPager->addWidget(m_lensNextButton);
    lensLayout->addLayout(lensPager);
    m_tabs->addTab(lensPage, localizedText("镜头", "Lenses"));

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
    QPushButton *clearLightFilterButton = secondaryButton(localizedText("清空筛选", "Clear Filters"));
    lightFilters->addWidget(m_lightSearchEdit, 2);
    lightFilters->addWidget(m_lightManufacturerFilter);
    lightFilters->addWidget(m_lightTypeFilter);
    lightFilters->addWidget(m_lightModeFilter);
    lightFilters->addWidget(clearLightFilterButton);
    lightLayout->addLayout(lightFilters);
    QHBoxLayout *lightActions = new QHBoxLayout;
    QPushButton *addLightButton = new QPushButton(localizedText("新增光源", "Add Light"));
    QPushButton *editLightButton = secondaryButton(localizedText("编辑", "Edit"));
    QPushButton *removeLightButton = secondaryButton(localizedText("删除", "Delete"));
    QPushButton *importLightButton = secondaryButton(localizedText("导入 CSV", "Import CSV"));
    QPushButton *exportLightButton = secondaryButton(localizedText("导出 CSV", "Export CSV"));
    QPushButton *exportFilteredLightButton = secondaryButton(localizedText("导出筛选", "Export Filtered"));
    QPushButton *resetLightButton = secondaryButton(localizedText("重置内置", "Reset Built-in"));
    lightActions->addWidget(addLightButton);
    lightActions->addWidget(editLightButton);
    lightActions->addWidget(removeLightButton);
    lightActions->addWidget(importLightButton);
    lightActions->addWidget(exportLightButton);
    lightActions->addWidget(exportFilteredLightButton);
    lightActions->addWidget(resetLightButton);
    lightActions->addStretch();
    lightLayout->addLayout(lightActions);
    m_lightModel = new CatalogTableModel(CatalogDomain::Light, this);
    m_lightTable = new QTableView;
    setupTableView(m_lightTable);
    m_lightTable->setModel(m_lightModel);
    lightLayout->addWidget(m_lightTable, 1);
    QHBoxLayout *lightPager = new QHBoxLayout;
    m_lightPrevButton = secondaryButton(localizedText("上一页", "Previous"));
    m_lightNextButton = secondaryButton(localizedText("下一页", "Next"));
    m_lightPageSpin = new QSpinBox;
    setupPageSpin(m_lightPageSpin);
    m_lightPageLabel = new QLabel;
    lightPager->addWidget(m_lightPageLabel);
    lightPager->addStretch();
    lightPager->addWidget(m_lightPageSpin);
    lightPager->addWidget(m_lightPrevButton);
    lightPager->addWidget(m_lightNextButton);
    lightLayout->addLayout(lightPager);
    m_tabs->addTab(lightPage, localizedText("光源", "Lights"));

    connect(m_tabs, &QTabWidget::currentChanged, this, [this]() { refreshCurrentPage(); });
    connect(m_cameraSearchEdit, &QLineEdit::textChanged, this, [this]() { m_cameraOffset = 0; scheduleCurrentRefresh(); });
    connect(m_lensSearchEdit, &QLineEdit::textChanged, this, [this]() { m_lensOffset = 0; scheduleCurrentRefresh(); });
    connect(m_lightSearchEdit, &QLineEdit::textChanged, this, [this]() { m_lightOffset = 0; scheduleCurrentRefresh(); });
    connect(m_cameraManufacturerFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { m_cameraOffset = 0; refreshCameraTable(); });
    connect(m_cameraInterfaceFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { m_cameraOffset = 0; refreshCameraTable(); });
    connect(m_cameraPageSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int page) {
        const int offset = (qMax(1, page) - 1) * kPageSize;
        if (offset == m_cameraOffset)
            return;
        m_cameraOffset = offset;
        refreshCameraTable();
    });
    connect(m_lensManufacturerFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { m_lensOffset = 0; refreshLensTable(); });
    connect(m_lensTypeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { m_lensOffset = 0; refreshLensTable(); });
    connect(m_lensMountFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { m_lensOffset = 0; refreshLensTable(); });
    connect(m_lensPageSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int page) {
        const int offset = (qMax(1, page) - 1) * kPageSize;
        if (offset == m_lensOffset)
            return;
        m_lensOffset = offset;
        refreshLensTable();
    });
    connect(m_lightManufacturerFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { m_lightOffset = 0; refreshLightTable(); });
    connect(m_lightTypeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { m_lightOffset = 0; refreshLightTable(); });
    connect(m_lightModeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { m_lightOffset = 0; refreshLightTable(); });
    connect(m_lightPageSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int page) {
        const int offset = (qMax(1, page) - 1) * kPageSize;
        if (offset == m_lightOffset)
            return;
        m_lightOffset = offset;
        refreshLightTable();
    });
    connect(clearCameraFilterButton, &QPushButton::clicked, this, &CatalogPage::clearCameraFilters);
    connect(clearLensFilterButton, &QPushButton::clicked, this, &CatalogPage::clearLensFilters);
    connect(clearLightFilterButton, &QPushButton::clicked, this, &CatalogPage::clearLightFilters);
    connect(m_cameraPrevButton, &QPushButton::clicked, this, [this]() { m_cameraOffset = qMax(0, m_cameraOffset - kPageSize); refreshCameraTable(); });
    connect(m_cameraNextButton, &QPushButton::clicked, this, [this]() { m_cameraOffset += kPageSize; refreshCameraTable(); });
    connect(m_lensPrevButton, &QPushButton::clicked, this, [this]() { m_lensOffset = qMax(0, m_lensOffset - kPageSize); refreshLensTable(); });
    connect(m_lensNextButton, &QPushButton::clicked, this, [this]() { m_lensOffset += kPageSize; refreshLensTable(); });
    connect(m_lightPrevButton, &QPushButton::clicked, this, [this]() { m_lightOffset = qMax(0, m_lightOffset - kPageSize); refreshLightTable(); });
    connect(m_lightNextButton, &QPushButton::clicked, this, [this]() { m_lightOffset += kPageSize; refreshLightTable(); });

    connect(addCameraButton, &QPushButton::clicked, this, &CatalogPage::cameraAddRequested);
    connect(editCameraButton, &QPushButton::clicked, this, &CatalogPage::cameraEditRequested);
    connect(removeCameraButton, &QPushButton::clicked, this, &CatalogPage::cameraRemoveRequested);
    connect(importCameraButton, &QPushButton::clicked, this, &CatalogPage::cameraImportRequested);
    connect(exportCameraButton, &QPushButton::clicked, this, &CatalogPage::cameraExportRequested);
    connect(exportFilteredCameraButton, &QPushButton::clicked, this, &CatalogPage::cameraExportFilteredRequested);
    connect(resetCameraButton, &QPushButton::clicked, this, &CatalogPage::cameraResetRequested);
    connect(m_cameraTable, &QTableView::doubleClicked, this, [this](const QModelIndex &) { emit cameraEditRequested(); });

    connect(addLensButton, &QPushButton::clicked, this, &CatalogPage::lensAddRequested);
    connect(editLensButton, &QPushButton::clicked, this, &CatalogPage::lensEditRequested);
    connect(removeLensButton, &QPushButton::clicked, this, &CatalogPage::lensRemoveRequested);
    connect(importLensButton, &QPushButton::clicked, this, &CatalogPage::lensImportRequested);
    connect(exportLensButton, &QPushButton::clicked, this, &CatalogPage::lensExportRequested);
    connect(exportFilteredLensButton, &QPushButton::clicked, this, &CatalogPage::lensExportFilteredRequested);
    connect(resetLensButton, &QPushButton::clicked, this, &CatalogPage::lensResetRequested);
    connect(m_lensTable, &QTableView::doubleClicked, this, [this](const QModelIndex &) { emit lensEditRequested(); });

    connect(addLightButton, &QPushButton::clicked, this, &CatalogPage::lightAddRequested);
    connect(editLightButton, &QPushButton::clicked, this, &CatalogPage::lightEditRequested);
    connect(removeLightButton, &QPushButton::clicked, this, &CatalogPage::lightRemoveRequested);
    connect(importLightButton, &QPushButton::clicked, this, &CatalogPage::lightImportRequested);
    connect(exportLightButton, &QPushButton::clicked, this, &CatalogPage::lightExportRequested);
    connect(exportFilteredLightButton, &QPushButton::clicked, this, &CatalogPage::lightExportFilteredRequested);
    connect(resetLightButton, &QPushButton::clicked, this, &CatalogPage::lightResetRequested);
    connect(m_lightTable, &QTableView::doubleClicked, this, [this](const QModelIndex &) { emit lightEditRequested(); });

    layout->addWidget(m_tabs, 1);
    updatePageLabel(CatalogDomain::Camera);
    updatePageLabel(CatalogDomain::Lens);
    updatePageLabel(CatalogDomain::Light);
}

void CatalogPage::setCatalog(const CatalogRepository *catalog)
{
    m_catalog = catalog;
    refreshCatalogFilterOptions();
    refreshCurrentPage();
}

void CatalogPage::refreshCurrentPage()
{
    if (!m_tabs)
        return;
    if (m_tabs->currentIndex() == 0)
        refreshCameraTable();
    else if (m_tabs->currentIndex() == 1)
        refreshLensTable();
    else
        refreshLightTable();
}

void CatalogPage::scheduleCurrentRefresh()
{
    if (m_filterTimer)
        m_filterTimer->start();
}

void CatalogPage::refreshCatalogFilterOptions()
{
    if (!m_catalog)
        return;
    QString filterError;
    const auto values = [this, &filterError](CatalogDomain domain, const QString &field) {
        QString error;
        const QStringList result = m_catalog->distinctValues(domain, field, &error);
        if (!error.isEmpty() && filterError.isEmpty())
            filterError = error;
        return result;
    };

    fillComboPreservingText(m_cameraManufacturerFilter, allManufacturersText(), values(CatalogDomain::Camera, QStringLiteral("manufacturer")));
    fillComboPreservingText(m_cameraInterfaceFilter, allInterfacesText(), values(CatalogDomain::Camera, QStringLiteral("interface")));
    fillComboPreservingText(m_lensManufacturerFilter, allManufacturersText(), values(CatalogDomain::Lens, QStringLiteral("manufacturer")));
    fillComboPreservingText(m_lensTypeFilter, allTypesText(), values(CatalogDomain::Lens, QStringLiteral("lens_type")));
    fillComboPreservingText(m_lensMountFilter, allMountsText(), values(CatalogDomain::Lens, QStringLiteral("lens_mount")));
    fillComboPreservingText(m_lightManufacturerFilter, allManufacturersText(), values(CatalogDomain::Light, QStringLiteral("manufacturer")));
    fillComboPreservingText(m_lightTypeFilter, allTypesText(), values(CatalogDomain::Light, QStringLiteral("light_type")));
    fillComboPreservingText(m_lightModeFilter, allModesText(), values(CatalogDomain::Light, QStringLiteral("mode")));

    if (!filterError.isEmpty()) {
        QLabel *label = m_tabs && m_tabs->currentIndex() == 1 ? m_lensPageLabel
            : (m_tabs && m_tabs->currentIndex() == 2 ? m_lightPageLabel : m_cameraPageLabel);
        if (label)
            label->setText(catalogErrorText(filterError));
    }
}

CatalogQuery CatalogPage::currentCameraQuery() const
{
    CatalogQuery query;
    query.search = m_cameraSearchEdit ? m_cameraSearchEdit->text() : QString();
    query.manufacturer = m_cameraManufacturerFilter && m_cameraManufacturerFilter->currentIndex() > 0 ? m_cameraManufacturerFilter->currentText() : QString();
    query.interfaceType = m_cameraInterfaceFilter && m_cameraInterfaceFilter->currentIndex() > 0 ? m_cameraInterfaceFilter->currentText() : QString();
    query.offset = m_cameraOffset;
    query.limit = kPageSize;
    query.sort.field = QStringLiteral("model");
    query.sort.ascending = true;
    return query;
}

CatalogQuery CatalogPage::currentLensQuery() const
{
    CatalogQuery query;
    query.search = m_lensSearchEdit ? m_lensSearchEdit->text() : QString();
    query.manufacturer = m_lensManufacturerFilter && m_lensManufacturerFilter->currentIndex() > 0 ? m_lensManufacturerFilter->currentText() : QString();
    query.type = m_lensTypeFilter && m_lensTypeFilter->currentIndex() > 0 ? m_lensTypeFilter->currentText() : QString();
    query.lensMount = m_lensMountFilter && m_lensMountFilter->currentIndex() > 0 ? m_lensMountFilter->currentText() : QString();
    query.offset = m_lensOffset;
    query.limit = kPageSize;
    query.sort.field = QStringLiteral("model");
    query.sort.ascending = true;
    return query;
}

CatalogQuery CatalogPage::currentLightQuery() const
{
    CatalogQuery query;
    query.search = m_lightSearchEdit ? m_lightSearchEdit->text() : QString();
    query.manufacturer = m_lightManufacturerFilter && m_lightManufacturerFilter->currentIndex() > 0 ? m_lightManufacturerFilter->currentText() : QString();
    query.type = m_lightTypeFilter && m_lightTypeFilter->currentIndex() > 0 ? m_lightTypeFilter->currentText() : QString();
    query.mode = m_lightModeFilter && m_lightModeFilter->currentIndex() > 0 ? m_lightModeFilter->currentText() : QString();
    query.offset = m_lightOffset;
    query.limit = kPageSize;
    query.sort.field = QStringLiteral("model");
    query.sort.ascending = true;
    return query;
}

void CatalogPage::refreshCameraTable()
{
    if (!m_catalog || !m_cameraModel)
        return;
    QString error;
    const CatalogPageResult<CameraSpec> page = m_catalog->queryCameras(currentCameraQuery(), &error);
    if (!error.isEmpty()) {
        m_cameraTotal = 0;
        m_cameraModel->setCameras(QVector<qint64>(), QVector<CameraSpec>());
        if (m_cameraPageLabel)
            m_cameraPageLabel->setText(catalogErrorText(error));
        updatePageSpin(m_cameraPageSpin, 0, 0, kPageSize);
        if (m_cameraPrevButton)
            m_cameraPrevButton->setEnabled(false);
        if (m_cameraNextButton)
            m_cameraNextButton->setEnabled(false);
        return;
    }
    m_cameraTotal = page.totalCount;
    if (m_cameraOffset >= m_cameraTotal && m_cameraOffset > 0) {
        m_cameraOffset = qMax(0, ((m_cameraTotal - 1) / kPageSize) * kPageSize);
        refreshCameraTable();
        return;
    }
    m_cameraModel->setCameras(page.ids, page.items);
    updatePageLabel(CatalogDomain::Camera);
    if (!page.items.isEmpty())
        m_cameraTable->selectRow(0);
}

void CatalogPage::refreshLensTable()
{
    if (!m_catalog || !m_lensModel)
        return;
    QString error;
    const CatalogPageResult<LensSpec> page = m_catalog->queryLenses(currentLensQuery(), &error);
    if (!error.isEmpty()) {
        m_lensTotal = 0;
        m_lensModel->setLenses(QVector<qint64>(), QVector<LensSpec>());
        if (m_lensPageLabel)
            m_lensPageLabel->setText(catalogErrorText(error));
        updatePageSpin(m_lensPageSpin, 0, 0, kPageSize);
        if (m_lensPrevButton)
            m_lensPrevButton->setEnabled(false);
        if (m_lensNextButton)
            m_lensNextButton->setEnabled(false);
        return;
    }
    m_lensTotal = page.totalCount;
    if (m_lensOffset >= m_lensTotal && m_lensOffset > 0) {
        m_lensOffset = qMax(0, ((m_lensTotal - 1) / kPageSize) * kPageSize);
        refreshLensTable();
        return;
    }
    m_lensModel->setLenses(page.ids, page.items);
    updatePageLabel(CatalogDomain::Lens);
    if (!page.items.isEmpty())
        m_lensTable->selectRow(0);
}

void CatalogPage::refreshLightTable()
{
    if (!m_catalog || !m_lightModel)
        return;
    QString error;
    const CatalogPageResult<LightSpec> page = m_catalog->queryLights(currentLightQuery(), &error);
    if (!error.isEmpty()) {
        m_lightTotal = 0;
        m_lightModel->setLights(QVector<qint64>(), QVector<LightSpec>());
        if (m_lightPageLabel)
            m_lightPageLabel->setText(catalogErrorText(error));
        updatePageSpin(m_lightPageSpin, 0, 0, kPageSize);
        if (m_lightPrevButton)
            m_lightPrevButton->setEnabled(false);
        if (m_lightNextButton)
            m_lightNextButton->setEnabled(false);
        return;
    }
    m_lightTotal = page.totalCount;
    if (m_lightOffset >= m_lightTotal && m_lightOffset > 0) {
        m_lightOffset = qMax(0, ((m_lightTotal - 1) / kPageSize) * kPageSize);
        refreshLightTable();
        return;
    }
    m_lightModel->setLights(page.ids, page.items);
    updatePageLabel(CatalogDomain::Light);
    if (!page.items.isEmpty())
        m_lightTable->selectRow(0);
}

void CatalogPage::updatePageLabel(CatalogDomain domain)
{
    if (domain == CatalogDomain::Camera) {
        const int shown = m_cameraModel ? m_cameraModel->rowCount() : 0;
        if (m_cameraPageLabel)
            m_cameraPageLabel->setText(pageText(m_cameraTotal, m_cameraOffset, shown));
        updatePageSpin(m_cameraPageSpin, m_cameraTotal, m_cameraOffset, kPageSize);
        if (m_cameraPrevButton)
            m_cameraPrevButton->setEnabled(m_cameraOffset > 0);
        if (m_cameraNextButton)
            m_cameraNextButton->setEnabled(m_cameraOffset + shown < m_cameraTotal);
    } else if (domain == CatalogDomain::Lens) {
        const int shown = m_lensModel ? m_lensModel->rowCount() : 0;
        if (m_lensPageLabel)
            m_lensPageLabel->setText(pageText(m_lensTotal, m_lensOffset, shown));
        updatePageSpin(m_lensPageSpin, m_lensTotal, m_lensOffset, kPageSize);
        if (m_lensPrevButton)
            m_lensPrevButton->setEnabled(m_lensOffset > 0);
        if (m_lensNextButton)
            m_lensNextButton->setEnabled(m_lensOffset + shown < m_lensTotal);
    } else {
        const int shown = m_lightModel ? m_lightModel->rowCount() : 0;
        if (m_lightPageLabel)
            m_lightPageLabel->setText(pageText(m_lightTotal, m_lightOffset, shown));
        updatePageSpin(m_lightPageSpin, m_lightTotal, m_lightOffset, kPageSize);
        if (m_lightPrevButton)
            m_lightPrevButton->setEnabled(m_lightOffset > 0);
        if (m_lightNextButton)
            m_lightNextButton->setEnabled(m_lightOffset + shown < m_lightTotal);
    }
}

qint64 CatalogPage::selectedId(const QTableView *view, const CatalogTableModel *model) const
{
    if (!view || !model || !view->selectionModel())
        return -1;
    const QModelIndexList rows = view->selectionModel()->selectedRows();
    if (rows.isEmpty())
        return -1;
    return model->idAt(rows.first().row());
}

qint64 CatalogPage::selectedCameraId() const
{
    return selectedId(m_cameraTable, m_cameraModel);
}

qint64 CatalogPage::selectedLensId() const
{
    return selectedId(m_lensTable, m_lensModel);
}

qint64 CatalogPage::selectedLightId() const
{
    return selectedId(m_lightTable, m_lightModel);
}

int CatalogPage::selectedIndex(CatalogDomain domain) const
{
    if (!m_catalog)
        return -1;
    if (domain == CatalogDomain::Camera)
        return m_catalog->cameraIndexById(selectedCameraId());
    if (domain == CatalogDomain::Lens)
        return m_catalog->lensIndexById(selectedLensId());
    return m_catalog->lightIndexById(selectedLightId());
}

int CatalogPage::selectedCameraCatalogIndex() const
{
    return selectedIndex(CatalogDomain::Camera);
}

int CatalogPage::selectedLensCatalogIndex() const
{
    return selectedIndex(CatalogDomain::Lens);
}

int CatalogPage::selectedLightCatalogIndex() const
{
    return selectedIndex(CatalogDomain::Light);
}

QVector<int> CatalogPage::visibleCameraCatalogIndexes() const
{
    QVector<int> indexes;
    if (!m_catalog || !m_cameraModel)
        return indexes;
    for (int row = 0; row < m_cameraModel->rowCount(); ++row) {
        const int index = m_catalog->cameraIndexById(m_cameraModel->idAt(row));
        if (index >= 0)
            indexes.append(index);
    }
    return indexes;
}

QVector<int> CatalogPage::visibleLensCatalogIndexes() const
{
    QVector<int> indexes;
    if (!m_catalog || !m_lensModel)
        return indexes;
    for (int row = 0; row < m_lensModel->rowCount(); ++row) {
        const int index = m_catalog->lensIndexById(m_lensModel->idAt(row));
        if (index >= 0)
            indexes.append(index);
    }
    return indexes;
}

QVector<int> CatalogPage::visibleLightCatalogIndexes() const
{
    QVector<int> indexes;
    if (!m_catalog || !m_lightModel)
        return indexes;
    for (int row = 0; row < m_lightModel->rowCount(); ++row) {
        const int index = m_catalog->lightIndexById(m_lightModel->idAt(row));
        if (index >= 0)
            indexes.append(index);
    }
    return indexes;
}

void CatalogPage::clearCameraFilters()
{
    if (m_cameraSearchEdit)
        m_cameraSearchEdit->clear();
    if (m_cameraManufacturerFilter)
        m_cameraManufacturerFilter->setCurrentIndex(0);
    if (m_cameraInterfaceFilter)
        m_cameraInterfaceFilter->setCurrentIndex(0);
    m_cameraOffset = 0;
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
    m_lensOffset = 0;
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
    m_lightOffset = 0;
    refreshLightTable();
}
