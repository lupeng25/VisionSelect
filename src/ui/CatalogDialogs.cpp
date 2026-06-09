#include "ui/CatalogDialogs.h"

#include "ui/UiHelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QScrollArea>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

using namespace UiHelpers;

namespace {
void addDialogSection(QFormLayout *form, const QString &title)
{
    QLabel *label = new QLabel(title);
    label->setObjectName(QStringLiteral("FilterGroupTitle"));
    form->addRow(label);
}

QDoubleSpinBox *optionalThreeDSpin(double max, double value, const QString &suffix, int decimals = 2)
{
    QDoubleSpinBox *spin = dialogSpin(0.0, max, threeDHasValue(value) ? value : 0.0, suffix, decimals);
    spin->setSpecialValueText(localizedText("未公开", "Unpublished"));
    return spin;
}

QSpinBox *optionalThreeDInt(int max, int value, const QString &suffix = QString())
{
    QSpinBox *spin = dialogIntSpin(0, max, value >= 0 ? value : 0, suffix);
    spin->setSpecialValueText(localizedText("未公开", "Unpublished"));
    return spin;
}

double optionalThreeDValue(const QDoubleSpinBox *spin)
{
    return spin && spin->value() > 0.0 ? spin->value() : -1.0;
}

int optionalThreeDIntValue(const QSpinBox *spin)
{
    return spin && spin->value() > 0 ? spin->value() : -1;
}

QComboBox *tristateCombo(int value)
{
    QComboBox *combo = new QComboBox;
    combo->addItem(localizedText("未公开", "Unpublished"), -1);
    combo->addItem(localizedText("是", "Yes"), 1);
    combo->addItem(localizedText("否", "No"), 0);
    const int index = combo->findData(value);
    combo->setCurrentIndex(index >= 0 ? index : 0);
    return combo;
}

int tristateComboValue(const QComboBox *combo)
{
    return combo ? combo->currentData().toInt() : -1;
}

QString listText(const QStringList &values)
{
    return values.join(QLatin1Char('\n'));
}

QStringList listFromText(const QTextEdit *edit)
{
    QString text = edit ? edit->toPlainText() : QString();
    text.replace(QLatin1Char(','), QLatin1Char('\n'));
    text.replace(QString::fromUtf8("，"), QStringLiteral("\n"));
    text.replace(QLatin1Char(';'), QLatin1Char('\n'));
    text.replace(QString::fromUtf8("；"), QStringLiteral("\n"));
    QStringList values;
    for (const QString &value : text.split(QLatin1Char('\n'))) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty())
            values.append(trimmed);
    }
    values.removeDuplicates();
    return values;
}
}

