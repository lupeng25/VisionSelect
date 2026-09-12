#include "ui/pages/ThreeDCameraPage.h"

#include "ui/CatalogDialogs.h"
#include "ui/UiHelpers.h"
#include "ui/UiSettings.h"

#include <QAction>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDate>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QList>
#include <QMessageBox>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QSplitter>
#include <QSpinBox>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBrowser>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

using namespace UiHelpers;

namespace {
QString localizedSpecText(QString value);

class FilterScrollArea : public QScrollArea
{
public:
    using QScrollArea::QScrollArea;
    QSize sizeHint() const override
    {
        return widget() ? widget()->sizeHint() + QSize(4, 4) : QScrollArea::sizeHint();
    }
    QSize minimumSizeHint() const override { return QSize(320, 160); }
};

QString allText()
{
    return localizedText("全部", "All");
}

QDoubleSpinBox *optionalSpin(double max, const QString &suffix, int decimals = 1)
{
    QDoubleSpinBox *spin = new QDoubleSpinBox;
    spin->setRange(0.0, max);
    spin->setDecimals(decimals);
    spin->setSpecialValueText(localizedText("不限", "Any"));
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);
    return spin;
}

double optionalValue(const QDoubleSpinBox *spin)
{
    if (!spin || spin->value() <= 0.0)
        return -1.0;
    return spin->value();
}

void addComboValues(QComboBox *combo, const QStringList &values)
{
    const QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem(allText(), QString());
    QStringList sorted = values;
    sorted.removeDuplicates();
    sorted.sort(Qt::CaseInsensitive);
    for (const QString &value : sorted) {
        if (!value.trimmed().isEmpty())
            combo->addItem(localizedSpecText(value), value);
    }
}

QString comboRequirement(const QComboBox *combo)
{
    if (!combo || combo->currentIndex() <= 0)
        return QString();
    const QString value = combo->currentData().toString();
    return value.isEmpty() ? combo->currentText() : value;
}

QString valueOrUnknown(double value, const QString &unit, int decimals = 1)
{
    return threeDHasValue(value)
        ? QStringLiteral("%1%2").arg(value, 0, 'f', decimals).arg(unit)
        : localizedText("未公开", "Unpublished");
}

QString linearityOrUnknown(double value)
{
    return threeDHasValue(value)
        ? QStringLiteral("+/- %1% F.S.").arg(value, 0, 'f', 3)
        : localizedText("未公开", "Unpublished");
}

QString intOrUnknown(int value)
{
    return value >= 0 ? QString::number(value) : localizedText("未公开", "Unpublished");
}

QString fovRange(double nearValue, double referenceValue, double farValue, const QString &unit)
{
    QStringList values;
    if (threeDHasValue(nearValue))
        values.append(localizedText("近端 %1%2", "Near %1%2").arg(nearValue, 0, 'f', 1).arg(unit));
    if (threeDHasValue(referenceValue))
        values.append(localizedText("参考 %1%2", "Reference %1%2").arg(referenceValue, 0, 'f', 1).arg(unit));
    if (threeDHasValue(farValue))
        values.append(localizedText("远端 %1%2", "Far %1%2").arg(farValue, 0, 'f', 1).arg(unit));
    return values.isEmpty() ? localizedText("未公开", "Unpublished") : values.join(localizedText("，", ", "));
}

QString geometrySummary(const ThreeDCameraSpec &spec)
{
    return localizedText("X：%1；Y：%2；Z：%3；参考距离：%4", "X: %1; Y: %2; Z: %3; Reference: %4")
        .arg(fovRange(spec.xFovNearMm, spec.xFovReferenceMm, spec.xFovFarMm, QStringLiteral(" mm")))
        .arg(fovRange(spec.yFovNearMm, spec.yFovReferenceMm, spec.yFovFarMm, QStringLiteral(" mm")))
        .arg(valueOrUnknown(spec.zMeasurementRangeMm, QStringLiteral(" mm"), 1))
        .arg(valueOrUnknown(spec.referenceDistanceMm, QStringLiteral(" mm"), 1));
}

QString qualitySummary(const ThreeDCameraSpec &spec)
{
    QStringList parts;
    if (threeDHasValue(spec.zRepeatabilityUm))
        parts.append(localizedText("Z重复 %1 um", "Z repeatability %1 um").arg(spec.zRepeatabilityUm, 0, 'f', 2));
    if (threeDHasValue(spec.xRepeatabilityUm))
        parts.append(localizedText("X重复 %1 um", "X repeatability %1 um").arg(spec.xRepeatabilityUm, 0, 'f', 2));
    if (threeDHasValue(spec.profileDataIntervalUm))
        parts.append(localizedText("X间隔 %1 um", "X interval %1 um").arg(spec.profileDataIntervalUm, 0, 'f', 2));
    if (threeDHasValue(spec.zLinearityPercentOfRange))
        parts.append(localizedText("Z线性 %1", "Z linearity %1").arg(linearityOrUnknown(spec.zLinearityPercentOfRange)));
    if (threeDHasValue(spec.zResolutionUm))
        parts.append(localizedText("Z分辨 %1 um", "Z resolution %1 um").arg(spec.zResolutionUm, 0, 'f', 2));
    if (threeDHasValue(spec.measurementAccuracyUm))
        parts.append(localizedText("测量精度 %1 um", "Accuracy %1 um").arg(spec.measurementAccuracyUm, 0, 'f', 1));
    return parts.isEmpty() ? localizedText("未公开", "Unpublished") : parts.join(localizedText("；", "; "));
}

QString speedSummary(const ThreeDCameraSpec &spec)
{
    QStringList parts;
    if (spec.profilePoints >= 0)
        parts.append(localizedText("%1 点/轮廓", "%1 points/profile").arg(spec.profilePoints));
    if (threeDHasValue(spec.scanRateMaxHz)) {
        if (threeDHasValue(spec.scanRateMinHz))
            parts.append(QStringLiteral("%1-%2 Hz").arg(spec.scanRateMinHz, 0, 'f', 0).arg(spec.scanRateMaxHz, 0, 'f', 0));
        else
            parts.append(QStringLiteral("%1 Hz").arg(spec.scanRateMaxHz, 0, 'f', 0));
    }
    if (threeDHasValue(spec.frameRateHz))
        parts.append(QStringLiteral("%1 fps").arg(spec.frameRateHz, 0, 'f', 1));
    if (threeDHasValue(spec.acquisitionTimeMs))
        parts.append(QStringLiteral("%1 ms").arg(spec.acquisitionTimeMs, 0, 'f', 1));
    return parts.isEmpty() ? localizedText("未公开", "Unpublished") : parts.join(localizedText("；", "; "));
}

QString yesNoUnknown(int value)
{
    if (value < 0)
        return localizedText("未公开", "Unpublished");
    return value > 0 ? localizedText("是", "Yes") : localizedText("否", "No");
}

struct SpecValueReplacement
{
    const char *zh;
    const char *en;
};

bool englishUi()
{
    return localizedText("中文", "English") == QLatin1String("English");
}

