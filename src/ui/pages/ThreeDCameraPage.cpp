#include "ui/pages/ThreeDCameraPage.h"

#include "ui/UiHelpers.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
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
#include <QPushButton>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBrowser>
#include <QVBoxLayout>

using namespace UiHelpers;

namespace {
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
    combo->clear();
    combo->addItem(allText());
    QStringList sorted = values;
    sorted.removeDuplicates();
    sorted.sort(Qt::CaseInsensitive);
    for (const QString &value : sorted) {
        if (!value.trimmed().isEmpty())
            combo->addItem(value);
    }
}

QString comboRequirement(const QComboBox *combo)
{
    if (!combo || combo->currentIndex() <= 0)
        return QString();
    return combo->currentText();
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
}

ThreeDCameraPage::ThreeDCameraPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);
    layout->addWidget(pageHeader(localizedText("3D 相机助手", "3D Camera Assistant"),
        localizedText("按几何范围、精度、速度和集成条件筛选 3D 相机资料库。", "Filter the 3D camera library by geometry, accuracy, speed, and integration constraints.")));

    m_summaryLabel = new QLabel;
    m_summaryLabel->setObjectName(QStringLiteral("SectionTitle"));
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_summaryLabel);

    buildFilters(layout);

    QSplitter *splitter = new QSplitter(Qt::Vertical);
    m_table = new QTableWidget;
    setupTable(m_table);
    m_table->setSortingEnabled(false);
    m_table->setWordWrap(false);
    m_table->setTextElideMode(Qt::ElideRight);
    m_table->verticalHeader()->setDefaultSectionSize(30);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_table->setColumnCount(9);
    m_table->setHorizontalHeaderLabels({
        localizedText("匹配状态", "Match Status"),
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
    m_table->setColumnWidth(0, 96);
    m_table->setColumnWidth(1, 88);
    m_table->setColumnWidth(2, 120);
    m_table->setColumnWidth(3, 120);
    m_table->setColumnWidth(4, 140);
    m_table->setColumnWidth(6, 240);
    m_table->setColumnWidth(7, 150);
    m_table->setColumnWidth(8, 180);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) { showDetailsForRow(row); });
    connect(m_table, &QTableWidget::cellActivated, this, [this](int row, int) { showDetailsForRow(row); });
    splitter->addWidget(m_table);

    m_details = new QTextBrowser;
    m_details->setOpenExternalLinks(true);
    m_details->setMinimumHeight(170);
    splitter->addWidget(m_details);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter, 1);

    populateFilterOptions();
    m_summaryLabel->setText(localizedText(
        "3D 型号库将在首次进入页面或应用筛选时加载。该页面只做 3D 查询过滤，不参与 2D 评分、推荐、BOM 或 PDF 报告。",
        "The 3D model library loads when this page is first opened or filters are applied. This page only queries and filters 3D products; it does not affect 2D scoring, recommendations, BOM, or PDF reports."));
}

void ThreeDCameraPage::activate()
{
    if (!m_resultsInitialized)
        refresh();
}