bool editCameraDialog(QWidget *parent, CameraSpec *camera, const QString &title)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    QVBoxLayout *outer = new QVBoxLayout(&dialog);
    QFormLayout *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);

    QLineEdit *model = new QLineEdit(camera->model);
    QLineEdit *manufacturer = new QLineEdit(camera->manufacturer);
    QSpinBox *resolutionX = dialogIntSpin(1, 200000, camera->resolutionX);
    QSpinBox *resolutionY = dialogIntSpin(1, 200000, camera->resolutionY);
    QDoubleSpinBox *pixelSize = dialogSpin(0.01, 1000.0, camera->pixelSizeUm, QStringLiteral(" um"));
    QLineEdit *sensorFormat = new QLineEdit(camera->sensorFormat);
    QComboBox *colorMode = editableCombo({QStringLiteral("Mono"), QStringLiteral("Color")}, camera->colorMode);
    QComboBox *shutterType = editableCombo({QStringLiteral("Global"), QStringLiteral("Rolling")}, camera->shutterType);
    QDoubleSpinBox *maxFps = dialogSpin(0.0, 100000.0, camera->maxFps, QStringLiteral(" fps"), 2);
    QComboBox *interfaceType = editableCombo({QStringLiteral("GigE"), QStringLiteral("USB3"), QStringLiteral("10GigE"), QStringLiteral("CameraLink"), QStringLiteral("CoaXPress")}, camera->interfaceType);
    QDoubleSpinBox *bandwidth = dialogSpin(0.0, 100000.0, camera->bandwidthMBps, QStringLiteral(" MB/s"), 2);
    QDoubleSpinBox *bitDepth = dialogSpin(1.0, 32.0, camera->bitDepth, QStringLiteral(" bit"), 1);
    QDoubleSpinBox *dynamicRange = dialogSpin(0.0, 200.0, camera->dynamicRangeDb, QStringLiteral(" dB"), 1);
    QComboBox *lensMount = editableCombo({QStringLiteral("C"), QStringLiteral("M12"), QStringLiteral("M42"), QStringLiteral("M58"), QStringLiteral("F")}, camera->lensMount);

    form->addRow(localizedText("型号", "Model"), model);
    form->addRow(localizedText("厂家", "Manufacturer"), manufacturer);
    form->addRow(localizedText("分辨率 X", "Resolution X"), resolutionX);
    form->addRow(localizedText("分辨率 Y", "Resolution Y"), resolutionY);
    form->addRow(localizedText("像元", "Pixel Size"), pixelSize);
    form->addRow(localizedText("靶面/传感器", "Format / Sensor"), sensorFormat);
    form->addRow(localizedText("颜色", "Color"), colorMode);
    form->addRow(localizedText("快门", "Shutter"), shutterType);
    form->addRow(QStringLiteral("fps"), maxFps);
    form->addRow(localizedText("接口", "Interface"), interfaceType);
    form->addRow(localizedText("带宽", "Bandwidth"), bandwidth);
    form->addRow(localizedText("位深", "Bit Depth"), bitDepth);
    form->addRow(localizedText("动态范围", "Dynamic Range"), dynamicRange);
    form->addRow(localizedText("镜头口", "Lens Mount"), lensMount);
    outer->addLayout(form);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    outer->addWidget(box);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    camera->model = model->text().trimmed();
    camera->manufacturer = manufacturer->text().trimmed();
    camera->resolutionX = resolutionX->value();
    camera->resolutionY = resolutionY->value();
    camera->pixelSizeUm = pixelSize->value();
    camera->sensorFormat = sensorFormat->text().trimmed();
    camera->colorMode = colorMode->currentText().trimmed();
    camera->shutterType = shutterType->currentText().trimmed();
    camera->maxFps = maxFps->value();
    camera->interfaceType = interfaceType->currentText().trimmed();
    camera->bandwidthMBps = bandwidth->value();
    camera->bitDepth = bitDepth->value();
    camera->dynamicRangeDb = dynamicRange->value();
    camera->lensMount = lensMount->currentText().trimmed();
    return true;
}