QString localizedSpecText(QString value)
{
    value = value.trimmed();
    if (value.isEmpty())
        return value;

    static const SpecValueReplacement replacements[] = {
        {"在售", "On sale"},
        {"已发布", "Released"},
        {"已停产", "Discontinued"},
        {"停产", "Discontinued"},
        {"用户录入", "User entered"},
        {"未公开", "Unpublished"},
        {"蓝色激光", "Blue laser"},
        {"蓝色结构光", "Blue structured light"},
        {"结构光蓝光", "Blue structured light"},
        {"点光谱共焦", "Point spectral confocal"},
        {"光谱共焦白光/多波长", "Spectral confocal white light / multi-wavelength"},
        {"光谱共焦", "Spectral confocal"},
        {"彩色共焦/彩色激光同轴", "Color confocal / coaxial color laser"},
        {"线共焦白光/多波长", "Line confocal white light / multi-wavelength"},
        {"线共焦", "Line confocal"},
        {"蓝色/红色激光", "Blue / red laser"},
        {"红色半导体激光", "Red semiconductor laser"},
        {"蓝色半导体激光", "Blue semiconductor laser"},
        {"红外 SLD / 白光干涉", "Infrared SLD / white-light interferometry"},
        {"双目结构光", "Stereo structured light"},
        {"结构光/激光投影", "Structured light / laser projection"},
        {"结构光", "Structured light"},
        {"激光", "Laser"},
        {"编码器", "Encoder"},
        {"高度轮廓", "Height profile"},
        {"测量工具结果", "Measurement tool result"},
        {"轮廓", "Profile"},
        {"高度图", "Height map"},
        {"高度", "Height"},
        {"强度图", "Intensity map"},
        {"强度", "Intensity"},
        {"厚度", "Thickness"},
        {"点云", "Point cloud"},
        {"粗糙度/外观", "Roughness / appearance"},
        {"深度图", "Depth map"},
        {"木材", "Wood"},
        {"大尺寸板材", "Large panels"},
        {"无需完整面阵快照", "No full-area snapshot required"},
        {"通用", "General purpose"},
        {"暗色", "Dark surfaces"},
        {"反光", "Reflective"},
        {"高精度", "High precision"},
        {"微小零件", "Small parts"},
        {"大视野", "Large FOV"},
        {"高速在线检测", "High-speed inline inspection"},
        {"小视野", "Small FOV"},
        {"高分辨率", "High resolution"},
        {"高反光复杂结构", "Highly reflective complex structures"},
        {"钢壳电池外壳", "Steel-shell battery housing"},
        {"手机中框内壁", "Phone mid-frame inner wall"},
        {"拐角", "corner"},
        {"槽", "groove"},
        {"遮挡/复杂外形", "Occlusion / complex shape"},
        {"静态零件", "Static parts"},
        {"无需运动平台", "No motion stage required"},
        {"机器人搭载", "Robot-mounted"},
        {"孔/槽/螺柱/间隙面差", "holes / slots / studs / gap step height"},
        {"微小电子元件", "Small electronic components"},
        {"透明膜厚", "Transparent film thickness"},
        {"透明物体厚度", "Transparent object thickness"},
        {"透明", "Transparent"},
        {"镜面反射目标", "Specular reflective targets"},
        {"镜面", "Mirror-like"},
        {"高反光", "Highly reflective"},
        {"多层材料", "Multi-layer materials"},
        {"玻璃", "Glass"},
        {"精密小件", "Precision small parts"},
        {"影像仪", "Vision measuring machines"},
        {"工业检测", "Industrial inspection"},
        {"高速", "High speed"},
        {"位移测量", "Displacement measurement"},
        {"漫反射目标", "Diffuse reflective targets"},
        {"黑色", "Black"},
        {"物流", "Logistics"},
        {"机器人", "Robot"},
        {"抓取定位", "Picking positioning"},
        {"定位", "Positioning"},
        {"模块化多点轮廓扫描器", "Modular multi-point profile scanner"},
        {"一体式智能传感器", "Integrated smart sensor"},
        {"一体式双相机 3D 线激光轮廓传感器", "Integrated dual-camera 3D line laser profile sensor"},
        {"一体式 3D 线激光传感器", "Integrated 3D line laser sensor"},
        {"分体式 3D 线激光传感器", "Split 3D line laser sensor"},
        {"高性能 3D 线激光传感器", "High-performance 3D line laser sensor"},
        {"传感器头 + 控制器", "Sensor head + controller"},
        {"激光振镜立体相机", "Laser galvanometer stereo camera"},
        {"一体式 3D 线激光相机", "Integrated 3D line laser camera"},
        {"3D激光轮廓传感器", "3D laser profile sensor"},
        {"3D线激光轮廓传感器", "3D line laser profile sensor"},
        {"高分辨率 3D 线激光相机", "High-resolution 3D line laser camera"},
        {"投影结构光立体相机", "Projected structured-light stereo camera"},
        {"一体式结构光 3D 相机", "Integrated structured-light 3D camera"},
        {"3D结构光传感器", "3D structured-light sensor"},
        {"官方公开规格", "Official published specifications"},
        {"官方快速规格", "Official quick specifications"},
        {"官方规格页", "Official specifications page"},
        {"官方中文数据表", "Official Chinese datasheet"},
        {"官网公开规格", "Official website published specifications"},
        {"官网新闻", "official website news"},
        {"下载中心", "download center"},
        {"官网产品页/参数页公开字段", "published fields from official product / parameter pages"},
        {"官网产品页公开", "published on the official product page"},
        {"官网产品列表公开", "published on the official product list"},
        {"测试条件按官方脚注", "test conditions follow official footnotes"},
        {"测试条件未公开", "test conditions unpublished"},
        {"详细测试条件未公开", "detailed test conditions unpublished"},
        {"具体测试条件未公开", "specific test conditions unpublished"},
        {"测量条件", "measurement conditions"},
        {"重复性", "repeatability"},
        {"标准目标", "standard target"},
        {"优化配置脚注", "optimized configuration footnote"},
        {"官网脚注条件", "official website footnote conditions"},
        {"官网脚注定义", "official website footnote definition"},
        {"按官网定义", "defined by the official website"},
        {"按官网表格", "from the official website table"},
        {"以官网下发的", "based on the official"},
        {"产品规格 ZIP 为准", "product specifications ZIP"},
        {"同一官方产品行的特殊定制版本", "special custom version in the same official product line"},
        {"特殊定制版本", "special custom version"},
        {"具体选型需咨询业务端", "consult sales for model selection"},
        {"官网型号列表以", "official model list shows"},
        {"组合展示", "as a paired listing"},
        {"标注为", "marked as"},
        {"新增", "adds"},
        {"大视野相机", "large-FOV cameras"},
        {"实现", "covering"},
        {"线宽测量范围覆盖", "line-width measurement range"},
        {"网页未直接展开逐项规格", "the webpage does not expand each specification item"},
        {"数值字段暂不臆造", "numeric fields are not inferred"},
        {"可达亚微米级", "can reach sub-micron level"},
        {"精度/重复精度", "accuracy / repeatability"},
        {"使用标准目标", "using a standard target"},
        {"指定矩形区域", "specified rectangular area"},
        {"测量", "measurement"},
        {"条件", "conditions"},
        {"按参考距离", "at reference distance"},
        {"次平均", "averages"},
        {"基恩士", "KEYENCE"},
        {"深视智能", "SRI"},
        {"官网", "official website"},
        {"产品页", "product page"},
        {"参数页", "parameter page"},
        {"规格页", "specification page"},
        {"公开字段", "published fields"},
        {"脚注", "footnote"},
        {"，", ", "},
        {"；", "; "},
        {"。", "."}
    };

    if (englishUi()) {
        for (const SpecValueReplacement &replacement : replacements)
            value.replace(QString::fromUtf8(replacement.zh), QString::fromUtf8(replacement.en), Qt::CaseSensitive);
        return value;
    }
    for (const SpecValueReplacement &replacement : replacements) {
        if (value.compare(QString::fromUtf8(replacement.en), Qt::CaseInsensitive) == 0)
            return QString::fromUtf8(replacement.zh);
    }
    return value;
}

QString localizedSpecList(const QStringList &values, const QString &separator)
{
    QStringList localized;
    localized.reserve(values.size());
    for (const QString &value : values)
        localized.append(localizedSpecText(value));
    return localized.join(separator);
}

QString htmlEscape(const QString &text)
{
    QString escaped = text.toHtmlEscaped();
    escaped.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    return escaped;
}

QString rawSpecsHtml(const QJsonObject &rawSpecs)
{
    if (rawSpecs.isEmpty())
        return QStringLiteral("<p>%1</p>").arg(htmlEscape(localizedText("未公开", "Unpublished")));

    QString html = QStringLiteral("<table cellspacing=\"0\" cellpadding=\"4\">");
    const QStringList keys = rawSpecs.keys();
    for (const QString &key : keys) {
        const QJsonValue value = rawSpecs.value(key);
        QString text;
        if (value.isArray())
            text = QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
        else if (value.isObject())
            text = QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
        else if (value.isDouble())
            text = QString::number(value.toDouble());
        else if (value.isBool())
            text = value.toBool() ? localizedText("是", "Yes") : localizedText("否", "No");
        else
            text = value.toString();
        html += QStringLiteral("<tr><td><b>%1</b></td><td>%2</td></tr>")
            .arg(htmlEscape(key), htmlEscape(text));
    }
    html += QStringLiteral("</table>");
    return html;
}

void setStatusColor(QTableWidgetItem *item, ThreeDMatchStatus status)
{
    if (!item)
        return;
    if (status == ThreeDMatchStatus::Match)
        item->setBackground(QColor(220, 252, 231));
    else if (status == ThreeDMatchStatus::MissingData)
        item->setBackground(QColor(254, 249, 195));
    else
        item->setBackground(QColor(254, 226, 226));
}

QString htmlList(const QStringList &items, const QString &fallback)
{
    if (items.isEmpty())
        return QStringLiteral("<p>%1</p>").arg(htmlEscape(fallback));
    QString html = QStringLiteral("<ul>");
    for (const QString &entry : items)
        html += QStringLiteral("<li>%1</li>").arg(htmlEscape(entry));
    html += QStringLiteral("</ul>");
    return html;
}

bool sameCameraIdentity(const ThreeDCameraSpec &left, const ThreeDCameraSpec &right)
{
    return left.manufacturer.trimmed().compare(right.manufacturer.trimmed(), Qt::CaseInsensitive) == 0
        && left.series.trimmed().compare(right.series.trimmed(), Qt::CaseInsensitive) == 0
        && left.model.trimmed().compare(right.model.trimmed(), Qt::CaseInsensitive) == 0;
}

QString sourceTypeText(const ThreeDCameraSpec &spec)
{
    return spec.userDefined ? localizedText("自定义", "Custom") : localizedText("内置", "Built-in");
}

QString triggerModeText(ThreeDTriggerMode mode)
{
    switch (mode) {
    case ThreeDTriggerMode::FreeRun:
        return localizedText("自由运行", "Free running");
    case ThreeDTriggerMode::ExternalTrigger:
        return localizedText("外部线触发", "External line trigger");
    case ThreeDTriggerMode::Encoder:
        return localizedText("编码器触发", "Encoder trigger");
    }
    return localizedText("自由运行", "Free running");
}
}

