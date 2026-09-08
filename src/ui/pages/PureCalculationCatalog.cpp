#include "ui/pages/PureCalculationPage.h"
#include "core/PixelFormat.h"
#include "ui/ParameterCatalogDialog.h"
#include "ui/ParameterUi.h"

#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>

using namespace UiHelpers;
using namespace ParameterUi;

void PureCalculationPage::setCatalog(const CatalogRepository *catalog)
{
    m_catalog = catalog;
    m_importCamera->setEnabled(catalog != nullptr);
    m_importLens->setEnabled(catalog != nullptr);
    m_matchLens->setEnabled(catalog != nullptr);
}
void PureCalculationPage::importCamera(const CameraSpec &camera)
{
    const QString previousSignature = cameraSignature();
    m_loading = true;
    const auto known = [](double value) { return value > 0.0 ? Number(value) : Number(); };
    setNumber("camera.nx", known(camera.resolutionX)); setNumber("camera.ny", known(camera.resolutionY)); setNumber("camera.pixel", known(camera.pixelSizeUm));
    setNumber("check.maxFps", known(camera.maxFps)); setNumber("check.capacity", known(camera.bandwidthMBps));
    m_cameraMount = camera.lensMount;
    m_texts.value("check.cameraMount")->setText(camera.lensMount);
    // 目录中的 Mono / Color 不是传输格式，不凭位深推测打包方式。
    const QString format = formats().contains(camera.colorMode) ? camera.colorMode : QString();
    setChoice("check.format", format);
    if (task() == QLatin1String("data")) {
        setChoice("data.format", format);
        setNumber("data.capacity", known(camera.bandwidthMBps));
    }
    setSource("camera", "catalog", productLabel(camera.manufacturer, camera.model));
    if (cameraSignature() != previousSignature) invalidateMeasuredFov();
    m_loading = false;
    refresh();
}
void PureCalculationPage::importLens(const LensSpec &lens)
{
    m_loading = true;
    const auto known = [](double value) { return value > 0.0 ? Number(value) : Number(); };
    QString destination = task();
    if (destination == QLatin1String("optics") && lens.isTelecentric()) destination = "tele";
    if (destination == QLatin1String("tele") && !lens.isTelecentric()) destination = "optics";
    if (destination == QLatin1String("check")) {
        setChoice("check.lens", lens.isTelecentric() ? "tele" : "fixed");
        setChoice("check.geometry", "estimate");
        setNumber("check.focal", known(lens.focalLengthMm)); setNumber("check.mag", known(lens.pmag));
        setNumber("check.imageCircle", known(lens.imageCircleMm)); setNumber("check.minDistance", known(lens.minWorkingDistanceMm));
        setNumber("check.nominalDistance", known(lens.nominalWorkingDistanceMm)); setNumber("check.distanceTolerance", known(lens.workingDistanceToleranceMm));
        setNumber("check.dof", known(lens.dofMm)); setNumber("check.telecentricity", lens.hasTelecentricity() ? Number(lens.telecentricityDeg) : Number());
        m_texts.value("check.lensMount")->setText(lens.lensMount);
        m_flags.value("check.dofConfirmed")->setChecked(false);
    } else if (destination == QLatin1String("tele")) {
        setNumber("tele.mag", known(lens.pmag)); setChoice("tele.solve", "fov");
    } else {
        const auto previous = Parameters::optics(opticsInput());
        if (!number("optics.distance")) setNumber("optics.distance", previous.distanceMm);
        setNumber("optics.focal", known(lens.focalLengthMm)); setChoice("optics.solve", "fov");
    }
    setSource(destination, "catalog", productLabel(lens.manufacturer, lens.model));
    m_loading = false;
    setTask(destination);
    refresh();
}
void PureCalculationPage::chooseCamera()
{
    if (m_catalog) if (const auto camera = ParameterCatalogDialog::camera(this, *m_catalog)) importCamera(*camera);
}
SelectionRequest PureCalculationPage::candidateRequest() const
{
    SelectionRequest request;
    request.placementMarginMm = 0.0;
    request.objectWidthMm = request.objectHeightMm = request.workingDistanceMm = 0.0;
    if (task() == QLatin1String("optics")) {
        const auto input = opticsInput(); const auto result = Parameters::optics(input);
        request.objectWidthMm = input.targetFovWidthMm.value_or(0.0); request.objectHeightMm = input.targetFovHeightMm.value_or(0.0);
        request.workingDistanceMm = result.distanceMm.value_or(0.0); request.allowTelecentric = false;
    } else if (task() == QLatin1String("tele")) {
        request.objectWidthMm = number("tele.width").value_or(0.0); request.objectHeightMm = number("tele.height").value_or(0.0);
        request.allowTelecentric = true;
    } else if (task() == QLatin1String("check")) {
        request.objectWidthMm = number("check.width").value_or(0.0); request.objectHeightMm = number("check.height").value_or(0.0);
        request.workingDistanceMm = number("check.distance").value_or(0.0); request.allowTelecentric = choice("check.lens") == QLatin1String("tele");
    }
    return request;
}
void PureCalculationPage::chooseLens(bool matched)
{
    if (!m_catalog) return;
    Parameters::SystemInput context;
    context.sensor = sensor(); context.cameraMount = m_cameraMount;
    if (task() == QLatin1String("check")) context = systemInput();
    else if (task() == QLatin1String("optics")) {
        const auto input = opticsInput(); const auto result = Parameters::optics(input);
        context.model = input.model; context.distanceMm = result.distanceMm;
        context.targetFovWidthMm = input.targetFovWidthMm; context.targetFovHeightMm = input.targetFovHeightMm;
        context.targetObjectPixelUm = number("optics.targetPixel");
    } else {
        context.telecentric = true;
        context.targetFovWidthMm = number("tele.width"); context.targetFovHeightMm = number("tele.height"); context.targetObjectPixelUm = number("tele.targetPixel");
    }
    if (const auto lens = ParameterCatalogDialog::lens(this, *m_catalog, context, matched)) importLens(*lens);
}