bool editLensDialog(QWidget *parent, LensSpec *lens, const QString &title)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    QVBoxLayout *outer = new QVBoxLayout(&dialog);
    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *content = new QWidget;
    QFormLayout *form = new QFormLayout(content);
    form->setLabelAlignment(Qt::AlignLeft);

    QLineEdit *model = new QLineEdit(lens->model);
    QLineEdit *manufacturer = new QLineEdit(lens->manufacturer);
    QComboBox *type = new QComboBox;
    type->addItems({QStringLiteral("FixedFocal"), QStringLiteral("ObjectTelecentric"), QStringLiteral("BiTelecentric")});
    type->setCurrentIndex(lens->lensType == LensType::FixedFocal ? 0 : lens->lensType == LensType::ObjectTelecentric ? 1 : 2);
    QComboBox *mount = editableCombo({QStringLiteral("C"), QStringLiteral("M12"), QStringLiteral("M42"), QStringLiteral("M58"), QStringLiteral("F")}, lens->lensMount);
    QDoubleSpinBox *focal = dialogSpin(0.0, 10000.0, lens->focalLengthMm, QStringLiteral(" mm"));
    QDoubleSpinBox *minWd = dialogSpin(0.0, 100000.0, lens->minWorkingDistanceMm, QStringLiteral(" mm"));
    QDoubleSpinBox *distortion = dialogSpin(0.0, 100.0, lens->distortionPercent, QStringLiteral(" %"));
    QDoubleSpinBox *imageCircle = dialogSpin(0.0, 1000.0, lens->imageCircleMm, QStringLiteral(" mm"));
    QDoubleSpinBox *mp = dialogSpin(0.0, 1000.0, lens->megapixelRating, QStringLiteral(" MP"), 2);
    QDoubleSpinBox *recommendedPixel = dialogSpin(0.0, 1000.0, lens->recommendedMinPixelUm, QStringLiteral(" um"));
    QDoubleSpinBox *pmag = dialogSpin(0.0, 1000.0, lens->pmag, QStringLiteral("x"));
    QDoubleSpinBox *nominalWd = dialogSpin(0.0, 100000.0, lens->nominalWorkingDistanceMm, QStringLiteral(" mm"));
    QDoubleSpinBox *wdTolerance = dialogSpin(0.0, 10000.0, lens->workingDistanceToleranceMm, QStringLiteral(" mm"));
    QDoubleSpinBox *maxSensor = dialogSpin(0.0, 1000.0, lens->maxSensorDiagonalMm, QStringLiteral(" mm"));
    QDoubleSpinBox *telecentricity = dialogSpin(0.0, 90.0, lens->telecentricityDeg, QStringLiteral(" deg"));
    QDoubleSpinBox *dof = dialogSpin(0.0, 100000.0, lens->dofMm, QStringLiteral(" mm"));
    QDoubleSpinBox *na = dialogSpin(0.0, 10.0, lens->numericalAperture, QString(), 4);
    QDoubleSpinBox *fNumber = dialogSpin(0.0, 1000.0, lens->fNumber, QStringLiteral(" F"), 2);
    QCheckBox *coaxial = new QCheckBox(localizedText("支持同轴照明", "Supports Coaxial Lighting"));
    coaxial->setChecked(lens->coaxialIllumination);
    QTextEdit *notes = new QTextEdit(lens->notes);
    notes->setMinimumHeight(72);

    form->addRow(localizedText("型号", "Model"), model);
    form->addRow(localizedText("厂家", "Manufacturer"), manufacturer);
    form->addRow(localizedText("类型", "Type"), type);
    form->addRow(localizedText("接口", "Mount"), mount);
    form->addRow(localizedText("焦距", "Focal Length"), focal);
    form->addRow(localizedText("最小 WD", "Minimum WD"), minWd);
    form->addRow(localizedText("畸变", "Distortion"), distortion);
    form->addRow(localizedText("像圈", "Image Circle"), imageCircle);
    form->addRow(QStringLiteral("MP"), mp);
    form->addRow(localizedText("推荐最小像元", "Recommended Min Pixel"), recommendedPixel);
    form->addRow(QStringLiteral("PMAG"), pmag);
    form->addRow(localizedText("标称 WD", "Nominal WD"), nominalWd);
    form->addRow(localizedText("WD 容差", "WD Tolerance"), wdTolerance);
    form->addRow(localizedText("最大靶面", "Max Sensor"), maxSensor);
    form->addRow(localizedText("远心度", "Telecentricity"), telecentricity);
    form->addRow(QStringLiteral("DOF"), dof);
    form->addRow(QStringLiteral("NA"), na);
    form->addRow(QStringLiteral("F/#"), fNumber);
    form->addRow(QString(), coaxial);
    form->addRow(localizedText("备注", "Notes"), notes);

    const auto setFieldVisible = [form](QWidget *field, bool visible) {
        if (!field)
            return;
        field->setVisible(visible);
        if (QWidget *label = form->labelForField(field))
            label->setVisible(visible);
    };
    const auto refreshLensFields = [=]() {
        const bool fixedLens = type->currentIndex() == 0;
        setFieldVisible(focal, fixedLens);
        setFieldVisible(minWd, fixedLens);
        setFieldVisible(pmag, !fixedLens);
        setFieldVisible(nominalWd, !fixedLens);
        setFieldVisible(wdTolerance, !fixedLens);
        setFieldVisible(maxSensor, !fixedLens);
        setFieldVisible(telecentricity, !fixedLens);
        setFieldVisible(dof, !fixedLens);
        setFieldVisible(coaxial, !fixedLens);
    };
    QObject::connect(type, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                     &dialog, refreshLensFields);
    refreshLensFields();

    scroll->setWidget(content);
    outer->addWidget(scroll);
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    outer->addWidget(box);
    dialog.resize(520, 720);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    lens->model = model->text().trimmed();
    lens->manufacturer = manufacturer->text().trimmed();
    lens->lensType = type->currentIndex() == 0 ? LensType::FixedFocal : type->currentIndex() == 1 ? LensType::ObjectTelecentric : LensType::BiTelecentric;
    lens->lensMount = mount->currentText().trimmed();
    const bool fixedLens = lens->lensType == LensType::FixedFocal;
    lens->focalLengthMm = fixedLens ? focal->value() : 0.0;
    lens->minWorkingDistanceMm = fixedLens ? minWd->value() : 0.0;
    lens->distortionPercent = distortion->value();
    lens->imageCircleMm = imageCircle->value();
    lens->megapixelRating = mp->value();
    lens->recommendedMinPixelUm = recommendedPixel->value();
    lens->pmag = fixedLens ? 0.0 : pmag->value();
    lens->nominalWorkingDistanceMm = fixedLens ? 0.0 : nominalWd->value();
    lens->workingDistanceToleranceMm = fixedLens ? 0.0 : wdTolerance->value();
    lens->maxSensorDiagonalMm = fixedLens ? 0.0 : maxSensor->value();
    lens->telecentricityDeg = fixedLens ? 0.0 : telecentricity->value();
    lens->dofMm = fixedLens ? 0.0 : dof->value();
    lens->numericalAperture = na->value();
    lens->fNumber = fNumber->value();
    lens->coaxialIllumination = fixedLens ? false : coaxial->isChecked();
    lens->notes = notes->toPlainText().trimmed();
    return true;
}

