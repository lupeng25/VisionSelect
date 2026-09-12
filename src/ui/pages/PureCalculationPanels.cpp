#include "ui/pages/PureCalculationPage.h"
#include "ui/ParameterNumberField.h"
#include "ui/ParameterUi.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

using namespace UiHelpers;
using namespace ParameterUi;

ParameterNumberField *PureCalculationPage::addNumber(QGridLayout *grid, const QString &key, const QString &label,
                                                    const QString &unit, int row, int column, bool integer)
{
    auto *field = new ParameterNumberField(key, label, unit, integer, grid->parentWidget());
    m_fields.insert(key, field);
    m_wrappers.insert(key, field);
    grid->addWidget(field, row, column);
    grid->setColumnStretch(column, 1);
    connect(field, &ParameterNumberField::changed, this, [this, key]() { fieldChanged(key); });
    return field;
}
QComboBox *PureCalculationPage::addChoice(QGridLayout *grid, const QString &key, const QString &label,
                                         const QStringList &labels, const QStringList &values, int row, int column, int span)
{
    auto *wrapper = new QWidget(grid->parentWidget());
    wrapper->setObjectName(QStringLiteral("ParameterField"));
    auto *layout = new QVBoxLayout(wrapper);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    auto *caption = new QLabel(label, wrapper);
    caption->setWordWrap(true);
    auto *combo = new QComboBox(wrapper);
    combo->setObjectName(key);
    combo->setAccessibleName(label);
    combo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    for (int i = 0; i < labels.size(); ++i) combo->addItem(labels.at(i), values.at(i));
    caption->setBuddy(combo);
    layout->addWidget(caption);
    layout->addWidget(combo);
    grid->addWidget(wrapper, row, column, 1, span);
    m_choices.insert(key, combo);
    m_wrappers.insert(key, wrapper);
    connect(combo, &QComboBox::currentIndexChanged, this, [this, key]() { fieldChanged(key); });
    return combo;
}
QCheckBox *PureCalculationPage::addFlag(QGridLayout *grid, const QString &key, const QString &label, int row)
{
    auto *flag = new QCheckBox(label, grid->parentWidget());
    flag->setObjectName(key);
    flag->setAccessibleName(label);
    grid->addWidget(flag, row, 0, 1, 2);
    m_flags.insert(key, flag);
    m_wrappers.insert(key, flag);
    connect(flag, &QCheckBox::toggled, this, [this, key]() { fieldChanged(key); });
    return flag;
}
QLineEdit *PureCalculationPage::addText(QGridLayout *grid, const QString &key, const QString &label, int row, int column, int span)
{
    auto *wrapper = new QWidget(grid->parentWidget());
    wrapper->setObjectName(QStringLiteral("ParameterField"));
    auto *layout = new QVBoxLayout(wrapper);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *caption = new QLabel(label, wrapper);
    caption->setWordWrap(true);
    auto *edit = new QLineEdit(wrapper);
    edit->setObjectName(key);
    edit->setAccessibleName(label);
    edit->setPlaceholderText(localizedText("未填写", "Not entered"));
    edit->setMaxLength(160);
    edit->setMinimumWidth(0);
    edit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    caption->setBuddy(edit);
    layout->addWidget(caption);
    layout->addWidget(edit);
    grid->addWidget(wrapper, row, column, 1, span);
    m_texts.insert(key, edit);
    m_wrappers.insert(key, wrapper);
    connect(edit, &QLineEdit::textChanged, this, [this, key]() { fieldChanged(key); });
    return edit;
}
QGridLayout *PureCalculationPage::makePanel(const QString &description)
{
    auto *panel = new QWidget(m_inputs);
    panel->setObjectName(QStringLiteral("ParameterInputPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *intro = new QLabel(description, panel);
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    layout->addLayout(grid);
    layout->addStretch();
    m_inputs->addWidget(panel);
    return grid;
}

void PureCalculationPage::buildPanels()
{
    const QStringList modelLabels = {localizedText("近轴粗算 · 机械距离估计", "Paraxial · approximate WD"),
                                     localizedText("薄透镜 · 主平面物距", "Thin lens · principal-plane distance")};
    const QStringList models = {"paraxial", "thin"};
    QGridLayout *grid = nullptr;
    const auto field = [&](const char *key, const char *zh, const char *en, const char *unit, int row, int col, bool integer = false) {
        addNumber(grid, key, localizedText(zh, en), QString::fromUtf8(unit), row, col, integer);
    };
    grid = makePanel(localizedText("先选求解方向。焦距比较同时检查两个方向的覆盖。", "Choose a solve direction. Focal comparisons check both FOV axes."));
    addChoice(grid, "optics.solve", localizedText("要求什么", "Solve for"),
              {localizedText("焦距", "Focal length"), localizedText("视场", "Field of view"), localizedText("距离", "Distance")},
              {"focal", "fov", "distance"}, 0, 0);
    addChoice(grid, "optics.model", localizedText("计算模型", "Optical model"), modelLabels, models, 0, 1);
    addChoice(grid, "optics.target", localizedText("需求视场来源", "Required FOV source"),
              {localizedText("直接输入矩形视场", "Rectangular FOV"), localizedText("工件尺寸 + 单边余量", "Part dimensions + margin"), localizedText("圆形目标", "Circular target")},
              {"fov", "part", "circle"}, 1, 0, 2);
    field("optics.width", "需求视场 X", "Required FOV X", "mm", 2, 0);
    field("optics.height", "需求视场 Y", "Required FOV Y", "mm", 2, 1);
    field("optics.diameter", "目标直径", "Object diameter", "mm", 3, 0);
    field("optics.margin", "单边余量", "Margin per side", "mm", 3, 1);
    field("optics.distance", "距离", "Distance", "mm", 4, 0);
    field("optics.focal", "已有焦距", "Known focal length", "mm", 4, 1);
    field("optics.targetPixel", "采样上限（可选）", "Pixel-scale limit (optional)", "μm/px", 5, 0);

    grid = makePanel(localizedText("把采样间隔与测量精度分开。也可记录局部实测标定。", "Separate sampling pitch from accuracy, or record a local measured calibration."));
    addChoice(grid, "sampling.solve", localizedText("计算任务", "Sampling task"),
              {localizedText("所需分辨率", "Required resolution"), localizedText("已有相机像素当量", "Existing camera pixel scale"), localizedText("实测标定", "Measured calibration")},
              {"required", "actual", "calibration"}, 0, 0, 2);
    field("sampling.width", "视场 X", "FOV X", "mm", 1, 0);
    field("sampling.height", "视场 Y", "FOV Y", "mm", 1, 1);
    field("sampling.feature", "最小特征", "Smallest feature", "μm", 2, 0);
    field("sampling.featurePixels", "每特征要求像素数", "Pixels per feature", "px", 2, 1);
    addFlag(grid, "sampling.measurement", localizedText("增加测量误差采样预算", "Include a measurement sampling budget"), 3);
    field("sampling.tolerance", "允许测量误差", "Allowed measurement error", "μm", 4, 0);
    field("sampling.tolerancePixels", "误差预算对应像素数", "Pixels per error budget", "px", 4, 1);
    field("sampling.length", "实测长度", "Measured length", "mm", 5, 0);
    field("sampling.pixelDistance", "图像像素距离", "Image pixel distance", "px", 5, 1);
    addChoice(grid, "sampling.axis", localizedText("标定方向", "Calibration axis"), {"X", "Y"}, {"x", "y"}, 6, 0);
    addText(grid, "sampling.region", localizedText("标定位置 / 适用区域", "Calibration region / scope"), 6, 1);

    grid = makePanel(localizedText("校核当前相机与镜头。未知项保留待补状态，不计为通过。", "Check the current camera and lens. Missing data remain unknown, not passed."));
    addChoice(grid, "check.lens", localizedText("镜头类型", "Lens type"),
              {localizedText("普通定焦", "Fixed focal"), localizedText("远心", "Telecentric")}, {"fixed", "tele"}, 0, 0);
    addChoice(grid, "check.geometry", localizedText("当前视场来源", "Actual FOV source"),
              {localizedText("按镜头参数估算", "Estimate from optics"), localizedText("使用实测视场", "Use measured FOV")}, {"estimate", "measured"}, 0, 1);
    addChoice(grid, "check.model", localizedText("普通镜头模型", "Fixed-lens model"), modelLabels, models, 1, 0, 2);
    field("check.width", "需求视场 X", "Required FOV X", "mm", 2, 0);
    field("check.height", "需求视场 Y", "Required FOV Y", "mm", 2, 1);
    field("check.targetPixel", "像素当量上限", "Maximum pixel scale", "μm/px", 3, 0);
    field("check.distance", "安装距离", "Installation distance", "mm", 3, 1);
    field("check.focal", "镜头焦距", "Lens focal length", "mm", 4, 0);
    field("check.mag", "远心倍率", "Telecentric magnification", "×", 4, 1);
    field("check.actualWidth", "实测视场 X", "Measured FOV X", "mm", 5, 0);
    field("check.actualHeight", "实测视场 Y", "Measured FOV Y", "mm", 5, 1);
    field("check.fps", "需求帧率", "Required frame rate", "fps", 6, 0);
    field("check.maxFps", "相机标称最大帧率", "Nominal maximum frame rate", "fps", 6, 1);
    addChoice(grid, "check.format", localizedText("传输像素格式", "Transport pixel format"), formatLabels(), formats(), 7, 0, 2);
    field("check.capacity", "有效链路容量", "Usable link capacity", "MB/s", 8, 0);
    field("check.imageCircle", "镜头像圈", "Lens image circle", "mm", 8, 1);
    addFlag(grid, "check.advanced", localizedText("展开安装、景深与运动条件", "Installation, depth and motion conditions"), 9);
    addText(grid, "check.cameraMount", localizedText("相机镜头接口", "Camera lens mount"), 10, 0);
    addText(grid, "check.lensMount", localizedText("镜头接口", "Lens mount"), 10, 1);
    field("check.minDistance", "最小工作距离", "Minimum working distance", "mm", 11, 0);
    field("check.nominalDistance", "远心标称工作距离", "Nominal telecentric WD", "mm", 11, 0);
    field("check.distanceTolerance", "工作距离允许偏差", "Allowed WD offset", "mm", 11, 1);
    field("check.heightRange", "高度范围（峰峰值）", "Height range (peak-to-peak)", "mm", 12, 0);
    field("check.dof", "规格景深", "Specified depth of field", "mm", 12, 1);
    addFlag(grid, "check.dofConfirmed", localizedText("景深适用于当前倍率与光圈", "DOF applies to current magnification and aperture"), 13);
    field("check.telecentricity", "远心度", "Telecentricity", "°", 14, 0);
    field("check.error", "允许测量误差", "Allowed measurement error", "μm", 14, 1);
    addFlag(grid, "check.motion", localizedText("校核连续运动拖影", "Check continuous-motion blur"), 15);
    field("check.speed", "物方运动速度", "Object speed", "mm/s", 16, 0);
    field("check.exposure", "当前曝光时间", "Current exposure", "μs", 16, 1);
    field("check.blur", "允许拖影", "Allowed blur", "px", 17, 0);

    grid = makePanel(localizedText("用固定倍率计算视场，或求同时满足覆盖与采样的倍率区间。", "Calculate FOV from fixed magnification or find a feasible coverage/sampling interval."));
    addChoice(grid, "tele.solve", localizedText("要求什么", "Solve for"),
              {localizedText("可行倍率范围", "Feasible magnification range"), localizedText("视场", "Field of view"), localizedText("倍率上限", "Maximum magnification")},
              {"range", "fov", "mag"}, 0, 0, 2);
    field("tele.width", "需求视场 X", "Required FOV X", "mm", 1, 0);
    field("tele.height", "需求视场 Y", "Required FOV Y", "mm", 1, 1);
    field("tele.targetPixel", "像素当量上限", "Maximum pixel scale", "μm/px", 2, 0);
    field("tele.mag", "已有倍率", "Known magnification", "×", 2, 1);

    grid = makePanel(localizedText("按选定方向的像素当量估算匀速拖影。速度为零时不限制运动曝光。", "Estimate constant-motion blur along one axis. Zero speed imposes no motion-exposure limit."));
    addChoice(grid, "motion.solve", localizedText("要求什么", "Solve for"),
              {localizedText("最大曝光", "Maximum exposure"), localizedText("预计拖影", "Predicted blur"), localizedText("允许速度", "Allowed speed")},
              {"exposure", "blur", "speed"}, 0, 0, 2);
    addChoice(grid, "motion.source", localizedText("像素当量来源", "Pixel scale source"),
              {localizedText("手动输入", "Manual"), localizedText("方案校核 · X", "System check · X"), localizedText("方案校核 · Y", "System check · Y")},
              {"manual", "checkX", "checkY"}, 1, 0, 2);
    field("motion.pixel", "沿运动轴的像素当量", "Pixel scale along motion axis", "μm/px", 2, 0);
    field("motion.speed", "物方速度", "Object speed", "mm/s", 2, 1);
    field("motion.exposure", "曝光时间", "Exposure time", "μs", 3, 0);
    field("motion.blur", "允许拖影", "Allowed blur", "px", 3, 1);

    grid = makePanel(localizedText("按实际输出像素格式计量。ROI 留空表示使用上方完整分辨率。", "Use the actual output pixel format. Blank ROI uses the full resolution above."));
    addChoice(grid, "data.format", localizedText("传输像素格式", "Transport pixel format"), formatLabels(), formats(), 0, 0, 2);
    field("data.roiWidth", "ROI 宽度（可选）", "ROI width (optional)", "px", 1, 0, true);
    field("data.roiHeight", "ROI 高度（可选）", "ROI height (optional)", "px", 1, 1, true);
    field("data.fps", "每台相机帧率", "Frame rate per camera", "fps", 2, 0);
    field("data.count", "共享链路相机数", "Cameras sharing the link", "", 2, 1, true);
    field("data.capacity", "共享链路总容量（可选）", "Shared link capacity (optional)", "MB/s", 3, 0);
    field("data.overhead", "协议与调度预留", "Protocol / scheduling overhead", "%", 3, 1);
    field("data.hours", "连续保存时长", "Continuous recording time", "h", 4, 0);
    addChoice(grid, "data.storage", localizedText("主机保存格式", "Host storage format"),
              {localizedText("原样保存传输载荷", "Save transport payload"), "Mono8", "Mono16", "RGB8"}, {"", "Mono8", "Mono16", "RGB8"}, 4, 1);
}

void PureCalculationPage::updateVisibility()
{
    const auto show = [&](const QString &key, bool visible) { m_wrappers.value(key)->setVisible(visible); };
    const QString active = task();
    const bool samplingActual = choice("sampling.solve") == QLatin1String("actual");
    m_cameraPanel->setVisible(active != QLatin1String("motion") && (active != QLatin1String("sampling") || samplingActual));
    show("camera.pixel", active != QLatin1String("data") && active != QLatin1String("sampling"));
    const bool optical = active == QLatin1String("optics") || active == QLatin1String("check") || active == QLatin1String("tele");
    m_importLens->setVisible(optical);
    m_matchLens->setVisible(optical);
    const bool circle = choice("optics.target") == QLatin1String("circle");
    const bool part = choice("optics.target") == QLatin1String("part");
    show("optics.width", !circle); show("optics.height", !circle);
    show("optics.diameter", circle); show("optics.margin", part);
    m_fields.value("optics.width")->setTitle(part ? localizedText("工件宽度", "Part width") : localizedText("需求视场 X", "Required FOV X"));
    m_fields.value("optics.height")->setTitle(part ? localizedText("工件高度", "Part height") : localizedText("需求视场 Y", "Required FOV Y"));
    show("optics.focal", choice("optics.solve") != QLatin1String("focal"));
    show("optics.distance", choice("optics.solve") != QLatin1String("distance"));
    m_fields.value("optics.distance")->setTitle(choice("optics.model") == QLatin1String("thin")
        ? localizedText("主平面物距", "Principal-plane distance") : localizedText("工作距离（估算基准）", "Working distance (estimate)"));
    const bool calibration = choice("sampling.solve") == QLatin1String("calibration");
    for (const auto &key : {"sampling.width", "sampling.height", "sampling.feature"}) show(key, !calibration);
    show("sampling.featurePixels", !calibration && !samplingActual);
    show("sampling.measurement", !calibration && !samplingActual);
    const bool budget = !calibration && !samplingActual && m_flags.value("sampling.measurement")->isChecked();
    show("sampling.tolerance", budget); show("sampling.tolerancePixels", budget);
    for (const auto &key : {"sampling.length", "sampling.pixelDistance", "sampling.axis", "sampling.region"}) show(key, calibration);
    const bool tele = choice("check.lens") == QLatin1String("tele");
    const bool measured = choice("check.geometry") == QLatin1String("measured");
    show("check.model", !tele && !measured); show("check.focal", !tele && !measured); show("check.mag", tele && !measured);
    show("check.actualWidth", measured); show("check.actualHeight", measured);
    m_fields.value("check.distance")->setTitle(!tele && !measured && choice("check.model") == QLatin1String("thin")
        ? localizedText("主平面物距", "Principal-plane distance") : localizedText("安装工作距离", "Installation WD"));
    const bool advanced = m_flags.value("check.advanced")->isChecked();
    for (const auto &key : {"check.cameraMount", "check.lensMount", "check.heightRange", "check.dof", "check.dofConfirmed", "check.motion"}) show(key, advanced);
    show("check.minDistance", advanced && !tele);
    for (const auto &key : {"check.nominalDistance", "check.distanceTolerance", "check.telecentricity", "check.error"}) show(key, advanced && tele);
    const bool motion = advanced && m_flags.value("check.motion")->isChecked();
    for (const auto &key : {"check.speed", "check.exposure", "check.blur"}) show(key, motion);
    const bool teleFov = choice("tele.solve") == QLatin1String("fov");
    show("tele.mag", teleFov);
    // FOV 求解时仍保留可选目标供比较，不把旧目标静默变为未知。
    show("tele.targetPixel", choice("tele.solve") != QLatin1String("mag"));
    show("motion.speed", choice("motion.solve") != QLatin1String("speed"));
    show("motion.exposure", choice("motion.solve") != QLatin1String("exposure"));
    show("motion.blur", choice("motion.solve") != QLatin1String("blur"));
    show("motion.pixel", choice("motion.source") == QLatin1String("manual"));
    m_applyCheck->setVisible(active == QLatin1String("optics") || active == QLatin1String("tele") || (active == QLatin1String("sampling") && !calibration));
    m_inputs->updateGeometry();
}

Number PureCalculationPage::number(const QString &key) const
{
    const auto *field = m_fields.value(key, nullptr);
    return field ? field->value() : Number();
}
QString PureCalculationPage::choice(const QString &key) const
{
    const auto *combo = m_choices.value(key, nullptr);
    return combo ? combo->currentData().toString() : QString();
}
void PureCalculationPage::setNumber(const QString &key, Number value)
{
    if (auto *field = m_fields.value(key, nullptr)) field->setValue(value);
}
void PureCalculationPage::setChoice(const QString &key, const QString &value)
{
    if (auto *combo = m_choices.value(key, nullptr)) {
        const int index = combo->findData(value);
        combo->setCurrentIndex(index >= 0 ? index : 0);
    }
}
Parameters::Sensor PureCalculationPage::sensor() const { return {number("camera.nx"), number("camera.ny"), number("camera.pixel")}; }
QPair<Number, Number> PureCalculationPage::targetFov(const QString &prefix) const
{
    Number width = number(prefix + ".width"), height = number(prefix + ".height");
    if (choice(prefix + ".target") == QLatin1String("circle")) width = height = number(prefix + ".diameter");
    else if (choice(prefix + ".target") == QLatin1String("part")) {
        const auto margin = number(prefix + ".margin");
        if (!margin) return {};
        if (width) width = *width + 2 * *margin;
        if (height) height = *height + 2 * *margin;
    }
    return {width, height};
}
Parameters::OpticsInput PureCalculationPage::opticsInput() const
{
    Parameters::OpticsInput input;
    input.sensor = sensor();
    input.model = choice("optics.model") == QLatin1String("thin") ? Parameters::OpticsModel::ThinLens : Parameters::OpticsModel::Paraxial;
    input.solve = choice("optics.solve") == QLatin1String("fov") ? Parameters::OpticsSolve::FieldOfView
        : (choice("optics.solve") == QLatin1String("distance") ? Parameters::OpticsSolve::Distance : Parameters::OpticsSolve::FocalLength);
    input.focalLengthMm = number("optics.focal"); input.distanceMm = number("optics.distance");
    const auto fov = targetFov("optics");
    input.targetFovWidthMm = fov.first; input.targetFovHeightMm = fov.second;
    return input;
}
Parameters::TelecentricInput PureCalculationPage::telecentricInput() const
{
    Parameters::TelecentricInput input;
    input.sensor = sensor();
    input.solve = choice("tele.solve") == QLatin1String("fov") ? Parameters::TelecentricSolve::FieldOfView
        : (choice("tele.solve") == QLatin1String("mag") ? Parameters::TelecentricSolve::Magnification : Parameters::TelecentricSolve::Range);
    input.magnification = number("tele.mag");
    input.targetFovWidthMm = number("tele.width"); input.targetFovHeightMm = number("tele.height");
    input.targetObjectPixelUm = number("tele.targetPixel");
    return input;
}
Parameters::SystemInput PureCalculationPage::systemInput() const
{
    Parameters::SystemInput input;
    input.sensor = sensor();
    input.model = choice("check.model") == QLatin1String("thin") ? Parameters::OpticsModel::ThinLens : Parameters::OpticsModel::Paraxial;
    input.telecentric = choice("check.lens") == QLatin1String("tele");
    input.focalLengthMm = number("check.focal"); input.distanceMm = number("check.distance"); input.magnification = number("check.mag");
    input.measuredFov = choice("check.geometry") == QLatin1String("measured");
    input.measuredFovWidthMm = number("check.actualWidth"); input.measuredFovHeightMm = number("check.actualHeight");
    input.targetFovWidthMm = number("check.width"); input.targetFovHeightMm = number("check.height");
    input.targetObjectPixelUm = number("check.targetPixel");
    input.fps = number("check.fps"); input.maxFps = number("check.maxFps"); input.capacityMBps = number("check.capacity");
    input.pixelFormat = choice("check.format"); input.imageCircleMm = number("check.imageCircle");
    input.cameraMount = m_texts.value("check.cameraMount")->text().trimmed();
    input.lensMount = m_texts.value("check.lensMount")->text().trimmed();
    input.minWorkingDistanceMm = number("check.minDistance"); input.nominalWorkingDistanceMm = number("check.nominalDistance");
    input.workingDistanceToleranceMm = number("check.distanceTolerance"); input.heightVariationMm = number("check.heightRange");
    input.dofMm = number("check.dof"); input.dofConditionsConfirmed = m_flags.value("check.dofConfirmed")->isChecked();
    input.telecentricityDeg = number("check.telecentricity"); input.measurementToleranceUm = number("check.error");
    input.motionEnabled = m_flags.value("check.motion")->isChecked();
    input.speedMmS = number("check.speed"); input.exposureUs = number("check.exposure"); input.allowedBlurPixels = number("check.blur");
    return input;
}