ThreeDCameraPage::ThreeDCameraPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 14, 20, 14);
    layout->setSpacing(10);
    layout->addWidget(pageHeader(localizedText("3D 相机助手", "3D Camera Assistant"),
        localizedText("按几何范围、精度、速度和集成条件筛选 3D 相机资料库。", "Filter the 3D camera library by geometry, accuracy, speed, and integration constraints.")));

    QTabWidget *tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("ThreeDTasks"));
    QWidget *productTab = new QWidget(tabs);
    QVBoxLayout *productLayout = new QVBoxLayout(productTab);
    productLayout->setContentsMargins(0, 0, 0, 0);
    productLayout->setSpacing(8);

    buildProductToolbar(productLayout);
    buildFilters(productLayout);

    m_productSplitter = new QSplitter(Qt::Vertical, productTab);
    m_productSplitter->setObjectName(QStringLiteral("threeD/browse"));
    m_productSplitter->setChildrenCollapsible(false);
    m_table = new QTableWidget(m_productSplitter);
    m_table->setObjectName(QStringLiteral("threeD/table"));
    m_table->setAccessibleName(localizedText("3D 相机匹配结果", "3D camera matching results"));
    setupTable(m_table);
    m_table->setSortingEnabled(false);
    m_table->setWordWrap(false);
    m_table->setTextElideMode(Qt::ElideRight);
    m_table->verticalHeader()->setDefaultSectionSize(UiSettings::tableRowHeight());
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_table->setColumnCount(10);
    m_table->setHorizontalHeaderLabels({
        localizedText("匹配状态", "Status"),
        localizedText("来源", "Source"),
        localizedText("品牌", "Brand"),
        localizedText("系列", "Series"),
        localizedText("型号", "Model"),
        localizedText("技术路线", "Technology"),
        localizedText("几何范围", "Geometry"),
        localizedText("精度质量", "Quality"),
        localizedText("速度采样", "Speed"),
        localizedText("集成接口", "Integration")
    });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    // 优先保证状态和型号可读，长参数保留提示、可调列宽与横向滚动。
    m_table->setColumnWidth(0, 90);
    m_table->setColumnWidth(1, englishUi() ? 70 : 55);
    m_table->setColumnWidth(2, 70);
    m_table->setColumnWidth(3, 95);
    m_table->setColumnWidth(4, 150);
    m_table->setColumnWidth(5, 125);
    m_table->setColumnWidth(6, 155);
    m_table->setColumnWidth(7, 175);
    m_table->setColumnWidth(8, 130);
    m_table->setColumnWidth(9, 135);
    for (int column = 0; column < m_table->columnCount(); ++column)
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(true);
    connect(m_table, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) { showDetailsForRow(row); });
    const auto openDetails = [this](int, int) { m_detailsToggle->setChecked(true); };
    connect(m_table, &QTableWidget::cellDoubleClicked, this, openDetails);
    connect(m_table, &QTableWidget::cellActivated, this, openDetails);
    m_productSplitter->addWidget(m_table);

    m_detailsPanel = new QFrame(m_productSplitter);
    m_detailsPanel->setObjectName(QStringLiteral("ThreeDDetailsPanel"));
    auto *detailsLayout = new QVBoxLayout(m_detailsPanel);
    detailsLayout->setContentsMargins(8, 6, 8, 6);
    detailsLayout->setSpacing(4);
    auto *detailsHeading = new QHBoxLayout;
    auto *detailsTitle = new QLabel(localizedText("型号详情", "Model details"), m_detailsPanel);
    detailsTitle->setObjectName(QStringLiteral("SectionTitle"));
    auto *closeDetails = new QPushButton(localizedText("收起详情", "Hide details"), m_detailsPanel);
    closeDetails->setObjectName(QStringLiteral("ThreeDDetailsClose"));
    detailsHeading->addWidget(detailsTitle);
    detailsHeading->addStretch();
    detailsHeading->addWidget(closeDetails);
    detailsLayout->addLayout(detailsHeading);
    m_details = new QTextBrowser(m_detailsPanel);
    m_details->setAccessibleName(localizedText("3D 相机型号详情", "3D camera model details"));
    m_details->setOpenExternalLinks(true);
    m_details->setMinimumHeight(60);
    detailsLayout->addWidget(m_details, 1);
    m_productSplitter->addWidget(m_detailsPanel);
    m_productSplitter->setStretchFactor(0, 3);
    m_productSplitter->setStretchFactor(1, 1);
    m_detailsPanel->hide();
    UiSettings::instance().restoreHeader(QStringLiteral("threeD/table/v3"), m_table->horizontalHeader());
    connect(closeDetails, &QPushButton::clicked, this, [this]() { m_detailsToggle->setChecked(false); });
    connect(m_detailsToggle, &QPushButton::toggled, this, [this](bool expanded) {
        m_detailsPanel->setVisible(expanded);
        if (expanded) {
            const int height = m_productSplitter->height();
            m_productSplitter->setSizes({height * 2 / 3, height / 3});
            showDetailsForRow(m_table->currentRow());
        }
    });
    productLayout->addWidget(m_productSplitter, 1);
    tabs->addTab(productTab, localizedText("产品筛选", "Product Filters"));

    QWidget *samplingTab = new QWidget(tabs);
    QVBoxLayout *samplingLayout = new QVBoxLayout(samplingTab);
    samplingLayout->setContentsMargins(0, 0, 0, 0);
    samplingLayout->setSpacing(14);
    buildSamplingPanel(samplingLayout);
    tabs->addTab(samplingTab, localizedText("参数设定", "Parameter Setup"));
    layout->addWidget(tabs, 1);

    populateFilterOptions();
    m_summaryLabel->setText(localizedText(
        "进入页面后加载型号库", "The model library loads when opened"));
}

ThreeDCameraPage::~ThreeDCameraPage()
{
    UiSettings::instance().saveHeader(QStringLiteral("threeD/table/v3"), m_table ? m_table->horizontalHeader() : nullptr);
    UiSettings::instance().setValue(QStringLiteral("ui/threeD/advancedExpanded"),
                                    m_advancedFilters ? !m_advancedFilters->isHidden() : false);
}

void ThreeDCameraPage::activate()
{
    if (!m_resultsInitialized)
        refresh();
}

void ThreeDCameraPage::buildProductToolbar(QLayout *parentLayout)
{
    auto *toolbar = new QWidget(parentLayout->parentWidget());
    toolbar->setObjectName(QStringLiteral("ThreeDProductToolbar"));
    auto *layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(8, 6, 8, 0);
    layout->setSpacing(10);
    m_filterToggle = new QPushButton(localizedText("筛选条件", "Filters"), toolbar);
    m_filterToggle->setObjectName(QStringLiteral("ThreeDFiltersToggle"));
    m_filterToggle->setCheckable(true);
    layout->addWidget(m_filterToggle);
    m_summaryLabel = new QLabel(toolbar);
    m_summaryLabel->setObjectName(QStringLiteral("ThreeDResultSummary"));
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(m_summaryLabel, 1);

    auto *management = new QToolButton(toolbar);
    management->setObjectName(QStringLiteral("SecondaryButton"));
    management->setProperty("hasMenu", true);
    management->setText(localizedText("型号管理", "Models"));
    management->setAccessibleName(localizedText("型号管理", "Model management"));
    management->setFocusPolicy(Qt::StrongFocus);
    management->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(management);
    menu->addAction(localizedText("新增型号", "Add model"), this, &ThreeDCameraPage::addCamera);
    menu->addAction(localizedText("复制型号", "Copy model"), this, &ThreeDCameraPage::copyCamera);
    menu->addAction(localizedText("编辑型号", "Edit model"), this, &ThreeDCameraPage::editCamera);
    menu->addSeparator();
    menu->addAction(localizedText("删除自定义型号…", "Delete custom model…"), this, &ThreeDCameraPage::removeCamera);
    management->setMenu(menu);
    layout->addWidget(management);

    m_detailsToggle = new QPushButton(localizedText("型号详情", "Details"), toolbar);
    m_detailsToggle->setObjectName(QStringLiteral("ThreeDDetailsToggle"));
    m_detailsToggle->setAccessibleName(localizedText("显示型号详情", "Show model details"));
    m_detailsToggle->setToolTip(localizedText("双击型号或按 Enter 也可展开详情", "Double-click a model or press Enter to show details"));
    m_detailsToggle->setCheckable(true);
    m_detailsToggle->setEnabled(false);
    layout->addWidget(m_detailsToggle);
    parentLayout->addWidget(toolbar);
    connect(m_filterToggle, &QPushButton::toggled, this, [this](bool expanded) {
        if (m_filterScroll) m_filterScroll->setVisible(expanded);
        updateFilterButton();
    });
}

void ThreeDCameraPage::updateFilterButton()
{
    int count = 0;
    for (auto *combo : {m_brandCombo, m_technologyCombo, m_interfaceCombo, m_ipCombo, m_materialCombo})
        if (combo && combo->currentIndex() > 0) ++count;
    for (auto *spin : {m_xCoverageSpin, m_yCoverageSpin, m_zRangeSpin, m_workingDistanceSpin, m_zRepeatabilitySpin, m_speedSpin})
        if (spin && spin->value() > 0.0) ++count;
    for (auto *check : {m_noMotionCheck, m_encoderCheck})
        if (check && check->isChecked()) ++count;
    QString text = m_filterToggle->isChecked()
        ? localizedText("收起筛选", "Hide filters") : localizedText("筛选条件", "Filters");
    if (count > 0) text += QStringLiteral(" (%1)").arg(count);
    if (m_filtersDirty) text += localizedText(" · 未应用", " · Unapplied");
    m_filterToggle->setText(text);
    m_filterToggle->setAccessibleName(text);
}