bool editLightDialog(QWidget *parent, LightSpec *light, const QString &title)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    QVBoxLayout *outer = new QVBoxLayout(&dialog);
    QFormLayout *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);

    QLineEdit *model = new QLineEdit(light->model);
    QLineEdit *manufacturer = new QLineEdit(light->manufacturer);
    QComboBox *type = new QComboBox;
    type->addItems({QStringLiteral("Backlight"), QStringLiteral("Ring"), QStringLiteral("Bar"),
                    QStringLiteral("Coaxial"), QStringLiteral("Dome"), QStringLiteral("TelecentricBacklight"),
                    QStringLiteral("DarkField")});
    const QString currentType = light->lightType == LightType::Backlight ? QStringLiteral("Backlight")
        : light->lightType == LightType::Ring ? QStringLiteral("Ring")
        : light->lightType == LightType::Bar ? QStringLiteral("Bar")
        : light->lightType == LightType::Coaxial ? QStringLiteral("Coaxial")
        : light->lightType == LightType::Dome ? QStringLiteral("Dome")
        : light->lightType == LightType::TelecentricBacklight ? QStringLiteral("TelecentricBacklight") : QStringLiteral("DarkField");
    type->setCurrentText(currentType);
    QComboBox *color = editableCombo({QStringLiteral("White"), QStringLiteral("Red"), QStringLiteral("Blue"),
                                      QStringLiteral("Green"), QStringLiteral("IR"), QStringLiteral("UV")}, light->color);
    QSpinBox *wavelength = dialogIntSpin(0, 100000, light->wavelengthNm, QStringLiteral(" nm"));
    QComboBox *mode = editableCombo({QStringLiteral("Continuous"), QStringLiteral("Strobe"), QStringLiteral("Trigger"), QStringLiteral("Pulse")}, light->mode);
    QDoubleSpinBox *activeWidth = dialogSpin(0.1, 100000.0, light->activeWidthMm, QStringLiteral(" mm"), 2);
    QDoubleSpinBox *activeHeight = dialogSpin(0.1, 100000.0, light->activeHeightMm, QStringLiteral(" mm"), 2);
    QTextEdit *bestFor = new QTextEdit(light->bestFor);
    bestFor->setMinimumHeight(88);

    form->addRow(localizedText("型号", "Model"), model);
    form->addRow(localizedText("厂家", "Manufacturer"), manufacturer);
    form->addRow(localizedText("类型", "Type"), type);
    form->addRow(localizedText("颜色", "Color"), color);
    form->addRow(localizedText("波长", "Wavelength"), wavelength);
    form->addRow(localizedText("模式", "Mode"), mode);
    form->addRow(localizedText("有效宽度", "Active Width"), activeWidth);
    form->addRow(localizedText("有效高度", "Active Height"), activeHeight);
    form->addRow(localizedText("适用场景", "Best For"), bestFor);
    outer->addLayout(form);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    outer->addWidget(box);
    dialog.resize(520, 520);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    light->model = model->text().trimmed();
    light->manufacturer = manufacturer->text().trimmed();
    light->lightType = lightTypeFromString(type->currentText().trimmed());
    light->color = color->currentText().trimmed();
    light->wavelengthNm = wavelength->value();
    light->mode = mode->currentText().trimmed();
    light->activeWidthMm = activeWidth->value();
    light->activeHeightMm = activeHeight->value();
    light->bestFor = bestFor->toPlainText().trimmed();
    return true;
}