void ThreeDCameraPage::buildFilters(QLayout *parentLayout)
{
    QFrame *panel = new QFrame;
    panel->setObjectName(QStringLiteral("FilterPanel"));
    QVBoxLayout *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(18, 16, 18, 18);
    panelLayout->setSpacing(14);

    QHBoxLayout *headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(12);
    QWidget *headerText = new QWidget(panel);
    headerText->setObjectName(QStringLiteral("FilterHeaderText"));
    QVBoxLayout *headerTextLayout = new QVBoxLayout(headerText);
    headerTextLayout->setContentsMargins(0, 0, 0, 0);
    headerTextLayout->setSpacing(3);
    QLabel *titleLabel = new QLabel(localizedText("需求过滤", "Requirement Filters"));
    titleLabel->setObjectName(QStringLiteral("FilterPanelTitle"));
    QLabel *subtitleLabel = new QLabel(localizedText(
        "按型号属性、空间范围、精度速度和集成条件筛选 3D 相机。",
        "Filter 3D cameras by model attributes, working range, accuracy, speed, and integration constraints."));
    subtitleLabel->setObjectName(QStringLiteral("FilterPanelSubtitle"));
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    headerTextLayout->addWidget(titleLabel);
    headerTextLayout->addWidget(subtitleLabel);
    headerLayout->addWidget(headerText, 1);
    headerLayout->addWidget(statusBadge(localizedText("独立 3D 查询", "Standalone 3D Query"), QStringLiteral("info")),
        0, Qt::AlignRight | Qt::AlignTop);
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
        holder->setObjectName(QStringLiteral("FilterField"));
        QVBoxLayout *layout = new QVBoxLayout(holder);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(5);
        QLabel *label = new QLabel(labelText);
        label->setObjectName(QStringLiteral("FilterFieldLabel"));
        label->setWordWrap(false);
        label->setToolTip(labelText);
        prepareControl(control);
        layout->addWidget(label);
        layout->addWidget(control);
        return holder;
    };

    struct FilterGroup {
        QFrame *frame;
        QGridLayout *grid;
    };
    const auto makeGroup = [](const QString &title) -> FilterGroup {
        QFrame *group = new QFrame;
        group->setObjectName(QStringLiteral("FilterGroup"));
        group->setMinimumWidth(0);
        group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        QVBoxLayout *layout = new QVBoxLayout(group);
        layout->setContentsMargins(12, 11, 12, 12);
        layout->setSpacing(10);
        QLabel *titleLabel = new QLabel(title);
        titleLabel->setObjectName(QStringLiteral("FilterGroupTitle"));
        layout->addWidget(titleLabel);
        QGridLayout *grid = new QGridLayout;
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(10);
        grid->setVerticalSpacing(9);
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
    groupsLayout->setSpacing(12);
    groupsLayout->addWidget(attributes.frame, 1);
    groupsLayout->addWidget(geometry.frame, 1);
    groupsLayout->addWidget(performance.frame, 1);
    panelLayout->addLayout(groupsLayout);

    QPushButton *applyButton = actionButton(localizedText("应用筛选", "Apply Filters"), QStringLiteral(":/icons/ui/calculate.png"));
    QPushButton *clearButton = actionButton(localizedText("清空条件", "Clear"), QStringLiteral(":/icons/ui/info.png"), true);

    QHBoxLayout *actionLayout = new QHBoxLayout;
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(10);
    QLabel *hintLabel = new QLabel(localizedText("数值为“不限”时不会参与过滤。", "Numeric fields set to Any are ignored."));
    hintLabel->setObjectName(QStringLiteral("FilterHint"));
    actionLayout->addWidget(hintLabel, 1);
    actionLayout->addWidget(clearButton);
    actionLayout->addWidget(applyButton);
    panelLayout->addLayout(actionLayout);

    connect(applyButton, &QPushButton::clicked, this, &ThreeDCameraPage::refresh);
    connect(clearButton, &QPushButton::clicked, this, &ThreeDCameraPage::clearFilters);
    parentLayout->addWidget(panel);
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

void ThreeDCameraPage::refresh()
{
    if (!ensureLoaded())
        return;

    ThreeDCameraMatcher matcher;
    m_matches = matcher.match(requirement(), m_repository.cameras());
    m_resultsInitialized = true;
    fillTable();
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
        "3D 型号库 %1 个；满足 %2，待确认 %3，不满足 %4。该页面只做 3D 查询过滤，不参与 2D 评分、推荐、BOM 或 PDF 报告。",
        "3D library: %1 models; meets %2, needs confirmation %3, does not meet %4. This page only queries and filters 3D products; it does not affect 2D scoring, recommendations, BOM, or PDF reports.")
        .arg(m_matches.size()).arg(matched).arg(missing).arg(rejected));

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
            m_table->setItem(row, 1, item(spec.manufacturer));
            m_table->setItem(row, 2, item(spec.series));
            m_table->setItem(row, 3, item(spec.model));
            m_table->setItem(row, 4, item(threeDTechnologyLabel(spec.technology)));
            m_table->setItem(row, 5, item(geometrySummary(spec)));
            m_table->setItem(row, 6, item(qualitySummary(spec)));
            m_table->setItem(row, 7, item(speedSummary(spec)));
            m_table->setItem(row, 8, item(spec.interfaces.join(QStringLiteral(", "))));
        }
        m_table->setUpdatesEnabled(true);
    }
    if (!m_matches.isEmpty()) {
        m_table->selectRow(0);
        showDetailsForRow(0);
    } else if (m_details) {
        m_details->clear();
    }
}