void ThreeDCameraPage::buildFilters(QLayout *parentLayout)
{
    FilterScrollArea *filterScroll = new FilterScrollArea(parentLayout->parentWidget());
    m_filterScroll = filterScroll;
    QFrame *panel = new QFrame(filterScroll->viewport());
    panel->setObjectName(QStringLiteral("FilterPanel"));
    QVBoxLayout *panelLayout = new QVBoxLayout(panel);
    // 3D 筛选是高频查询区，垂直尺寸优先让出空间给结果表。
    panelLayout->setContentsMargins(18, 9, 18, 10);
    panelLayout->setSpacing(8);

    QHBoxLayout *headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(12);
    auto *filterHint = new QLabel(localizedText("编辑条件后点击“应用筛选”", "Edit conditions, then apply filters"), panel);
    filterHint->setObjectName(QStringLiteral("FilterHint"));
    filterHint->setWordWrap(true);
    headerLayout->addWidget(filterHint, 1);
    QToolButton *advancedButton = new QToolButton(panel);
    advancedButton->setObjectName(QStringLiteral("ThreeDAdvancedFilters"));
    advancedButton->setText(localizedText("高级筛选", "Advanced Filters"));
    advancedButton->setCheckable(true);
    advancedButton->setFocusPolicy(Qt::StrongFocus);
    advancedButton->setAccessibleName(advancedButton->text());
    headerLayout->addWidget(advancedButton, 0, Qt::AlignRight | Qt::AlignTop);
    panelLayout->addLayout(headerLayout);

    m_brandCombo = new QComboBox;
    m_technologyCombo = new QComboBox;
    m_interfaceCombo = new QComboBox;
    m_ipCombo = new QComboBox;
    m_materialCombo = new QComboBox;
    m_xCoverageSpin = optionalSpin(3000.0, QStringLiteral(" mm"));
    m_yCoverageSpin = optionalSpin(3000.0, QStringLiteral(" mm"));
    m_zRangeSpin = optionalSpin(3000.0, QStringLiteral(" mm"));
    m_workingDistanceSpin = optionalSpin(3000.0, QStringLiteral(" mm"));
    m_zRepeatabilitySpin = optionalSpin(200.0, QStringLiteral(" um"), 2);
    m_speedSpin = optionalSpin(100000.0, QStringLiteral(" Hz"), 0);
    m_noMotionCheck = new QCheckBox(localizedText("无需运动平台", "No motion platform"));
    m_encoderCheck = new QCheckBox(localizedText("需要编码器接口", "Encoder interface required"));
    const int filterControlHeight = UiSettings::instance().density() == UiDensity::Compact ? 30 : 34;

    const auto prepareControl = [filterControlHeight](QWidget *control) {
        control->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        control->setMinimumWidth(0);
        if (qobject_cast<QComboBox *>(control) || qobject_cast<QDoubleSpinBox *>(control))
            control->setFixedHeight(filterControlHeight);
        if (QComboBox *combo = qobject_cast<QComboBox *>(control)) {
            combo->setMinimumContentsLength(0);
            combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        }
    };
    const auto field = [prepareControl](const QString &labelText, QWidget *control) {
        QWidget *holder = new QWidget;
        holder->setObjectName(QStringLiteral("FilterField"));
        QVBoxLayout *layout = new QVBoxLayout(holder);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(3);
        QLabel *label = new QLabel(labelText);
        label->setObjectName(QStringLiteral("FilterFieldLabel"));
        label->setWordWrap(false);
        label->setToolTip(labelText);
        label->setBuddy(control);
        control->setAccessibleName(labelText);
        prepareControl(control);
        layout->addWidget(label);
        layout->addWidget(control);
        return holder;
    };

    struct FilterGroup {
        QFrame *frame;
        QGridLayout *grid;
    };
    const auto makeGroup = [panel](const QString &title) -> FilterGroup {
        QFrame *group = new QFrame(panel);
        group->setObjectName(QStringLiteral("FilterGroup"));
        group->setMinimumWidth(0);
        group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        QVBoxLayout *layout = new QVBoxLayout(group);
        layout->setContentsMargins(10, 7, 10, 8);
        layout->setSpacing(6);
        QLabel *titleLabel = new QLabel(title);
        titleLabel->setObjectName(QStringLiteral("FilterGroupTitle"));
        layout->addWidget(titleLabel);
        QGridLayout *grid = new QGridLayout;
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(10);
        grid->setVerticalSpacing(5);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);
        layout->addLayout(grid);
        return FilterGroup{group, grid};
    };

    FilterGroup attributes = makeGroup(localizedText("型号属性", "Model Attributes"));
    attributes.grid->addWidget(field(localizedText("品牌", "Brand"), m_brandCombo), 0, 0);
    attributes.grid->addWidget(field(localizedText("技术路线", "Technology"), m_technologyCombo), 0, 1);
    attributes.grid->addWidget(field(localizedText("接口", "Interface"), m_interfaceCombo), 1, 0);
    attributes.grid->addWidget(field(localizedText("材质场景", "Material Scenario"), m_materialCombo), 1, 1);

    FilterGroup geometry = makeGroup(localizedText("空间范围", "Working Range"));
    geometry.grid->addWidget(field(localizedText("目标 X 覆盖", "Target X Coverage"), m_xCoverageSpin), 0, 0);
    geometry.grid->addWidget(field(localizedText("目标 Y 覆盖", "Target Y Coverage"), m_yCoverageSpin), 0, 1);
    geometry.grid->addWidget(field(localizedText("Z 量程", "Z Range"), m_zRangeSpin), 1, 0);
    geometry.grid->addWidget(field(localizedText("工作/参考距离", "Working / Reference Distance"), m_workingDistanceSpin), 1, 1);

    QWidget *integrationBox = new QWidget;
    integrationBox->setObjectName(QStringLiteral("FilterCheckGroup"));
    QVBoxLayout *integrationLayout = new QVBoxLayout(integrationBox);
    integrationLayout->setContentsMargins(0, 0, 0, 0);
    integrationLayout->setSpacing(6);
    integrationLayout->addWidget(m_noMotionCheck);
    integrationLayout->addWidget(m_encoderCheck);
    integrationLayout->addStretch();

    FilterGroup performance = makeGroup(localizedText("性能与集成", "Performance and Integration"));
    performance.grid->addWidget(field(localizedText("最大 Z 重复精度", "Max Z Repeatability"), m_zRepeatabilitySpin), 0, 0);
    performance.grid->addWidget(field(localizedText("最小速度", "Minimum Speed"), m_speedSpin), 0, 1);
    performance.grid->addWidget(field(localizedText("防护等级", "IP Rating"), m_ipCombo), 1, 0);
    performance.grid->addWidget(field(localizedText("集成要求", "Integration"), integrationBox), 1, 1);

    QHBoxLayout *groupsLayout = new QHBoxLayout;
    groupsLayout->setContentsMargins(0, 0, 0, 0);
    groupsLayout->setSpacing(10);
    groupsLayout->addWidget(attributes.frame, 1);
    groupsLayout->addWidget(geometry.frame, 1);
    panelLayout->addLayout(groupsLayout);
    m_advancedFilters = performance.frame;
    const bool advancedExpanded = UiSettings::instance().boolValue(QStringLiteral("ui/threeD/advancedExpanded"), false);
    advancedButton->setChecked(advancedExpanded);
    m_advancedFilters->setVisible(advancedExpanded);
    panelLayout->addWidget(m_advancedFilters);
    connect(advancedButton, &QToolButton::toggled, this, [this](bool expanded) {
        if (m_advancedFilters)
            m_advancedFilters->setVisible(expanded);
        UiSettings::instance().setValue(QStringLiteral("ui/threeD/advancedExpanded"), expanded);
    });

    QPushButton *applyButton = actionButton(localizedText("应用筛选", "Apply Filters"), QStringLiteral(":/icons/ui/calculate.png"));
    applyButton->setObjectName(QStringLiteral("ThreeDApplyFilters"));
    QPushButton *clearButton = actionButton(localizedText("清空条件", "Clear"), QStringLiteral(":/icons/ui/info.png"), true);
    clearButton->setObjectName(QStringLiteral("ThreeDClearFilters"));

    QHBoxLayout *actionLayout = new QHBoxLayout;
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(8);
    QLabel *hintLabel = new QLabel(localizedText("数值为“不限”时不会参与过滤。", "Numeric fields set to Any are ignored."));
    hintLabel->setObjectName(QStringLiteral("FilterHint"));
    actionLayout->addWidget(hintLabel, 1);
    actionLayout->addWidget(clearButton);
    actionLayout->addWidget(applyButton);
    clearButton->setFixedHeight(filterControlHeight);
    applyButton->setFixedHeight(filterControlHeight);
    panelLayout->addLayout(actionLayout);

    connect(applyButton, &QPushButton::clicked, this, [this]() {
        refresh();
        if (!m_filtersDirty) m_filterToggle->setChecked(false);
    });
    connect(clearButton, &QPushButton::clicked, this, &ThreeDCameraPage::clearFilters);
    const auto dirty = [this]() { m_filtersDirty = true; updateFilterButton(); };
    for (auto *combo : {m_brandCombo, m_technologyCombo, m_interfaceCombo, m_ipCombo, m_materialCombo})
        connect(combo, &QComboBox::currentIndexChanged, this, dirty);
    for (auto *spin : {m_xCoverageSpin, m_yCoverageSpin, m_zRangeSpin, m_workingDistanceSpin, m_zRepeatabilitySpin, m_speedSpin})
        connect(spin, &QDoubleSpinBox::valueChanged, this, dirty);
    for (auto *check : {m_noMotionCheck, m_encoderCheck})
        connect(check, &QCheckBox::toggled, this, dirty);
    // 小窗口和高级筛选通过局部滚动保持可达，不再撑大整个工作台。
    filterScroll->setObjectName(QStringLiteral("ThreeDFilterScroll"));
    filterScroll->setWidgetResizable(true);
    filterScroll->setFrameShape(QFrame::NoFrame);
    filterScroll->setMaximumHeight(270);
    filterScroll->setWidget(panel);
    parentLayout->addWidget(filterScroll);
    filterScroll->hide();
}