bool editThreeDCameraDialog(QWidget *parent, ThreeDCameraSpec *camera, const QString &title)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    QVBoxLayout *outer = new QVBoxLayout(&dialog);
    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *content = new QWidget;
    QFormLayout *form = new QFormLayout(content);
    form->setLabelAlignment(Qt::AlignLeft);

    QLineEdit *manufacturer = new QLineEdit(camera->manufacturer);
    QLineEdit *series = new QLineEdit(camera->series);
    QLineEdit *model = new QLineEdit(camera->model);
    QComboBox *technology = editableCombo(threeDTechnologyLabels(),
        camera->technologyLabel.isEmpty() ? threeDTechnologyLabel(camera->technology) : camera->technologyLabel);
    QComboBox *status = editableCombo({QString::fromUtf8("在售"), QString::fromUtf8("停产"), localizedText("用户录入", "User entered")},
        camera->status.isEmpty() ? localizedText("用户录入", "User entered") : camera->status);
    QLineEdit *sourceUrl = new QLineEdit(camera->sourceUrl);
    QLineEdit *sourceDate = new QLineEdit(camera->sourceDate.isEmpty()
        ? QDate::currentDate().toString(Qt::ISODate) : camera->sourceDate);

    QDoubleSpinBox *referenceDistance = optionalThreeDSpin(100000.0, camera->referenceDistanceMm, QStringLiteral(" mm"), 3);
    QDoubleSpinBox *workingDistanceMin = optionalThreeDSpin(100000.0, camera->workingDistanceMinMm, QStringLiteral(" mm"), 3);
    QDoubleSpinBox *workingDistanceMax = optionalThreeDSpin(100000.0, camera->workingDistanceMaxMm, QStringLiteral(" mm"), 3);
    QDoubleSpinBox *zRange = optionalThreeDSpin(100000.0, camera->zMeasurementRangeMm, QStringLiteral(" mm"), 3);
    QDoubleSpinBox *xNear = optionalThreeDSpin(100000.0, camera->xFovNearMm, QStringLiteral(" mm"), 3);
    QDoubleSpinBox *xReference = optionalThreeDSpin(100000.0, camera->xFovReferenceMm, QStringLiteral(" mm"), 3);
    QDoubleSpinBox *xFar = optionalThreeDSpin(100000.0, camera->xFovFarMm, QStringLiteral(" mm"), 3);
    QDoubleSpinBox *yNear = optionalThreeDSpin(100000.0, camera->yFovNearMm, QStringLiteral(" mm"), 3);
    QDoubleSpinBox *yReference = optionalThreeDSpin(100000.0, camera->yFovReferenceMm, QStringLiteral(" mm"), 3);
    QDoubleSpinBox *yFar = optionalThreeDSpin(100000.0, camera->yFovFarMm, QStringLiteral(" mm"), 3);

    QDoubleSpinBox *zRepeatability = optionalThreeDSpin(100000.0, camera->zRepeatabilityUm, QStringLiteral(" um"), 3);
    QDoubleSpinBox *xRepeatability = optionalThreeDSpin(100000.0, camera->xRepeatabilityUm, QStringLiteral(" um"), 3);
    QDoubleSpinBox *zResolution = optionalThreeDSpin(100000.0, camera->zResolutionUm, QStringLiteral(" um"), 3);
    QDoubleSpinBox *zLinearity = optionalThreeDSpin(100.0, camera->zLinearityPercentOfRange, QStringLiteral(" % F.S."), 4);
    QDoubleSpinBox *measurementAccuracy = optionalThreeDSpin(100000.0, camera->measurementAccuracyUm, QStringLiteral(" um"), 3);
    QDoubleSpinBox *xyResolution = optionalThreeDSpin(100000.0, camera->xyResolutionUm, QStringLiteral(" um"), 3);
    QDoubleSpinBox *profileDataInterval = optionalThreeDSpin(1000000.0, camera->profileDataIntervalUm, QStringLiteral(" um"), 3);
    QTextEdit *accuracyCondition = new QTextEdit(camera->accuracyCondition);
    accuracyCondition->setMinimumHeight(58);

    QSpinBox *profilePoints = optionalThreeDInt(100000000, camera->profilePoints);
    QDoubleSpinBox *scanRateMin = optionalThreeDSpin(10000000.0, camera->scanRateMinHz, QStringLiteral(" Hz"), 1);
    QDoubleSpinBox *scanRateMax = optionalThreeDSpin(10000000.0, camera->scanRateMaxHz, QStringLiteral(" Hz"), 1);
    QDoubleSpinBox *acquisitionTime = optionalThreeDSpin(100000.0, camera->acquisitionTimeMs, QStringLiteral(" ms"), 3);
    QDoubleSpinBox *frameRate = optionalThreeDSpin(10000000.0, camera->frameRateHz, QStringLiteral(" fps"), 2);
    QDoubleSpinBox *encoderRateMax = optionalThreeDSpin(10000000.0, camera->encoderRateMaxHz, QStringLiteral(" Hz"), 1);
    QDoubleSpinBox *exposureMin = optionalThreeDSpin(10000000.0, camera->exposureTimeMinUs, QStringLiteral(" us"), 2);
    QDoubleSpinBox *exposureMax = optionalThreeDSpin(10000000.0, camera->exposureTimeMaxUs, QStringLiteral(" us"), 2);
    QDoubleSpinBox *readoutTime = optionalThreeDSpin(10000000.0, camera->readoutTimeUs, QStringLiteral(" us"), 2);
    QComboBox *requiresMotion = tristateCombo(camera->requiresExternalMotion);
    QComboBox *supportsEncoder = tristateCombo(camera->supportsEncoder);
    QComboBox *supportsTrigger = tristateCombo(camera->supportsExternalTrigger);

    QLineEdit *lightSource = new QLineEdit(camera->lightSource);
    QDoubleSpinBox *wavelength = optionalThreeDSpin(100000.0, camera->wavelengthNm, QStringLiteral(" nm"), 1);
    QLineEdit *laserClass = new QLineEdit(camera->laserClass);
    QLineEdit *structure = new QLineEdit(camera->structure);
    QLineEdit *ipRating = new QLineEdit(camera->ipRating);
    QDoubleSpinBox *weight = optionalThreeDSpin(1000000.0, camera->weightG, QStringLiteral(" g"), 1);
    QLineEdit *dimensions = new QLineEdit(camera->dimensions);
    QLineEdit *temperature = new QLineEdit(camera->temperature);
    QLineEdit *power = new QLineEdit(camera->power);
    QTextEdit *interfaces = new QTextEdit(listText(camera->interfaces));
    QTextEdit *outputs = new QTextEdit(listText(camera->outputTypes));
    QTextEdit *materials = new QTextEdit(listText(camera->materialScenarios));
    QTextEdit *notes = new QTextEdit(listText(camera->notes));
    for (QTextEdit *edit : {interfaces, outputs, materials, notes}) {
        edit->setMinimumHeight(64);
    }

    addDialogSection(form, localizedText("基础信息", "Basic Information"));
    form->addRow(localizedText("品牌", "Manufacturer"), manufacturer);
    form->addRow(localizedText("系列", "Series"), series);
    form->addRow(localizedText("型号", "Model"), model);
    form->addRow(localizedText("技术路线", "Technology"), technology);
    form->addRow(localizedText("状态", "Status"), status);
    form->addRow(localizedText("资料来源 URL", "Source URL"), sourceUrl);
    form->addRow(localizedText("采集日期", "Source Date"), sourceDate);

    addDialogSection(form, localizedText("几何范围", "Geometry"));
    form->addRow(localizedText("参考距离", "Reference Distance"), referenceDistance);
    form->addRow(localizedText("工作距离最小值", "Working Distance Min"), workingDistanceMin);
    form->addRow(localizedText("工作距离最大值", "Working Distance Max"), workingDistanceMax);
    form->addRow(localizedText("Z 量程", "Z Range"), zRange);
    form->addRow(localizedText("X 视场近端", "X FOV Near"), xNear);
    form->addRow(localizedText("X 视场参考", "X FOV Reference"), xReference);
    form->addRow(localizedText("X 视场远端", "X FOV Far"), xFar);
    form->addRow(localizedText("Y 视场近端", "Y FOV Near"), yNear);
    form->addRow(localizedText("Y 视场参考", "Y FOV Reference"), yReference);
    form->addRow(localizedText("Y 视场远端", "Y FOV Far"), yFar);

    addDialogSection(form, localizedText("精度质量", "Accuracy / Quality"));
    form->addRow(localizedText("Z 重复精度", "Z Repeatability"), zRepeatability);
    form->addRow(localizedText("X 重复精度", "X Repeatability"), xRepeatability);
    form->addRow(localizedText("Z 分辨率", "Z Resolution"), zResolution);
    form->addRow(localizedText("Z 线性度", "Z Linearity"), zLinearity);
    form->addRow(localizedText("测量精度", "Measurement Accuracy"), measurementAccuracy);
    form->addRow(localizedText("XY 分辨率", "XY Resolution"), xyResolution);
    form->addRow(localizedText("X 轮廓数据间隔", "X Profile Data Interval"), profileDataInterval);
    form->addRow(localizedText("测试条件", "Test Conditions"), accuracyCondition);

    addDialogSection(form, localizedText("速度、触发与编码器", "Speed, Trigger and Encoder"));
    form->addRow(localizedText("点数/轮廓", "Points per Profile"), profilePoints);
    form->addRow(localizedText("最小扫描频率", "Min Scan Rate"), scanRateMin);
    form->addRow(localizedText("最大扫描频率", "Max Scan Rate"), scanRateMax);
    form->addRow(localizedText("采集时间", "Acquisition Time"), acquisitionTime);
    form->addRow(localizedText("帧率", "Frame Rate"), frameRate);
    form->addRow(localizedText("编码器输入上限", "Encoder Input Limit"), encoderRateMax);
    form->addRow(localizedText("最小曝光", "Min Exposure"), exposureMin);
    form->addRow(localizedText("最大曝光", "Max Exposure"), exposureMax);
    form->addRow(localizedText("读出/复位时间", "Readout / Reset Time"), readoutTime);
    form->addRow(localizedText("需要外部运动", "Requires External Motion"), requiresMotion);
    form->addRow(localizedText("支持编码器", "Supports Encoder"), supportsEncoder);
    form->addRow(localizedText("支持外部触发", "Supports External Trigger"), supportsTrigger);

    addDialogSection(form, localizedText("光学、结构与环境", "Optics, Structure and Environment"));
    form->addRow(localizedText("光源", "Light Source"), lightSource);
    form->addRow(localizedText("波长", "Wavelength"), wavelength);
    form->addRow(localizedText("激光等级", "Laser Class"), laserClass);
    form->addRow(localizedText("结构", "Structure"), structure);
    form->addRow(QStringLiteral("IP"), ipRating);
    form->addRow(localizedText("重量", "Weight"), weight);
    form->addRow(localizedText("尺寸", "Dimensions"), dimensions);
    form->addRow(localizedText("温度", "Temperature"), temperature);
    form->addRow(localizedText("供电", "Power"), power);
    form->addRow(localizedText("接口列表", "Interfaces"), interfaces);
    form->addRow(localizedText("输出类型", "Output Types"), outputs);
    form->addRow(localizedText("材质场景", "Material Scenarios"), materials);
    form->addRow(localizedText("备注", "Notes"), notes);

    scroll->setWidget(content);
    outer->addWidget(scroll);
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    outer->addWidget(box);
    dialog.resize(640, 760);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    camera->manufacturer = manufacturer->text().trimmed();
    camera->series = series->text().trimmed();
    camera->model = model->text().trimmed();
    camera->technologyLabel = technology->currentText().trimmed();
    camera->technology = threeDTechnologyFromLabel(camera->technologyLabel);
    camera->status = status->currentText().trimmed();
    camera->sourceUrl = sourceUrl->text().trimmed();
    camera->sourceDate = sourceDate->text().trimmed();
    camera->referenceDistanceMm = optionalThreeDValue(referenceDistance);
    camera->workingDistanceMinMm = optionalThreeDValue(workingDistanceMin);
    camera->workingDistanceMaxMm = optionalThreeDValue(workingDistanceMax);
    camera->zMeasurementRangeMm = optionalThreeDValue(zRange);
    camera->xFovNearMm = optionalThreeDValue(xNear);
    camera->xFovReferenceMm = optionalThreeDValue(xReference);
    camera->xFovFarMm = optionalThreeDValue(xFar);
    camera->yFovNearMm = optionalThreeDValue(yNear);
    camera->yFovReferenceMm = optionalThreeDValue(yReference);
    camera->yFovFarMm = optionalThreeDValue(yFar);
    camera->zRepeatabilityUm = optionalThreeDValue(zRepeatability);
    camera->xRepeatabilityUm = optionalThreeDValue(xRepeatability);
    camera->zResolutionUm = optionalThreeDValue(zResolution);
    camera->zLinearityPercentOfRange = optionalThreeDValue(zLinearity);
    camera->measurementAccuracyUm = optionalThreeDValue(measurementAccuracy);
    camera->xyResolutionUm = optionalThreeDValue(xyResolution);
    camera->profileDataIntervalUm = optionalThreeDValue(profileDataInterval);
    camera->accuracyCondition = accuracyCondition->toPlainText().trimmed();
    camera->profilePoints = optionalThreeDIntValue(profilePoints);
    camera->scanRateMinHz = optionalThreeDValue(scanRateMin);
    camera->scanRateMaxHz = optionalThreeDValue(scanRateMax);
    camera->acquisitionTimeMs = optionalThreeDValue(acquisitionTime);
    camera->frameRateHz = optionalThreeDValue(frameRate);
    camera->encoderRateMaxHz = optionalThreeDValue(encoderRateMax);
    camera->exposureTimeMinUs = optionalThreeDValue(exposureMin);
    camera->exposureTimeMaxUs = optionalThreeDValue(exposureMax);
    camera->readoutTimeUs = optionalThreeDValue(readoutTime);
    camera->requiresExternalMotion = tristateComboValue(requiresMotion);
    camera->supportsEncoder = tristateComboValue(supportsEncoder);
    camera->supportsExternalTrigger = tristateComboValue(supportsTrigger);
    camera->lightSource = lightSource->text().trimmed();
    camera->wavelengthNm = optionalThreeDValue(wavelength);
    camera->laserClass = laserClass->text().trimmed();
    camera->structure = structure->text().trimmed();
    camera->ipRating = ipRating->text().trimmed();
    camera->weightG = optionalThreeDValue(weight);
    camera->dimensions = dimensions->text().trimmed();
    camera->temperature = temperature->text().trimmed();
    camera->power = power->text().trimmed();
    camera->interfaces = listFromText(interfaces);
    camera->outputTypes = listFromText(outputs);
    camera->materialScenarios = listFromText(materials);
    camera->notes = listFromText(notes);
    camera->userDefined = true;
    return true;
}
