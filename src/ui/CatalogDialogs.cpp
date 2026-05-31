#include "ui/CatalogDialogs.h"

#include "ui/UiHelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QLineEdit>
#include <QObject>
#include <QScrollArea>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

using namespace UiHelpers;

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
    lens->focalLengthMm = focal->value();
    lens->minWorkingDistanceMm = minWd->value();
    lens->distortionPercent = distortion->value();
    lens->imageCircleMm = imageCircle->value();
    lens->megapixelRating = mp->value();
    lens->recommendedMinPixelUm = recommendedPixel->value();
    lens->pmag = pmag->value();
    lens->nominalWorkingDistanceMm = nominalWd->value();
    lens->workingDistanceToleranceMm = wdTolerance->value();
    lens->maxSensorDiagonalMm = maxSensor->value();
    lens->telecentricityDeg = telecentricity->value();
    lens->dofMm = dof->value();
    lens->numericalAperture = na->value();
    lens->fNumber = fNumber->value();
    lens->coaxialIllumination = coaxial->isChecked();
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