void ThreeDCameraPage::buildSamplingPanel(QLayout *parentLayout)
{
    QWidget *bodyWidget = new QWidget(parentLayout->parentWidget());
    bodyWidget->setObjectName(QStringLiteral("ParameterInputPanel"));
    QHBoxLayout *body = new QHBoxLayout(bodyWidget);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(14);

    QScrollArea *inputScroll = new QScrollArea(bodyWidget);
    inputScroll->setObjectName(QStringLiteral("ParameterScroll"));
    inputScroll->setWidgetResizable(true);
    inputScroll->setFrameShape(QFrame::NoFrame);
    inputScroll->setMinimumWidth(320);
    inputScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QWidget *inputPanel = new QWidget(inputScroll->viewport());
    inputPanel->setObjectName(QStringLiteral("ParameterInputPanel"));
    QVBoxLayout *inputLayout = new QVBoxLayout(inputPanel);
    inputLayout->setContentsMargins(0, 0, 8, 0);
    inputLayout->setSpacing(12);

    const auto prepareControl = [](QWidget *control) {
        control->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        control->setMinimumWidth(0);
        if (QComboBox *combo = qobject_cast<QComboBox *>(control)) {
            combo->setMinimumContentsLength(0);
            combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        }
    };
    const auto field = [prepareControl](const QString &labelText, QWidget *control) {
        QWidget *holder = new QWidget;
        holder->setObjectName(QStringLiteral("ParameterField"));
        QVBoxLayout *layout = new QVBoxLayout(holder);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(5);
        QLabel *label = new QLabel(labelText);
        label->setObjectName(QStringLiteral("ParameterFieldLabel"));
        label->setBuddy(control);
        control->setAccessibleName(labelText);
        label->setToolTip(labelText);
        prepareControl(control);
        layout->addWidget(label);
        layout->addWidget(control);
        return holder;
    };

    struct ParameterGroup {
        QFrame *frame;
        QGridLayout *grid;
        QVBoxLayout *layout;
    };
    const auto makeGroup = [inputPanel](const QString &title, const QString &subtitle) -> ParameterGroup {
        QFrame *group = new QFrame(inputPanel);
        group->setObjectName(QStringLiteral("ParameterGroup"));
        QVBoxLayout *layout = new QVBoxLayout(group);
        layout->setContentsMargins(14, 12, 14, 14);
        layout->setSpacing(9);
        QLabel *titleLabel = new QLabel(title);
        titleLabel->setObjectName(QStringLiteral("ParameterGroupTitle"));
        layout->addWidget(titleLabel);
        if (!subtitle.isEmpty()) {
            QLabel *subtitleLabel = new QLabel(subtitle);
            subtitleLabel->setObjectName(QStringLiteral("ParameterGroupSubtitle"));
            subtitleLabel->setWordWrap(true);
            layout->addWidget(subtitleLabel);
        }
        QGridLayout *grid = new QGridLayout;
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(10);
        grid->setVerticalSpacing(9);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);
        layout->addLayout(grid);
        return ParameterGroup{group, grid, layout};
    };

    ParameterGroup cameraGroup = makeGroup(
        localizedText("当前 3D 相机", "Current 3D Camera"),
        localizedText("从产品筛选页选中型号后，系统会自动读取公开的 X 轮廓数据间隔和速度上限。",
                      "After selecting a model in Product Filters, published X interval and speed limit are applied automatically."));
    m_samplingCameraLabel = new QLabel(localizedText("尚未选择 3D 相机。", "No 3D camera selected."));
    m_samplingCameraLabel->setObjectName(QStringLiteral("CalculationResultSubtitle"));
    m_samplingCameraLabel->setWordWrap(true);
    cameraGroup.layout->addWidget(m_samplingCameraLabel);

    ParameterGroup scanGroup = makeGroup(
        localizedText("扫描与轮廓", "Scan and Profile"),
        localizedText("对应 Excel 中固定工作距离内的采集帧数计算。",
                      "Matches the spreadsheet's profile count calculation for a fixed scan distance."));
    m_scanDistanceSpin = makeSpin(0.001, 100000.0, 300.0, QStringLiteral(" mm"), 3);
    m_profileIntervalSpin = makeSpin(0.0001, 1000.0, 0.05, localizedText(" mm/轮廓", " mm/profile"), 4);
    m_samplingRateSpin = makeSpin(0.1, 1000000.0, 1000.0, QStringLiteral(" Hz"), 1);
    m_targetAxisSpeedSpin = makeSpin(0.0, 1000000.0, 0.0, QStringLiteral(" mm/s"), 3);
    m_targetAxisSpeedSpin->setSpecialValueText(localizedText("不校验", "No check"));
    m_safetyFactorSpin = makeSpin(0.01, 1.0, 0.8, QString(), 2);
    scanGroup.grid->addWidget(field(localizedText("扫描距离", "Scan distance"), m_scanDistanceSpin), 0, 0);
    scanGroup.grid->addWidget(field(localizedText("轮廓采样间隔", "Profile interval"), m_profileIntervalSpin), 0, 1);
    scanGroup.grid->addWidget(field(localizedText("采样频率", "Sampling rate"), m_samplingRateSpin), 1, 0);
    scanGroup.grid->addWidget(field(localizedText("安全系数", "Safety factor"), m_safetyFactorSpin), 1, 1);
    scanGroup.grid->addWidget(field(localizedText("目标轴速度", "Target axis speed"), m_targetAxisSpeedSpin), 2, 0);

    ParameterGroup triggerGroup = makeGroup(
        localizedText("触发与曝光", "Trigger and Exposure"),
        localizedText("校验自由运行、外部线触发或编码器触发下的有效轮廓频率和曝光周期余量。",
                      "Validate effective profile rate and exposure cycle margin for free-running, external trigger, or encoder trigger."));
    m_triggerModeCombo = new QComboBox;
    m_triggerModeCombo->addItem(triggerModeText(ThreeDTriggerMode::FreeRun), static_cast<int>(ThreeDTriggerMode::FreeRun));
    m_triggerModeCombo->addItem(triggerModeText(ThreeDTriggerMode::ExternalTrigger), static_cast<int>(ThreeDTriggerMode::ExternalTrigger));
    m_triggerModeCombo->addItem(triggerModeText(ThreeDTriggerMode::Encoder), static_cast<int>(ThreeDTriggerMode::Encoder));
    m_exposureTimeSpin = makeSpin(0.0, 10000000.0, 100.0, QStringLiteral(" us"), 2);
    m_readoutMarginSpin = makeSpin(0.0, 1000000.0, 3.0, QStringLiteral(" us"), 2);
    m_encoderFrequencySpin = makeSpin(0.0, 10000000.0, 0.0, QStringLiteral(" Hz"), 1);
    m_encoderFrequencySpin->setSpecialValueText(localizedText("未输入", "Not set"));
    triggerGroup.grid->addWidget(field(localizedText("触发方式", "Trigger mode"), m_triggerModeCombo), 0, 0);
    triggerGroup.grid->addWidget(field(localizedText("曝光时间", "Exposure time"), m_exposureTimeSpin), 0, 1);
    triggerGroup.grid->addWidget(field(localizedText("读出/复位余量", "Readout/reset margin"), m_readoutMarginSpin), 1, 0);
    triggerGroup.grid->addWidget(field(localizedText("编码器脉冲频率", "Encoder pulse frequency"), m_encoderFrequencySpin), 1, 1);

    ParameterGroup encoderGroup = makeGroup(
        localizedText("编码器与像素当量", "Encoder and Pixel Pitch"),
        localizedText("对应 Excel 中脉冲采样间隔、Y 像素当量和 X 像素当量计算。",
                      "Matches pulse interval, Y pitch, and X pitch calculations in the spreadsheet."));
    m_axisTravelSpin = makeSpin(0.001, 100000.0, 10.0, QStringLiteral(" mm"), 3);
    m_pulseCountSpin = dialogIntSpin(1, 1000000000, 10000);
    m_refinementPointsSpin = dialogIntSpin(1, 1000000, 50);
    m_encoderPulsesPerProfileSpin = dialogIntSpin(1, 1000000, 50);
    m_xPitchOverrideSpin = makeSpin(0.0, 1000.0, 0.0, QStringLiteral(" mm"), 4);
    m_xPitchOverrideSpin->setSpecialValueText(localizedText("自动", "Auto"));
    encoderGroup.grid->addWidget(field(localizedText("轴移动距离", "Axis travel"), m_axisTravelSpin), 0, 0);
    encoderGroup.grid->addWidget(field(localizedText("脉冲数量", "Pulse count"), m_pulseCountSpin), 0, 1);
    encoderGroup.grid->addWidget(field(localizedText("细化点数", "Refinement points"), m_refinementPointsSpin), 1, 0);
    encoderGroup.grid->addWidget(field(localizedText("每轮廓脉冲数", "Pulses per profile"), m_encoderPulsesPerProfileSpin), 1, 1);
    encoderGroup.grid->addWidget(field(localizedText("手动 X 像素当量", "Manual X pitch"), m_xPitchOverrideSpin), 2, 0);

    inputLayout->addWidget(cameraGroup.frame);
    inputLayout->addWidget(scanGroup.frame);
    inputLayout->addWidget(triggerGroup.frame);
    inputLayout->addWidget(encoderGroup.frame);
    inputLayout->addStretch();
    inputScroll->setWidget(inputPanel);

    QFrame *outputPanel = new QFrame(bodyWidget);
    outputPanel->setObjectName(QStringLiteral("CalculationResultPanel"));
    QVBoxLayout *outputLayout = new QVBoxLayout(outputPanel);
    outputLayout->setContentsMargins(16, 14, 16, 16);
    outputLayout->setSpacing(12);
    QHBoxLayout *actions = new QHBoxLayout;
    actions->setContentsMargins(0, 0, 0, 0);
    QWidget *resultHeader = new QWidget(outputPanel);
    resultHeader->setObjectName(QStringLiteral("CalculationResultHeader"));
    QVBoxLayout *resultHeaderLayout = new QVBoxLayout(resultHeader);
    resultHeaderLayout->setContentsMargins(0, 0, 0, 0);
    resultHeaderLayout->setSpacing(3);
    QLabel *resultTitle = new QLabel(localizedText("3D 参数设定结果", "3D Parameter Result"));
    resultTitle->setObjectName(QStringLiteral("CalculationResultTitle"));
    QLabel *resultSubtitle = new QLabel(localizedText(
        "按当前相机和现场参数计算轮廓数、脉冲间隔、X/Y 像素当量和允许轴速度。",
        "Calculate profile count, pulse interval, X/Y pitch, and allowed axis speed from the selected camera and site parameters."));
    resultSubtitle->setObjectName(QStringLiteral("CalculationResultSubtitle"));
    resultSubtitle->setWordWrap(true);
    resultHeaderLayout->addWidget(resultTitle);
    resultHeaderLayout->addWidget(resultSubtitle);
    actions->addWidget(resultHeader, 1);
    QPushButton *resetButton = actionButton(localizedText("恢复示例", "Reset Example"), QStringLiteral(":/icons/ui/info.png"), true);
    QPushButton *calculateButton = actionButton(localizedText("计算", "Calculate"), QStringLiteral(":/icons/ui/calculate.png"));
    actions->addWidget(resetButton);
    actions->addWidget(calculateButton);
    outputLayout->addLayout(actions);

    m_samplingOutput = new QTextEdit;
    m_samplingOutput->setObjectName(QStringLiteral("CalculationResultText"));
    m_samplingOutput->setReadOnly(true);
    outputLayout->addWidget(m_samplingOutput, 1);

    body->addWidget(inputScroll);
    body->addWidget(outputPanel, 1);
    parentLayout->addWidget(bodyWidget);

    connect(calculateButton, &QPushButton::clicked, this, &ThreeDCameraPage::refreshSampling);
    connect(resetButton, &QPushButton::clicked, this, &ThreeDCameraPage::resetSamplingDefaults);

    const QList<QDoubleSpinBox *> doubleSpins = {
        m_scanDistanceSpin, m_profileIntervalSpin, m_targetAxisSpeedSpin, m_axisTravelSpin,
        m_samplingRateSpin, m_safetyFactorSpin, m_xPitchOverrideSpin,
        m_exposureTimeSpin, m_readoutMarginSpin, m_encoderFrequencySpin
    };
    for (QDoubleSpinBox *spin : doubleSpins)
        connect(spin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this, [this](double) { refreshSampling(); });
    const QList<QSpinBox *> intSpins = {m_pulseCountSpin, m_refinementPointsSpin, m_encoderPulsesPerProfileSpin};
    for (QSpinBox *spin : intSpins)
        connect(spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, [this](int) { refreshSampling(); });
    connect(m_triggerModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int) { refreshSampling(); });
    refreshSampling();
}

void ThreeDCameraPage::populateFilterOptions()
{
    addComboValues(m_brandCombo, m_repository.manufacturers());
    QStringList technologies = threeDTechnologyLabels();
    addComboValues(m_technologyCombo, technologies);
    addComboValues(m_interfaceCombo, m_repository.interfaces());
    addComboValues(m_ipCombo, m_repository.ipRatings());
    addComboValues(m_materialCombo, m_repository.materialScenarios());
}

bool ThreeDCameraPage::ensureLoaded()
{
    if (m_loaded)
        return true;

    QString error;
    if (!m_repository.loadFromResource(QStringLiteral(":/data/three_d_cameras.json"), &error)) {
        m_summaryLabel->setText(error);
        return false;
    }

    m_loaded = true;
    populateFilterOptions();
    return true;
}