void ThreeDCameraPage::showDetailsForRow(int row)
{
    if (!m_details || !m_table || row < 0 || row >= m_table->rowCount())
        return;
    const int sourceIndex = rowSourceIndex(m_table, row);
    if (sourceIndex < 0 || sourceIndex >= m_matches.size())
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
    html += line(localizedText("技术路线", "Technology"), htmlEscape(threeDTechnologyLabel(spec.technology)));
    html += line(localizedText("产品状态", "Product Status"), htmlEscape(spec.status));
    html += QStringLiteral("<b>%1</b>: <a href=\"%2\">%2</a><br>")
        .arg(htmlEscape(localizedText("官方来源", "Official Source")), htmlEscape(spec.sourceUrl));
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
        : spec.accuracyCondition;
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
    html += QStringLiteral("<h4>%1</h4><p>%2<br>%3%4</p>")
        .arg(htmlEscape(localizedText("速度采样", "Speed / Sampling")), htmlEscape(speedSummary(spec)),
            line(localizedText("需要外部运动", "External Motion Required"), htmlEscape(yesNoUnknown(spec.requiresExternalMotion))),
            line(localizedText("支持编码器", "Encoder Supported"), htmlEscape(yesNoUnknown(spec.supportsEncoder))));
    html += QStringLiteral("<h4>%1</h4><p>%2%3%4</p>")
        .arg(htmlEscape(localizedText("光学与集成", "Optics / Integration")),
            line(localizedText("光源", "Light Source"), htmlEscape(spec.lightSource.isEmpty() ? localizedText("未公开", "Unpublished") : spec.lightSource)),
            line(localizedText("波长", "Wavelength"), htmlEscape(valueOrUnknown(spec.wavelengthNm, QStringLiteral(" nm"), 0))),
            line(localizedText("接口", "Interfaces"), htmlEscape(spec.interfaces.join(QStringLiteral(", ")))));
    html += QStringLiteral("<h4>%1</h4><p>%2%3%4%5%6</p>")
        .arg(htmlEscape(localizedText("结构环境", "Structure / Environment")),
            line(QStringLiteral("IP"), htmlEscape(spec.ipRating.isEmpty() ? localizedText("未公开", "Unpublished") : spec.ipRating)),
            line(localizedText("结构", "Structure"), htmlEscape(spec.structure.isEmpty() ? localizedText("未公开", "Unpublished") : spec.structure)),
            line(localizedText("尺寸", "Dimensions"), htmlEscape(spec.dimensions.isEmpty() ? localizedText("未公开", "Unpublished") : spec.dimensions)),
            line(localizedText("重量", "Weight"), htmlEscape(valueOrUnknown(spec.weightG, QStringLiteral(" g"), 0))),
            line(localizedText("温度", "Temperature"), htmlEscape(spec.temperature.isEmpty() ? localizedText("未公开", "Unpublished") : spec.temperature)));
    if (!spec.materialScenarios.isEmpty())
        html += QStringLiteral("<p><b>%1</b>: %2</p>").arg(localizedText("材质场景", "Material Scenarios"), htmlEscape(spec.materialScenarios.join(localizedText("，", ", "))));
    if (!spec.notes.isEmpty())
        html += QStringLiteral("<p><b>%1</b>: %2</p>").arg(localizedText("备注", "Notes"), htmlEscape(spec.notes.join(localizedText("；", "; "))));
    html += QStringLiteral("<h4>%1</h4>%2").arg(localizedText("官方原始规格字段", "Official Raw Spec Fields"), rawSpecsHtml(spec.rawSpecs));
    m_details->setHtml(html);
}