ThreeDCameraRequirement ThreeDCameraPage::requirement() const
{
    ThreeDCameraRequirement req;
    req.manufacturer = comboRequirement(m_brandCombo);
    req.technologyLabel = comboRequirement(m_technologyCombo);
    req.interfaceText = comboRequirement(m_interfaceCombo);
    req.ipRating = comboRequirement(m_ipCombo);
    req.materialScenario = comboRequirement(m_materialCombo);
    req.targetXCoverageMm = optionalValue(m_xCoverageSpin);
    req.targetYCoverageMm = optionalValue(m_yCoverageSpin);
    req.zMeasurementRangeMm = optionalValue(m_zRangeSpin);
    req.workingDistanceMm = optionalValue(m_workingDistanceSpin);
    req.maxZRepeatabilityUm = optionalValue(m_zRepeatabilitySpin);
    req.minSpeedHz = optionalValue(m_speedSpin);
    req.requireNoExternalMotion = m_noMotionCheck && m_noMotionCheck->isChecked();
    req.requireEncoder = m_encoderCheck && m_encoderCheck->isChecked();
    return req;
}

ThreeDMotionSamplingInput ThreeDCameraPage::samplingInput() const
{
    ThreeDMotionSamplingInput input;
    if (!m_scanDistanceSpin)
        return input;
    input.scanDistanceMm = m_scanDistanceSpin->value();
    input.profileIntervalMm = m_profileIntervalSpin->value();
    input.targetAxisSpeedMmS = m_targetAxisSpeedSpin && m_targetAxisSpeedSpin->value() > 0.0
        ? m_targetAxisSpeedSpin->value()
        : -1.0;
    input.axisTravelMm = m_axisTravelSpin->value();
    input.pulseCount = m_pulseCountSpin->value();
    input.refinementPoints = m_refinementPointsSpin->value();
    input.encoderPulsesPerProfile = m_encoderPulsesPerProfileSpin ? m_encoderPulsesPerProfileSpin->value() : 1;
    input.samplingRateHz = m_samplingRateSpin->value();
    input.safetyFactor = m_safetyFactorSpin->value();
    input.overrideXPixelPitchMm = m_xPitchOverrideSpin->value() > 0.0
        ? m_xPitchOverrideSpin->value()
        : -1.0;
    input.triggerMode = m_triggerModeCombo
        ? static_cast<ThreeDTriggerMode>(m_triggerModeCombo->currentData().toInt())
        : ThreeDTriggerMode::FreeRun;
    input.exposureTimeUs = m_exposureTimeSpin ? m_exposureTimeSpin->value() : 100.0;
    input.readoutMarginUs = m_readoutMarginSpin ? m_readoutMarginSpin->value() : 3.0;
    input.encoderPulseFrequencyHz = m_encoderFrequencySpin && m_encoderFrequencySpin->value() > 0.0
        ? m_encoderFrequencySpin->value()
        : -1.0;
    return input;
}

const ThreeDCameraSpec *ThreeDCameraPage::selectedCameraSpec() const
{
    if (m_selectedMatchIndex < 0 || m_selectedMatchIndex >= m_matches.size())
        return nullptr;
    return &m_matches.at(m_selectedMatchIndex).spec;
}

void ThreeDCameraPage::refresh()
{
    if (!ensureLoaded())
        return;

    ThreeDCameraMatcher matcher;
    m_matches = matcher.match(requirement(), m_repository.cameras());
    m_resultsInitialized = true;
    m_filtersDirty = false;
    updateFilterButton();
    fillTable();
}

void ThreeDCameraPage::refreshSampling()
{
    if (!m_samplingOutput)
        return;

    const ThreeDCameraSpec *camera = selectedCameraSpec();
    const ThreeDMotionSamplingInput input = samplingInput();
    const ThreeDMotionSamplingResult result = ThreeDCalculation::estimateMotionSampling(input, camera);

    if (m_samplingCameraLabel) {
        if (camera) {
            QStringList parts;
            parts.append(localizedText("型号：%1 %2", "Model: %1 %2").arg(camera->manufacturer, camera->model));
            parts.append(localizedText("技术路线：%1", "Technology: %1").arg(threeDTechnologyLabel(camera->technology)));
            parts.append(localizedText("X 间隔：%1", "X interval: %1").arg(valueOrUnknown(camera->profileDataIntervalUm, QStringLiteral(" um"), 2)));
            parts.append(localizedText("速度上限：%1", "Speed limit: %1").arg(valueOrUnknown(result.cameraSamplingRateLimitHz, QStringLiteral(" Hz"), 0)));
            parts.append(localizedText("编码器上限：%1", "Encoder limit: %1").arg(valueOrUnknown(camera->encoderRateMaxHz, QStringLiteral(" Hz"), 0)));
            m_samplingCameraLabel->setText(parts.join(localizedText("；", "; ")));
        } else {
            m_samplingCameraLabel->setText(localizedText(
                "尚未选择 3D 相机。可以先用手动 X 像素当量计算，或回到产品筛选页选择型号。",
                "No 3D camera selected. You can calculate with a manual X pitch or select a model in Product Filters."));
        }
    }

    const QString rateLimitText = result.samplingRateKnown
        ? QStringLiteral("%1 Hz").arg(result.cameraSamplingRateLimitHz, 0, 'f', 0)
        : localizedText("未公开", "Unpublished");
    const QString xPitchText = result.xPixelPitchKnown
        ? QStringLiteral("%1 mm").arg(result.xPixelPitchMm, 0, 'f', 4)
        : localizedText("需确认", "Needs confirmation");
    const QString ratioText = threeDHasValue(result.xyPitchRatio)
        ? QStringLiteral("%1:1").arg(result.xyPitchRatio, 0, 'f', 2)
        : localizedText("需确认", "Needs confirmation");
    const QString requiredRateText = threeDHasValue(result.requiredProfileRateHz)
        ? QStringLiteral("%1 Hz").arg(result.requiredProfileRateHz, 0, 'f', 1)
        : localizedText("不校验", "No check");
    const QString effectiveRateText = threeDHasValue(result.effectiveProfileRateHz)
        ? QStringLiteral("%1 Hz").arg(result.effectiveProfileRateHz, 0, 'f', 1)
        : localizedText("需确认", "Needs confirmation");
    const QString periodText = threeDHasValue(result.profilePeriodUs)
        ? QStringLiteral("%1 us").arg(result.profilePeriodUs, 0, 'f', 2)
        : localizedText("需确认", "Needs confirmation");
    const QString exposureLimitText = threeDHasValue(result.maxExposureTimeUs)
        ? QStringLiteral("%1 us").arg(result.maxExposureTimeUs, 0, 'f', 2)
        : localizedText("需确认", "Needs confirmation");
    const QString encoderIntervalText = threeDHasValue(result.encoderProfileIntervalMm)
        ? QStringLiteral("%1 mm/轮廓").arg(result.encoderProfileIntervalMm, 0, 'f', 6)
        : localizedText("未计算", "Not calculated");
    const QString encoderAxisSpeedText = threeDHasValue(result.encoderAxisSpeedMmS)
        ? QStringLiteral("%1 mm/s").arg(result.encoderAxisSpeedMmS, 0, 'f', 2)
        : localizedText("未计算", "Not calculated");
    const QString stateText = result.status == ThreeDCalculationStatus::Valid
        ? localizedText("参数正常", "Parameters OK")
        : result.status == ThreeDCalculationStatus::Warning
            ? localizedText("需要确认", "Needs confirmation")
            : result.status == ThreeDCalculationStatus::Infeasible
                ? localizedText("参数不可行", "Parameters infeasible")
                : localizedText("输入无效", "Invalid input");

    QString html;
    html += QStringLiteral("<h3>%1</h3>").arg(htmlEscape(localizedText("3D 参数设定结果", "3D Parameter Setup Result")));
    html += QStringLiteral("<p><b>%1</b>: %2</p>")
        .arg(htmlEscape(localizedText("状态", "Status")), htmlEscape(stateText));
    html += QStringLiteral("<table cellspacing=\"0\" cellpadding=\"5\">");
    const auto row = [](const QString &label, const QString &value, const QString &formula) {
        return QStringLiteral("<tr><td><b>%1</b></td><td>%2</td><td style=\"color:#667085;\">%3</td></tr>")
            .arg(htmlEscape(label), htmlEscape(value), htmlEscape(formula));
    };
    html += row(localizedText("采集轮廓数", "Profile count"),
                QString::number(result.profileCount),
                localizedText("向上取整(扫描距离 / 间隔)，起点采集、终点不重复", "ceil(distance / interval); include start, exclude end"));
    html += row(localizedText("脉冲采样间隔", "Pulse interval"),
                QStringLiteral("%1 mm/pulse").arg(result.pulseIntervalMm, 0, 'f', 6),
                localizedText("轴移动距离 / 脉冲数量", "axis travel / pulse count"));
    html += row(localizedText("Y 像素当量", "Y pixel pitch"),
                QStringLiteral("%1 mm").arg(result.yPixelPitchMm, 0, 'f', 4),
                localizedText("脉冲采样间隔 × 细化点数", "pulse interval x refinement points"));
    html += row(localizedText("X 像素当量", "X pixel pitch"),
                xPitchText,
                result.usesManualXPixelPitch
                    ? localizedText("手动输入", "manual input")
                    : localizedText("相机 X 轮廓数据间隔 / 1000", "camera X interval / 1000"));
    html += row(localizedText("允许轴速度", "Allowed axis speed"),
                QStringLiteral("%1 mm/s").arg(result.maxAxisSpeedMmS, 0, 'f', 2),
                localizedText("采样频率 × 轮廓采样间隔 × 安全系数", "sampling rate x profile interval x safety factor"));
    html += row(localizedText("X/Y 点距比例", "X/Y pitch ratio"),
                ratioText,
                localizedText("Y 像素当量 / X 像素当量", "Y pitch / X pitch"));
    html += row(localizedText("相机速度上限", "Camera speed limit"),
                rateLimitText,
                localizedText("最大扫描频率或帧率", "max scan rate or frame rate"));
    html += row(localizedText("触发方式", "Trigger mode"),
                triggerModeText(input.triggerMode),
                localizedText("自由运行 / 外部线触发 / 编码器触发", "free running / external trigger / encoder trigger"));
    html += row(localizedText("有效轮廓频率", "Effective profile rate"),
                effectiveRateText,
                input.triggerMode == ThreeDTriggerMode::Encoder
                    ? localizedText("编码器脉冲频率 / 每轮廓脉冲数", "encoder pulse frequency / pulses per profile")
                    : localizedText("采样频率", "sampling rate"));
    html += row(localizedText("目标所需轮廓频率", "Required profile rate"),
                requiredRateText,
                localizedText("目标轴速度 / (轮廓间隔 × 安全系数)", "target speed / (profile interval x safety factor)"));
    html += row(localizedText("轮廓周期", "Profile period"),
                periodText,
                localizedText("1 / 有效轮廓频率", "1 / effective profile rate"));
    html += row(localizedText("最大曝光时间", "Max exposure time"),
                exposureLimitText,
                localizedText("轮廓周期 - 读出/复位余量", "profile period - readout/reset margin"));
    html += row(localizedText("编码器每轮廓距离", "Encoder distance per profile"),
                encoderIntervalText,
                localizedText("脉冲采样间隔 × 每轮廓脉冲数", "pulse interval x pulses per profile"));
    html += row(localizedText("编码器推算轴速", "Encoder-derived axis speed"),
                encoderAxisSpeedText,
                localizedText("编码器脉冲频率 × 脉冲采样间隔", "encoder pulse frequency x pulse interval"));
    html += QStringLiteral("</table>");

    html += QStringLiteral("<h4>%1</h4>%2")
        .arg(htmlEscape(localizedText("依据", "Reasons")),
             htmlList(result.reasons, localizedText("已按当前参数完成计算。", "Calculated from current parameters.")));
    html += QStringLiteral("<h4>%1</h4>%2")
        .arg(htmlEscape(localizedText("风险与确认项", "Risks and Confirmation Items")),
             htmlList(result.risks, localizedText("无主要风险。", "No major risk.")));

    m_samplingOutput->setHtml(html);
}

void ThreeDCameraPage::resetSamplingDefaults()
{
    if (!m_scanDistanceSpin)
        return;
    m_scanDistanceSpin->setValue(300.0);
    m_profileIntervalSpin->setValue(0.05);
    m_targetAxisSpeedSpin->setValue(0.0);
    m_axisTravelSpin->setValue(10.0);
    m_pulseCountSpin->setValue(10000);
    m_refinementPointsSpin->setValue(50);
    m_encoderPulsesPerProfileSpin->setValue(50);
    m_samplingRateSpin->setValue(1000.0);
    m_safetyFactorSpin->setValue(0.8);
    m_xPitchOverrideSpin->setValue(0.0);
    m_triggerModeCombo->setCurrentIndex(0);
    m_exposureTimeSpin->setValue(100.0);
    m_readoutMarginSpin->setValue(3.0);
    m_encoderFrequencySpin->setValue(0.0);
    refreshSampling();
}

void ThreeDCameraPage::clearFilters()
{
    const QList<QComboBox *> combos = {m_brandCombo, m_technologyCombo, m_interfaceCombo, m_ipCombo, m_materialCombo};
    for (QComboBox *combo : combos) {
        if (combo)
            combo->setCurrentIndex(0);
    }
    const QList<QDoubleSpinBox *> spins = {m_xCoverageSpin, m_yCoverageSpin, m_zRangeSpin, m_workingDistanceSpin, m_zRepeatabilitySpin, m_speedSpin};
    for (QDoubleSpinBox *spin : spins) {
        if (spin)
            spin->setValue(0.0);
    }
    if (m_noMotionCheck)
        m_noMotionCheck->setChecked(false);
    if (m_encoderCheck)
        m_encoderCheck->setChecked(false);
    refresh();
}

void ThreeDCameraPage::fillTable()
{
    if (!m_table)
        return;
    const int previousRow = m_table->currentRow();
    const QString previousModel = m_table->item(previousRow, 4) ? m_table->item(previousRow, 4)->text() : QString();
    const QString previousBrand = m_table->item(previousRow, 2) ? m_table->item(previousRow, 2)->text() : QString();
    int selectedRow = 0;
    for (int row = 0; row < m_matches.size(); ++row) {
        if (m_matches.at(row).spec.model == previousModel && m_matches.at(row).spec.manufacturer == previousBrand) {
            selectedRow = row;
            break;
        }
    }

    int matched = 0;
    int missing = 0;
    int rejected = 0;
    for (const ThreeDCameraMatch &match : m_matches) {
        if (match.status == ThreeDMatchStatus::Match)
            ++matched;
        else if (match.status == ThreeDMatchStatus::MissingData)
            ++missing;
        else
            ++rejected;
    }
    m_summaryLabel->setText(localizedText(
        "%1 个型号 · 满足 %2 · 待确认 %3 · 不满足 %4",
        "%1 models · %2 matched · %3 pending · %4 unmatched")
        .arg(m_matches.size()).arg(matched).arg(missing).arg(rejected));
    m_summaryLabel->setToolTip(m_summaryLabel->text());

    {
        QSignalBlocker blocker(m_table);
        m_table->setUpdatesEnabled(false);
        m_table->setSortingEnabled(false);
        m_table->clearContents();
        m_table->setRowCount(0);
        m_table->setRowCount(m_matches.size());
        for (int row = 0; row < m_matches.size(); ++row) {
            const ThreeDCameraMatch &match = m_matches.at(row);
            const ThreeDCameraSpec &spec = match.spec;
            QTableWidgetItem *statusItem = indexedItem(threeDMatchStatusLabel(match.status), row);
            setStatusColor(statusItem, match.status);
            m_table->setItem(row, 0, statusItem);
            m_table->setItem(row, 1, item(sourceTypeText(spec)));
            m_table->setItem(row, 2, item(spec.manufacturer));
            m_table->setItem(row, 3, item(spec.series));
            m_table->setItem(row, 4, item(spec.model));
            m_table->setItem(row, 5, item(threeDTechnologyLabel(spec.technology)));
            m_table->setItem(row, 6, item(geometrySummary(spec)));
            m_table->setItem(row, 7, item(qualitySummary(spec)));
            m_table->setItem(row, 8, item(speedSummary(spec)));
            m_table->setItem(row, 9, item(localizedSpecList(spec.interfaces, QStringLiteral(", "))));
        }
        m_table->setUpdatesEnabled(true);
    }
    // 应用条件后保持从首列开始，避免上一次的横向位置让“匹配状态”消失。
    if (m_table->horizontalScrollBar())
        m_table->horizontalScrollBar()->setValue(0);
    if (!m_matches.isEmpty()) {
        {
            const QSignalBlocker blocker(m_table);
            m_table->selectRow(selectedRow);
        }
        showDetailsForRow(selectedRow);
    } else if (m_details) {
        m_selectedMatchIndex = -1;
        m_detailsToggle->setChecked(false);
        m_detailsToggle->setEnabled(false);
        m_details->clear();
        refreshSampling();
    }
}

void ThreeDCameraPage::updateSamplingFromSelected(int row)
{
    if (!m_table || row < 0 || row >= m_table->rowCount()) {
        m_selectedMatchIndex = -1;
        refreshSampling();
        return;
    }
    const int sourceIndex = rowSourceIndex(m_table, row);
    m_selectedMatchIndex = sourceIndex >= 0 ? sourceIndex : row;
    refreshSampling();
}

int ThreeDCameraPage::selectedCameraRepositoryIndex() const
{
    int matchIndex = -1;
    if (m_table && m_table->currentRow() >= 0)
        matchIndex = rowSourceIndex(m_table, m_table->currentRow());
    if (matchIndex < 0)
        matchIndex = m_selectedMatchIndex;
    if (matchIndex < 0 || matchIndex >= m_matches.size())
        return -1;

    const ThreeDCameraSpec &selected = m_matches.at(matchIndex).spec;
    const QVector<ThreeDCameraSpec> &cameras = m_repository.cameras();
    for (int i = 0; i < cameras.size(); ++i) {
        if (sameCameraIdentity(cameras.at(i), selected))
            return i;
    }
    return -1;
}

void ThreeDCameraPage::addCamera()
{
    if (!ensureLoaded())
        return;
    ThreeDCameraSpec camera;
    camera.manufacturer = localizedText("自定义品牌", "Custom Brand");
    camera.series = localizedText("自定义系列", "Custom Series");
    camera.model = localizedText("自定义型号", "Custom Model");
    camera.technology = ThreeDTechnology::LineLaserProfile;
    camera.technologyLabel = threeDTechnologyLabel(camera.technology);
    camera.status = localizedText("用户录入", "User entered");
    camera.sourceDate = QDate::currentDate().toString(Qt::ISODate);
    camera.supportsEncoder = 1;
    camera.supportsExternalTrigger = 1;
    camera.userDefined = true;
    if (!editThreeDCameraDialog(this, &camera, localizedText("新增 3D 相机", "Add 3D Camera")))
        return;

    QString error;
    if (!m_repository.addCamera(camera, &error)) {
        QMessageBox::warning(this, localizedText("保存失败", "Save Failed"), error);
        return;
    }
    populateFilterOptions();
    refresh();
}

void ThreeDCameraPage::copyCamera()
{
    if (!ensureLoaded())
        return;
    const int index = selectedCameraRepositoryIndex();
    if (index < 0 || index >= m_repository.cameras().size()) {
        QMessageBox::information(this, localizedText("未选择型号", "No Model Selected"),
            localizedText("请先在表格中选择一个 3D 相机型号。", "Select a 3D camera model in the table first."));
        return;
    }

    ThreeDCameraSpec camera = m_repository.cameras().at(index);
    camera.model += localizedText(" 副本", " Copy");
    camera.status = localizedText("用户录入", "User entered");
    camera.sourceDate = QDate::currentDate().toString(Qt::ISODate);
    camera.userDefined = true;
    if (!editThreeDCameraDialog(this, &camera, localizedText("复制 3D 相机", "Copy 3D Camera")))
        return;

    QString error;
    if (!m_repository.addCamera(camera, &error)) {
        QMessageBox::warning(this, localizedText("保存失败", "Save Failed"), error);
        return;
    }
    populateFilterOptions();
    refresh();
}

void ThreeDCameraPage::editCamera()
{
    if (!ensureLoaded())
        return;
    const int index = selectedCameraRepositoryIndex();
    if (index < 0 || index >= m_repository.cameras().size()) {
        QMessageBox::information(this, localizedText("未选择型号", "No Model Selected"),
            localizedText("请先在表格中选择一个 3D 相机型号。", "Select a 3D camera model in the table first."));
        return;
    }

    ThreeDCameraSpec camera = m_repository.cameras().at(index);
    if (!editThreeDCameraDialog(this, &camera, localizedText("编辑 3D 相机", "Edit 3D Camera")))
        return;

    QString error;
    if (!m_repository.updateCamera(index, camera, &error)) {
        QMessageBox::warning(this, localizedText("保存失败", "Save Failed"), error);
        return;
    }
    populateFilterOptions();
    refresh();
}

void ThreeDCameraPage::removeCamera()
{
    if (!ensureLoaded())
        return;
    const int index = selectedCameraRepositoryIndex();
    if (index < 0 || index >= m_repository.cameras().size()) {
        QMessageBox::information(this, localizedText("未选择型号", "No Model Selected"),
            localizedText("请先在表格中选择一个自定义 3D 相机型号。", "Select a custom 3D camera model in the table first."));
        return;
    }
    const ThreeDCameraSpec camera = m_repository.cameras().at(index);
    if (!camera.userDefined) {
        QMessageBox::information(this, localizedText("不能删除内置型号", "Cannot Delete Built-in Model"),
            localizedText("内置 3D 相机不能删除；可复制为自定义型号后维护。", "Built-in 3D cameras cannot be deleted; copy one as a custom model first."));
        return;
    }

    const int answer = QMessageBox::question(this, localizedText("删除自定义 3D 相机", "Delete Custom 3D Camera"),
        localizedText("确定删除自定义型号“%1 %2”吗？", "Delete custom model \"%1 %2\"?")
            .arg(camera.manufacturer, camera.model));
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    if (!m_repository.removeCamera(index, &error)) {
        QMessageBox::warning(this, localizedText("删除失败", "Delete Failed"), error);
        return;
    }
    populateFilterOptions();
    refresh();
}

void ThreeDCameraPage::showDetailsForRow(int row)
{
    if (!m_details || !m_table || row < 0 || row >= m_table->rowCount())
        return;
    const int sourceIndex = rowSourceIndex(m_table, row);
    if (sourceIndex < 0 || sourceIndex >= m_matches.size())
        return;
    m_selectedMatchIndex = sourceIndex;
    m_detailsToggle->setEnabled(true);
    refreshSampling();
    if (m_detailsPanel->isHidden())
        return;

    const ThreeDCameraMatch &match = m_matches.at(sourceIndex);
    const ThreeDCameraSpec &spec = match.spec;
    const QString missing = match.missingFields.isEmpty()
        ? localizedText("无", "None")
        : match.missingFields.join(localizedText("；", "; "));
    const QString reasons = match.rejectionReasons.isEmpty()
        ? localizedText("无", "None")
        : match.rejectionReasons.join(localizedText("；", "; "));

    QString html;
    html += QStringLiteral("<h3>%1 %2</h3>").arg(htmlEscape(spec.manufacturer), htmlEscape(spec.model));
    const auto line = [](const QString &label, const QString &value) {
        return QStringLiteral("<b>%1</b>: %2<br>").arg(htmlEscape(label), value);
    };
    html += QStringLiteral("<p>");
    html += line(localizedText("匹配状态", "Match Status"), htmlEscape(threeDMatchStatusLabel(match.status)));
    html += line(localizedText("资料来源类型", "Source Type"), htmlEscape(sourceTypeText(spec)));
    html += line(localizedText("技术路线", "Technology"), htmlEscape(threeDTechnologyLabel(spec.technology)));
    html += line(localizedText("产品状态", "Product Status"), htmlEscape(localizedSpecText(spec.status)));
    if (spec.sourceUrl.trimmed().isEmpty()) {
        html += line(localizedText("资料来源", "Source"), htmlEscape(localizedText("用户录入", "User entered")));
    } else {
        html += QStringLiteral("<b>%1</b>: <a href=\"%2\">%2</a><br>")
            .arg(htmlEscape(localizedText("资料来源", "Source")), htmlEscape(spec.sourceUrl));
    }
    html += line(localizedText("采集日期", "Collection Date"), htmlEscape(spec.sourceDate));
    html += QStringLiteral("</p>");
    html += QStringLiteral("<p>");
    html += line(localizedText("缺失字段", "Missing Fields"), htmlEscape(missing));
    html += line(localizedText("不适配原因", "Mismatch Reasons"), htmlEscape(reasons));
    html += QStringLiteral("</p>");

    QString geometryHtml;
    geometryHtml += line(localizedText("X视场", "X FOV"), htmlEscape(fovRange(spec.xFovNearMm, spec.xFovReferenceMm, spec.xFovFarMm, QStringLiteral(" mm"))));
    geometryHtml += line(localizedText("Y视场", "Y FOV"), htmlEscape(fovRange(spec.yFovNearMm, spec.yFovReferenceMm, spec.yFovFarMm, QStringLiteral(" mm"))));
    geometryHtml += line(localizedText("Z测量范围", "Z Measurement Range"), htmlEscape(valueOrUnknown(spec.zMeasurementRangeMm, QStringLiteral(" mm"), 1)));
    geometryHtml += line(localizedText("参考距离", "Reference Distance"), htmlEscape(valueOrUnknown(spec.referenceDistanceMm, QStringLiteral(" mm"), 1)));
    html += QStringLiteral("<h4>%1</h4><p>%2</p>")
        .arg(htmlEscape(localizedText("几何覆盖", "Geometry Coverage")), geometryHtml);

    const QString accuracyCondition = spec.accuracyCondition.isEmpty()
        ? localizedText("未公开", "Unpublished")
        : localizedSpecText(spec.accuracyCondition);
    QString qualityHtml;
    qualityHtml += line(localizedText("Z轴重复精度", "Z Repeatability"), htmlEscape(valueOrUnknown(spec.zRepeatabilityUm, QStringLiteral(" um"), 2)));
    qualityHtml += line(localizedText("X轴重复精度", "X Repeatability"), htmlEscape(valueOrUnknown(spec.xRepeatabilityUm, QStringLiteral(" um"), 2)));
    qualityHtml += line(localizedText("X轴数据间隔", "X Data Interval"), htmlEscape(valueOrUnknown(spec.profileDataIntervalUm, QStringLiteral(" um"), 2)));
    qualityHtml += line(localizedText("Z轴线性度", "Z Linearity"), htmlEscape(linearityOrUnknown(spec.zLinearityPercentOfRange)));
    qualityHtml += line(localizedText("Z轴分辨率", "Z Resolution"), htmlEscape(valueOrUnknown(spec.zResolutionUm, QStringLiteral(" um"), 2)));
    qualityHtml += line(localizedText("测量精度", "Measurement Accuracy"), htmlEscape(valueOrUnknown(spec.measurementAccuracyUm, QStringLiteral(" um"), 1)));
    qualityHtml += line(localizedText("测试条件", "Test Conditions"), htmlEscape(accuracyCondition));
    html += QStringLiteral("<h4>%1</h4><p>%2</p>")
        .arg(htmlEscape(localizedText("精度质量", "Accuracy / Quality")), qualityHtml);
    const QString exposureRange = (threeDHasValue(spec.exposureTimeMinUs) || threeDHasValue(spec.exposureTimeMaxUs))
        ? QStringLiteral("%1 - %2")
            .arg(valueOrUnknown(spec.exposureTimeMinUs, QStringLiteral(" us"), 2),
                 valueOrUnknown(spec.exposureTimeMaxUs, QStringLiteral(" us"), 2))
        : localizedText("未公开", "Unpublished");
    html += QStringLiteral("<h4>%1</h4><p>%2<br>%3%4%5%6%7</p>")
        .arg(htmlEscape(localizedText("速度采样", "Speed / Sampling")), htmlEscape(speedSummary(spec)),
            line(localizedText("需要外部运动", "External Motion Required"), htmlEscape(yesNoUnknown(spec.requiresExternalMotion))),
            line(localizedText("支持编码器", "Encoder Supported"), htmlEscape(yesNoUnknown(spec.supportsEncoder))),
            line(localizedText("支持外部触发", "External Trigger Supported"), htmlEscape(yesNoUnknown(spec.supportsExternalTrigger))),
            line(localizedText("编码器输入上限", "Encoder Input Limit"), htmlEscape(valueOrUnknown(spec.encoderRateMaxHz, QStringLiteral(" Hz"), 0))),
            line(localizedText("曝光范围", "Exposure Range"), htmlEscape(exposureRange)));
    html += QStringLiteral("<h4>%1</h4><p>%2%3%4</p>")
        .arg(htmlEscape(localizedText("光学与集成", "Optics / Integration")),
            line(localizedText("光源", "Light Source"), htmlEscape(spec.lightSource.isEmpty() ? localizedText("未公开", "Unpublished") : localizedSpecText(spec.lightSource))),
            line(localizedText("波长", "Wavelength"), htmlEscape(valueOrUnknown(spec.wavelengthNm, QStringLiteral(" nm"), 0))),
            line(localizedText("接口", "Interfaces"), htmlEscape(localizedSpecList(spec.interfaces, QStringLiteral(", ")))));
    html += QStringLiteral("<h4>%1</h4><p>%2%3%4%5%6</p>")
        .arg(htmlEscape(localizedText("结构环境", "Structure / Environment")),
            line(QStringLiteral("IP"), htmlEscape(spec.ipRating.isEmpty() ? localizedText("未公开", "Unpublished") : spec.ipRating)),
            line(localizedText("结构", "Structure"), htmlEscape(spec.structure.isEmpty() ? localizedText("未公开", "Unpublished") : localizedSpecText(spec.structure))),
            line(localizedText("尺寸", "Dimensions"), htmlEscape(spec.dimensions.isEmpty() ? localizedText("未公开", "Unpublished") : spec.dimensions)),
            line(localizedText("重量", "Weight"), htmlEscape(valueOrUnknown(spec.weightG, QStringLiteral(" g"), 0))),
            line(localizedText("温度", "Temperature"), htmlEscape(spec.temperature.isEmpty() ? localizedText("未公开", "Unpublished") : spec.temperature)));
    if (!spec.materialScenarios.isEmpty())
        html += QStringLiteral("<p><b>%1</b>: %2</p>").arg(localizedText("材质场景", "Material Scenarios"), htmlEscape(localizedSpecList(spec.materialScenarios, localizedText("，", ", "))));
    if (!spec.notes.isEmpty())
        html += QStringLiteral("<p><b>%1</b>: %2</p>").arg(localizedText("备注", "Notes"), htmlEscape(localizedSpecList(spec.notes, localizedText("；", "; "))));
    html += QStringLiteral("<h4>%1</h4>%2").arg(localizedText("官方原始规格字段", "Official Raw Spec Fields"), rawSpecsHtml(spec.rawSpecs));
    m_details->setHtml(html);
}
