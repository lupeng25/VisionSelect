#include "catalog/CatalogRepository.h"
#include "i18n/LanguageManager.h"
#include "license/LicenseIssuer.h"
#include "license/LicenseManager.h"
#include "report/PdfReportWriter.h"
#include "selection/CalculationAssistant.h"
#include "selection/SelectionEngine.h"
#include "three_d/ThreeDCameraMatcher.h"
#include "three_d/ThreeDCameraRepository.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtTest/QtTest>

class SelectionEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void defaultCatalogManufacturersAreLoaded();
    void calculationAssistantEstimatesRequirements();
    void lensAssistantEstimatesLenses();
    void pureCalculationFixedLens();
    void pureCalculationTelecentric();
    void catalogPersistenceRoundTrip();
    void motionExposureAndStrobePreference();
    void dataThroughputAndInterfaceRisk();
    void hardConstraintsPreferCompatibleResults();
    void hardConstraintFallbackKeepsDiagnosticResult();
    void fixedLensDofAndDistortionRisk();
    void lightCoverageAffectsScore();
    void telecentricMeasurementWins();
    void largeFovPrefersFixedFocal();
    void motionPrefersGlobalShutter();
    void reflectiveSurfaceGetsCoaxialOrDome();
    void chineseTextIsUnicode();
    void threeDCameraCatalogLoadsFromResource();
    void threeDCameraMatcherClassifiesRequirements();
    void threeDCameraDataDoesNotAffect2DSelection();
    void licenseValidationCoversSignatureMachineAndExpiry();
    void licenseIssuerParsesXmlAndSignsCompatibleKey();
    void licenseIssuerRejectsInvalidInput();
    void machineCodeGenerationIsStable();
    void languageManagerSwitchesAvailableLanguages();
    void invalidTelecentricCsvFails();
    void invalidLightCsvFails();
    void pdfReportWrites();

private:
    QTemporaryDir m_catalogStorage;
    CatalogRepository m_catalog;
};

namespace {
const char *kTestPrivateKeyXml =
    "<RSAKeyValue><Modulus>5hzYFnHq3/1l3dpJFHV8XBnUejhF6oIE5lVzrDcKm1rq5bLIOTKmRgJmjaa9had4v8w1W3jIX1E/OU5y50KE2YDqHJvAPkiOT7Zpka5U7+pypzLEH5zQfyeaKKgQsXxgoGq3z6DtKv/1mfz5xq0jv5Nr4Ouv/Xep5LNuk8eG7nE=</Modulus><Exponent>AQAB</Exponent><P>9Ml93yaFiAybA4/iVgWhudDbZRLiE9tO042H97yuw5oXeEBF4KNFrsPXT5hjAutCaiMpxXeFbDZlwtleEiLm6w==</P><Q>8KdFsZm6HVMz2nkuB1UHAjhoVwlfQeUvJbZbVjsSTF9Rh5+BeloVC+U/K9C8/C0o1odg5fa3IBxl6gueUaKhEw==</Q><DP>4dmiVCijnWIcCA5SQxIRJHNqaXghtTZsJU55O/8PtBNRQjbzAg9CtLumxZ6RA9lyPqFQ4gujw7Lw8vVBETS4nw==</DP><DQ>0AYYJaSYED9a5HC5zBbA3zd5Yjs0v5ZoQfY3T/vyHliK9mx4FRaHeOfqymo+4tH6qi8OINs6gyRpKH5wlWq6Rw==</DQ><InverseQ>xbFUUJeQ4X+QFiIQCq/YmJcCgbj5Qz/+5gbH1sz1Food2SipieeYQojiMERCAmD8VgSOKVXdKHmzEjeikvBPbA==</InverseQ><D>g3HejZOtEx3wXnYeYK1ryECI+vfCGF8E5X3SgYE/cdbRbzxc2y9vg3ZDlo60m/A6LXU81W99JdWHQ/jn8eoxb+fDvXVnHdGg8sCm/7d9/8MnOEXDRllZbxNE/ICm1k9V9nX1yWQJxPKQ7l3Ify3UEurivZ4e8VB9hDITzoKRKzE=</D></RSAKeyValue>";
}

void SelectionEngineTest::initTestCase()
{
    QVERIFY(m_catalogStorage.isValid());
    m_catalog.setStorageDirectory(m_catalogStorage.path());
    QString error;
    QVERIFY2(m_catalog.loadDefaults(&error), qPrintable(error));
    QVERIFY(!m_catalog.cameras().isEmpty());
    QVERIFY(!m_catalog.lenses().isEmpty());
    QVERIFY(!m_catalog.lights().isEmpty());
}

void SelectionEngineTest::defaultCatalogManufacturersAreLoaded()
{
    for (const CameraSpec &camera : m_catalog.cameras())
        QVERIFY2(!camera.manufacturer.trimmed().isEmpty(), qPrintable(camera.model));
    for (const LensSpec &lens : m_catalog.lenses())
        QVERIFY2(!lens.manufacturer.trimmed().isEmpty(), qPrintable(lens.model));
    for (const LightSpec &light : m_catalog.lights())
        QVERIFY2(!light.manufacturer.trimmed().isEmpty(), qPrintable(light.model));
}

void SelectionEngineTest::calculationAssistantEstimatesRequirements()
{
    SelectionRequest request;
    request.objectWidthMm = 20.0;
    request.objectHeightMm = 10.0;
    request.placementMarginMm = 2.0;
    request.minFeatureUm = 50.0;
    request.measurementToleranceUm = 10.0;
    request.workingDistanceMm = 110.0;
    request.requiredFps = 20.0;
    request.detectionType = DetectionType::Measurement;

    const RequirementEstimate requirement = CalculationAssistant::estimateRequirement(request);
    QCOMPARE(requirement.requiredFovWidthMm, 24.0);
    QCOMPARE(requirement.requiredFovHeightMm, 14.0);
    QVERIFY(requirement.requiredResolutionX > 0);
    QVERIFY(requirement.requiredResolutionY > 0);
    QVERIFY(requirement.requiredMegapixels > 0.0);
    QVERIFY(requirement.telecentricPreferred);

    const QVector<CameraCalculationEstimate> estimates = CalculationAssistant::estimateCameras(request, m_catalog.cameras(), 5);
    QVERIFY(!estimates.isEmpty());
    QVERIFY(estimates.first().fixedFocalLengthMm > 0.0);
    QVERIFY(estimates.first().sensorDiagonalMm > 0.0);
}

void SelectionEngineTest::lensAssistantEstimatesLenses()
{
    SelectionRequest request;
    request.objectWidthMm = 20.0;
    request.objectHeightMm = 20.0;
    request.placementMarginMm = 2.0;
    request.minFeatureUm = 50.0;
    request.measurementToleranceUm = 10.0;
    request.workingDistanceMm = 110.0;
    request.heightVariationMm = 2.0;
    request.detectionType = DetectionType::Measurement;

    const QVector<CameraCalculationEstimate> cameras = CalculationAssistant::estimateCameras(request, m_catalog.cameras(), 1);
    QVERIFY(!cameras.isEmpty());

    const QVector<LensCalculationEstimate> lenses = CalculationAssistant::estimateLenses(request, cameras.first().camera, m_catalog.lenses(), 0);
    QVERIFY(!lenses.isEmpty());
    QVERIFY2(lenses.first().fovOk, qPrintable(lenses.first().lens.model));
    QVERIFY2(lenses.first().samplingOk, qPrintable(lenses.first().lens.model));
    QVERIFY2(lenses.first().mountOk, qPrintable(lenses.first().lens.model));

    bool hasFixed = false;
    bool hasTelecentric = false;
    for (const LensCalculationEstimate &estimate : lenses) {
        if (estimate.lens.isTelecentric()) {
            hasTelecentric = true;
            QVERIFY(estimate.magnification > 0.0);
            QVERIFY(estimate.effectiveFovWidthMm > 0.0);
        } else {
            hasFixed = true;
            QVERIFY(estimate.estimatedFocalLengthMm > 0.0);
            QVERIFY(estimate.effectiveFovWidthMm > 0.0);
        }
    }
    QVERIFY(hasFixed);
    QVERIFY(hasTelecentric);
}

void SelectionEngineTest::pureCalculationFixedLens()
{
    PureCalculationInput input;
    input.request.objectWidthMm = 20.0;
    input.request.objectHeightMm = 20.0;
    input.request.placementMarginMm = 2.0;
    input.request.minFeatureUm = 50.0;
    input.request.measurementToleranceUm = 10.0;
    input.request.workingDistanceMm = 110.0;
    input.request.heightVariationMm = 2.0;
    input.request.motionSpeedMmS = 500.0;
    input.request.requiredFps = 120.0;
    input.request.detectionType = DetectionType::Measurement;
    input.request.surfaceType = SurfaceType::ReflectiveMetal;
    input.request.reflective = true;

    input.camera.resolutionX = 2448;
    input.camera.resolutionY = 2048;
    input.camera.pixelSizeUm = 3.45;
    input.camera.bitDepth = 12.0;
    input.camera.bandwidthMBps = 120.0;
    input.camera.shutterType = QStringLiteral("Rolling");

    input.lens.lensType = LensType::FixedFocal;
    input.lens.focalLengthMm = 25.0;
    input.lens.minWorkingDistanceMm = 100.0;
    input.lens.fNumber = 4.0;
    input.lens.distortionPercent = 0.2;
    input.lens.imageCircleMm = 11.0;
    input.lens.megapixelRating = 5.0;

    input.light.lightType = LightType::Ring;
    input.light.mode = QStringLiteral("Continuous");
    input.light.activeWidthMm = 20.0;
    input.light.activeHeightMm = 20.0;
    input.telecentricMode = false;

    const PureCalculationResult result = CalculationAssistant::estimatePure(input);
    QCOMPARE(result.requirement.requiredFovWidthMm, 24.0);
    QCOMPARE(result.requirement.requiredFovHeightMm, 24.0);
    QVERIFY(result.requirement.maxExposureUsForOnePixelBlur > 0.0);
    QVERIFY(result.targetFixedFocalLengthMm > 0.0);
    QVERIFY(result.effectiveFovWidthMm > 0.0);
    QVERIFY(result.lensObjectPixelSizeUm > 0.0);
    QVERIFY(result.estimatedDofMm > 0.0);
    QVERIFY(result.distortionErrorUm > 0.0);
    QVERIFY(result.bandwidthRequiredMBps > result.interfaceCapacityMBps);
    QVERIFY(result.storagePerHourGB > 0.0);
    QVERIFY(!result.risks.isEmpty());
}

void SelectionEngineTest::pureCalculationTelecentric()
{
    PureCalculationInput input;
    input.request.objectWidthMm = 12.0;
    input.request.objectHeightMm = 8.0;
    input.request.placementMarginMm = 1.0;
    input.request.minFeatureUm = 20.0;
    input.request.measurementToleranceUm = 5.0;
    input.request.workingDistanceMm = 110.0;
    input.request.heightVariationMm = 3.0;
    input.request.requiredFps = 30.0;
    input.request.detectionType = DetectionType::Measurement;
    input.request.surfaceType = SurfaceType::Matte;
    input.request.reflective = false;

    input.camera.resolutionX = 2448;
    input.camera.resolutionY = 2048;
    input.camera.pixelSizeUm = 3.45;
    input.camera.bitDepth = 12.0;
    input.camera.bandwidthMBps = 380.0;
    input.camera.shutterType = QStringLiteral("Global");

    input.lens.lensType = LensType::ObjectTelecentric;
    input.lens.pmag = 0.3;
    input.lens.nominalWorkingDistanceMm = 110.0;
    input.lens.workingDistanceToleranceMm = 5.0;
    input.lens.dofMm = 2.0;
    input.lens.telecentricityDeg = 0.2;
    input.lens.distortionPercent = 0.02;
    input.lens.imageCircleMm = 11.0;
    input.lens.megapixelRating = 12.0;

    input.light.lightType = LightType::TelecentricBacklight;
    input.light.mode = QStringLiteral("Strobe");
    input.light.activeWidthMm = 60.0;
    input.light.activeHeightMm = 60.0;
    input.telecentricMode = true;

    const PureCalculationResult result = CalculationAssistant::estimatePure(input);
    QVERIFY(result.effectiveFovWidthMm > 0.0);
    QVERIFY(result.effectiveFovHeightMm > 0.0);
    QVERIFY(result.lensObjectPixelSizeUm > 0.0);
    QCOMPARE(result.magnification, 0.3);
    QVERIFY(result.residualTelecentricErrorUm > input.request.measurementToleranceUm);
    QVERIFY(result.lightCoverageMarginPercent >= 10.0);
    QVERIFY(!result.risks.isEmpty());
}

void SelectionEngineTest::catalogPersistenceRoundTrip()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());

    CatalogRepository repo;
    repo.setStorageDirectory(storage.path());
    QString error;
    QVERIFY2(repo.loadDefaults(&error), qPrintable(error));
    const int originalCameraCount = repo.cameras().size();
    const int originalLensCount = repo.lenses().size();
    const int originalLightCount = repo.lights().size();

    CameraSpec camera;
    camera.model = QStringLiteral("TEST-CAM-001");
    camera.manufacturer = QStringLiteral("TestMaker");
    camera.resolutionX = 1280;
    camera.resolutionY = 1024;
    camera.pixelSizeUm = 4.8;
    camera.sensorFormat = QStringLiteral("1/2\"");
    camera.colorMode = QStringLiteral("Mono");
    camera.shutterType = QStringLiteral("Global");
    camera.maxFps = 60.0;
    camera.interfaceType = QStringLiteral("USB3");
    camera.bandwidthMBps = 380.0;
    camera.bitDepth = 12.0;
    camera.dynamicRangeDb = 60.0;
    camera.lensMount = QStringLiteral("C");
    QVERIFY2(repo.addCamera(camera, &error), qPrintable(error));

    LensSpec lens;
    lens.model = QStringLiteral("TEST-LENS-25");
    lens.manufacturer = QStringLiteral("TestMaker");
    lens.lensType = LensType::FixedFocal;
    lens.lensMount = QStringLiteral("C");
    lens.focalLengthMm = 25.0;
    lens.minWorkingDistanceMm = 100.0;
    lens.distortionPercent = 0.05;
    lens.imageCircleMm = 11.0;
    lens.megapixelRating = 5.0;
    lens.recommendedMinPixelUm = 3.45;
    lens.fNumber = 2.8;
    QVERIFY2(repo.addLens(lens, &error), qPrintable(error));

    LightSpec light;
    light.model = QStringLiteral("TEST-LIGHT-100");
    light.manufacturer = QStringLiteral("TestMaker");
    light.lightType = LightType::Ring;
    light.color = QStringLiteral("White");
    light.wavelengthNm = 0;
    light.mode = QStringLiteral("Strobe");
    light.activeWidthMm = 100.0;
    light.activeHeightMm = 100.0;
    light.bestFor = QStringLiteral("Test light");
    QVERIFY2(repo.addLight(light, &error), qPrintable(error));

    CatalogRepository reloaded;
    reloaded.setStorageDirectory(storage.path());
    QVERIFY2(reloaded.loadDefaults(&error), qPrintable(error));
    QCOMPARE(reloaded.cameras().size(), originalCameraCount + 1);
    QCOMPARE(reloaded.lenses().size(), originalLensCount + 1);
    QCOMPARE(reloaded.lights().size(), originalLightCount + 1);
    QCOMPARE(reloaded.cameras().last().model, QStringLiteral("TEST-CAM-001"));
    QCOMPARE(reloaded.lenses().last().model, QStringLiteral("TEST-LENS-25"));
    QCOMPARE(reloaded.lights().last().model, QStringLiteral("TEST-LIGHT-100"));

    CameraSpec editedCamera = reloaded.cameras().last();
    editedCamera.model = QStringLiteral("TEST-CAM-EDITED");
    QVERIFY2(reloaded.updateCamera(reloaded.cameras().size() - 1, editedCamera, &error), qPrintable(error));
    QVERIFY2(reloaded.removeCamera(reloaded.cameras().size() - 1, &error), qPrintable(error));
    QVERIFY2(reloaded.removeLens(reloaded.lenses().size() - 1, &error), qPrintable(error));
    QVERIFY2(reloaded.removeLight(reloaded.lights().size() - 1, &error), qPrintable(error));

    CatalogRepository finalReload;
    finalReload.setStorageDirectory(storage.path());
    QVERIFY2(finalReload.loadDefaults(&error), qPrintable(error));
    QCOMPARE(finalReload.cameras().size(), originalCameraCount);
    QCOMPARE(finalReload.lenses().size(), originalLensCount);
    QCOMPARE(finalReload.lights().size(), originalLightCount);
}

void SelectionEngineTest::motionExposureAndStrobePreference()
{
    SelectionRequest request;
    request.objectWidthMm = 30.0;
    request.objectHeightMm = 20.0;
    request.placementMarginMm = 2.0;
    request.minFeatureUm = 100.0;
    request.measurementToleranceUm = 50.0;
    request.workingDistanceMm = 120.0;
    request.motionSpeedMmS = 500.0;
    request.requiredFps = 30.0;
    request.detectionType = DetectionType::Positioning;

    const RequirementEstimate requirement = CalculationAssistant::estimateRequirement(request);
    QVERIFY(requirement.hasMotionConstraint);
    QVERIFY(requirement.maxExposureUsForOnePixelBlur > 0.0);
    QVERIFY(requirement.maxExposureUsForOnePixelBlur < 100.0);

    CameraSpec camera;
    camera.model = QStringLiteral("CAM");
    camera.manufacturer = QStringLiteral("Test");
    camera.resolutionX = 2448;
    camera.resolutionY = 2048;
    camera.pixelSizeUm = 3.45;
    camera.colorMode = QStringLiteral("Mono");
    camera.shutterType = QStringLiteral("Global");
    camera.maxFps = 100.0;
    camera.interfaceType = QStringLiteral("USB3");
    camera.bandwidthMBps = 500.0;
    camera.bitDepth = 8.0;
    camera.lensMount = QStringLiteral("C");

    LensSpec lens;
    lens.model = QStringLiteral("LENS");
    lens.manufacturer = QStringLiteral("Test");
    lens.lensType = LensType::FixedFocal;
    lens.lensMount = QStringLiteral("C");
    lens.focalLengthMm = 25.0;
    lens.minWorkingDistanceMm = 80.0;
    lens.imageCircleMm = 12.0;
    lens.fNumber = 2.8;

    LightSpec continuous;
    continuous.model = QStringLiteral("CONT");
    continuous.manufacturer = QStringLiteral("Test");
    continuous.lightType = LightType::Ring;
    continuous.mode = QStringLiteral("Continuous");
    continuous.activeWidthMm = 100.0;
    continuous.activeHeightMm = 100.0;

    LightSpec strobe = continuous;
    strobe.model = QStringLiteral("STROBE");
    strobe.mode = QStringLiteral("Strobe");

    SelectionEngine engine;
    const QVector<SelectionResult> results = engine.select(request, {camera}, {lens}, {continuous, strobe}, 1);
    QVERIFY(!results.isEmpty());
    QCOMPARE(results.first().light.model, QStringLiteral("STROBE"));
    QVERIFY(results.first().maxExposureUsForOnePixelBlur > 0.0);
}

void SelectionEngineTest::dataThroughputAndInterfaceRisk()
{
    SelectionRequest request;
    request.objectWidthMm = 40.0;
    request.objectHeightMm = 30.0;
    request.placementMarginMm = 2.0;
    request.minFeatureUm = 100.0;
    request.measurementToleranceUm = 50.0;
    request.workingDistanceMm = 120.0;
    request.requiredFps = 200.0;
    request.detectionType = DetectionType::Positioning;

    CameraSpec camera;
    camera.model = QStringLiteral("HIGH-DATA-CAM");
    camera.manufacturer = QStringLiteral("Test");
    camera.resolutionX = 4096;
    camera.resolutionY = 4096;
    camera.pixelSizeUm = 3.45;
    camera.colorMode = QStringLiteral("Mono");
    camera.shutterType = QStringLiteral("Global");
    camera.maxFps = 250.0;
    camera.interfaceType = QStringLiteral("GigE");
    camera.bandwidthMBps = 120.0;
    camera.bitDepth = 12.0;
    camera.lensMount = QStringLiteral("C");

    LensSpec lens;
    lens.model = QStringLiteral("LOW-MP-LENS");
    lens.manufacturer = QStringLiteral("Test");
    lens.lensType = LensType::FixedFocal;
    lens.lensMount = QStringLiteral("C");
    lens.focalLengthMm = 16.0;
    lens.minWorkingDistanceMm = 50.0;
    lens.imageCircleMm = 20.0;
    lens.megapixelRating = 5.0;
    lens.recommendedMinPixelUm = 3.45;
    lens.fNumber = 4.0;

    LightSpec light;
    light.model = QStringLiteral("LIGHT");
    light.manufacturer = QStringLiteral("Test");
    light.lightType = LightType::Ring;
    light.mode = QStringLiteral("Strobe");
    light.activeWidthMm = 100.0;
    light.activeHeightMm = 100.0;

    QCOMPARE(SelectionEngine::framePayloadMB(camera) > 20.0, true);
    QCOMPARE(SelectionEngine::interfaceCapacityMBps(camera), 120.0);

    CameraSpec fallback = camera;
    fallback.bandwidthMBps = 0.0;
    fallback.interfaceType = QStringLiteral("USB3");
    QCOMPARE(SelectionEngine::interfaceCapacityMBps(fallback), 380.0);

    SelectionEngine engine;
    const QVector<SelectionResult> results = engine.select(request, {camera}, {lens}, {light}, 1);
    QVERIFY(!results.isEmpty());
    QVERIFY(results.first().bandwidthRequiredMBps > results.first().interfaceCapacityMBps);
    QVERIFY(results.first().bandwidthUtilizationPercent > 100.0);
    QVERIFY(results.first().storagePerHourGB > 1000.0);
    QVERIFY(results.first().lensMegapixelUtilizationPercent > 100.0);
    const QString risks = results.first().score.risks.join(QStringLiteral(";"));
    QVERIFY2(risks.contains(QString::fromUtf8("带宽")), qPrintable(risks));
    QVERIFY2(risks.contains(QStringLiteral("MP")), qPrintable(risks));
    QVERIFY(!results.first().hardConstraintsPassed);
    QVERIFY(results.first().hardFailures.join(QStringLiteral(";")).contains(QString::fromUtf8("接口带宽")));
}

void SelectionEngineTest::hardConstraintsPreferCompatibleResults()
{
    SelectionRequest request;
    request.objectWidthMm = 20.0;
    request.objectHeightMm = 18.0;
    request.placementMarginMm = 2.0;
    request.minFeatureUm = 300.0;
    request.measurementToleranceUm = 100.0;
    request.workingDistanceMm = 120.0;
    request.requiredFps = 10.0;
    request.detectionType = DetectionType::DefectInspection;
    request.heightVariationMm = 0.0;

    CameraSpec camera;
    camera.model = QStringLiteral("CAM");
    camera.manufacturer = QStringLiteral("Test");
    camera.resolutionX = 2448;
    camera.resolutionY = 2048;
    camera.pixelSizeUm = 3.45;
    camera.colorMode = QStringLiteral("Mono");
    camera.shutterType = QStringLiteral("Global");
    camera.maxFps = 60.0;
    camera.interfaceType = QStringLiteral("USB3");
    camera.bandwidthMBps = 500.0;
    camera.bitDepth = 8.0;
    camera.lensMount = QStringLiteral("C");

    LensSpec badLens;
    badLens.model = QStringLiteral("BAD-FOV");
    badLens.manufacturer = QStringLiteral("Test");
    badLens.lensType = LensType::FixedFocal;
    badLens.lensMount = QStringLiteral("C");
    badLens.focalLengthMm = 100.0;
    badLens.minWorkingDistanceMm = 50.0;
    badLens.imageCircleMm = 12.0;
    badLens.megapixelRating = 12.0;
    badLens.recommendedMinPixelUm = 3.45;
    badLens.fNumber = 4.0;

    LensSpec goodLens = badLens;
    goodLens.model = QStringLiteral("GOOD-FOV");
    goodLens.focalLengthMm = 16.0;

    LightSpec light;
    light.model = QStringLiteral("LIGHT");
    light.manufacturer = QStringLiteral("Test");
    light.lightType = LightType::Bar;
    light.mode = QStringLiteral("Strobe");
    light.activeWidthMm = 80.0;
    light.activeHeightMm = 80.0;

    SelectionEngine engine;
    const QVector<SelectionResult> results = engine.select(request, {camera}, {badLens, goodLens}, {light}, 1);
    QVERIFY(!results.isEmpty());
    QCOMPARE(results.first().lens.model, QStringLiteral("GOOD-FOV"));
    QVERIFY(results.first().hardConstraintsPassed);
}

void SelectionEngineTest::hardConstraintFallbackKeepsDiagnosticResult()
{
    SelectionRequest request;
    request.objectWidthMm = 20.0;
    request.objectHeightMm = 18.0;
    request.placementMarginMm = 2.0;
    request.minFeatureUm = 300.0;
    request.measurementToleranceUm = 100.0;
    request.workingDistanceMm = 120.0;
    request.requiredFps = 10.0;
    request.detectionType = DetectionType::DefectInspection;
    request.heightVariationMm = 0.0;

    CameraSpec camera;
    camera.model = QStringLiteral("CAM");
    camera.manufacturer = QStringLiteral("Test");
    camera.resolutionX = 2448;
    camera.resolutionY = 2048;
    camera.pixelSizeUm = 3.45;
    camera.colorMode = QStringLiteral("Mono");
    camera.shutterType = QStringLiteral("Global");
    camera.maxFps = 60.0;
    camera.interfaceType = QStringLiteral("USB3");
    camera.bandwidthMBps = 500.0;
    camera.bitDepth = 8.0;
    camera.lensMount = QStringLiteral("C");

    LensSpec lens;
    lens.model = QStringLiteral("BAD-FOV");
    lens.manufacturer = QStringLiteral("Test");
    lens.lensType = LensType::FixedFocal;
    lens.lensMount = QStringLiteral("C");
    lens.focalLengthMm = 100.0;
    lens.minWorkingDistanceMm = 50.0;
    lens.imageCircleMm = 12.0;
    lens.megapixelRating = 12.0;
    lens.recommendedMinPixelUm = 3.45;
    lens.fNumber = 4.0;

    LightSpec light;
    light.model = QStringLiteral("LIGHT");
    light.manufacturer = QStringLiteral("Test");
    light.lightType = LightType::Bar;
    light.mode = QStringLiteral("Strobe");
    light.activeWidthMm = 80.0;
    light.activeHeightMm = 80.0;

    SelectionEngine engine;
    const QVector<SelectionResult> results = engine.select(request, {camera}, {lens}, {light}, 1);
    QVERIFY(!results.isEmpty());
    QVERIFY(!results.first().hardConstraintsPassed);
    QVERIFY(results.first().score.score <= 20.0);
    QVERIFY(results.first().hardFailures.join(QStringLiteral(";")).contains(QStringLiteral("FOV")));
}

void SelectionEngineTest::fixedLensDofAndDistortionRisk()
{
    SelectionRequest request;
    request.objectWidthMm = 20.0;
    request.objectHeightMm = 20.0;
    request.placementMarginMm = 2.0;
    request.minFeatureUm = 50.0;
    request.measurementToleranceUm = 10.0;
    request.workingDistanceMm = 110.0;
    request.heightVariationMm = 2.0;
    request.detectionType = DetectionType::Measurement;

    CameraSpec camera;
    camera.model = QStringLiteral("CAM");
    camera.manufacturer = QStringLiteral("Test");
    camera.resolutionX = 2448;
    camera.resolutionY = 2048;
    camera.pixelSizeUm = 3.45;
    camera.colorMode = QStringLiteral("Mono");
    camera.shutterType = QStringLiteral("Global");
    camera.maxFps = 60.0;
    camera.interfaceType = QStringLiteral("USB3");
    camera.bandwidthMBps = 500.0;
    camera.bitDepth = 8.0;
    camera.lensMount = QStringLiteral("C");

    LensSpec lens;
    lens.model = QStringLiteral("DISTORTED");
    lens.manufacturer = QStringLiteral("Test");
    lens.lensType = LensType::FixedFocal;
    lens.lensMount = QStringLiteral("C");
    lens.focalLengthMm = 25.0;
    lens.minWorkingDistanceMm = 80.0;
    lens.imageCircleMm = 12.0;
    lens.distortionPercent = 5.0;
    lens.fNumber = 2.8;

    LightSpec light;
    light.model = QStringLiteral("LIGHT");
    light.manufacturer = QStringLiteral("Test");
    light.lightType = LightType::Backlight;
    light.mode = QStringLiteral("Strobe");
    light.activeWidthMm = 100.0;
    light.activeHeightMm = 100.0;

    SelectionEngine engine;
    const QVector<SelectionResult> results = engine.select(request, {camera}, {lens}, {light}, 1);
    QVERIFY(!results.isEmpty());
    QVERIFY(results.first().estimatedDofMm > 0.0);
    QVERIFY(results.first().distortionErrorUm > request.measurementToleranceUm);
    QVERIFY(results.first().score.risks.join(QStringLiteral(";")).contains(QString::fromUtf8("畸变")));
    QVERIFY(results.first().score.risks.join(QStringLiteral(";")).contains(QStringLiteral("DOF")));
}

void SelectionEngineTest::lightCoverageAffectsScore()
{
    SelectionRequest request;
    request.objectWidthMm = 80.0;
    request.objectHeightMm = 60.0;
    request.placementMarginMm = 5.0;
    request.minFeatureUm = 200.0;
    request.measurementToleranceUm = 80.0;
    request.workingDistanceMm = 180.0;
    request.detectionType = DetectionType::DefectInspection;

    CameraSpec camera;
    camera.model = QStringLiteral("CAM");
    camera.manufacturer = QStringLiteral("Test");
    camera.resolutionX = 2448;
    camera.resolutionY = 2048;
    camera.pixelSizeUm = 3.45;
    camera.colorMode = QStringLiteral("Mono");
    camera.shutterType = QStringLiteral("Global");
    camera.maxFps = 60.0;
    camera.interfaceType = QStringLiteral("USB3");
    camera.bandwidthMBps = 500.0;
    camera.bitDepth = 8.0;
    camera.lensMount = QStringLiteral("C");

    LensSpec lens;
    lens.model = QStringLiteral("LENS");
    lens.manufacturer = QStringLiteral("Test");
    lens.lensType = LensType::FixedFocal;
    lens.lensMount = QStringLiteral("C");
    lens.focalLengthMm = 12.0;
    lens.minWorkingDistanceMm = 50.0;
    lens.imageCircleMm = 12.0;
    lens.fNumber = 4.0;

    LightSpec small;
    small.model = QStringLiteral("SMALL");
    small.manufacturer = QStringLiteral("Test");
    small.lightType = LightType::Bar;
    small.mode = QStringLiteral("Strobe");
    small.activeWidthMm = 30.0;
    small.activeHeightMm = 30.0;

    LightSpec large = small;
    large.model = QStringLiteral("LARGE");
    large.activeWidthMm = 140.0;
    large.activeHeightMm = 120.0;

    SelectionEngine engine;
    const QVector<SelectionResult> results = engine.select(request, {camera}, {lens}, {small, large}, 1);
    QVERIFY(!results.isEmpty());
    QCOMPARE(results.first().light.model, QStringLiteral("LARGE"));
    QVERIFY(results.first().lightCoverageMarginPercent >= 10.0);
}

void SelectionEngineTest::telecentricMeasurementWins()
{
    SelectionRequest request;
    request.objectWidthMm = 20.0;
    request.objectHeightMm = 20.0;
    request.placementMarginMm = 2.0;
    request.minFeatureUm = 50.0;
    request.measurementToleranceUm = 10.0;
    request.workingDistanceMm = 110.0;
    request.heightVariationMm = 2.0;
    request.detectionType = DetectionType::Measurement;
    request.surfaceType = SurfaceType::ReflectiveMetal;
    request.reflective = true;

    SelectionEngine engine;
    const QVector<SelectionResult> results = engine.select(request, m_catalog.cameras(), m_catalog.lenses(), m_catalog.lights(), 5);
    QVERIFY(!results.isEmpty());
    QVERIFY2(results.first().isTelecentric(), qPrintable(results.first().lens.model));
    QVERIFY(results.first().lens.lensType == LensType::BiTelecentric
            || results.first().lens.lensType == LensType::ObjectTelecentric);
}

void SelectionEngineTest::largeFovPrefersFixedFocal()
{
    SelectionRequest request;
    request.objectWidthMm = 200.0;
    request.objectHeightMm = 120.0;
    request.placementMarginMm = 10.0;
    request.minFeatureUm = 800.0;
    request.measurementToleranceUm = 500.0;
    request.workingDistanceMm = 300.0;
    request.heightVariationMm = 0.2;
    request.detectionType = DetectionType::DefectInspection;
    request.surfaceType = SurfaceType::Plastic;
    request.reflective = false;

    SelectionEngine engine;
    const QVector<SelectionResult> results = engine.select(request, m_catalog.cameras(), m_catalog.lenses(), m_catalog.lights(), 5);
    QVERIFY(!results.isEmpty());
    QVERIFY2(!results.first().isTelecentric(), qPrintable(results.first().lens.model));
}

void SelectionEngineTest::motionPrefersGlobalShutter()
{
    SelectionRequest request;
    request.objectWidthMm = 60.0;
    request.objectHeightMm = 40.0;
    request.placementMarginMm = 5.0;
    request.minFeatureUm = 200.0;
    request.measurementToleranceUm = 100.0;
    request.workingDistanceMm = 220.0;
    request.motionSpeedMmS = 500.0;
    request.requiredFps = 40.0;
    request.detectionType = DetectionType::Positioning;

    SelectionEngine engine;
    const QVector<SelectionResult> results = engine.select(request, m_catalog.cameras(), m_catalog.lenses(), m_catalog.lights(), 5);
    QVERIFY(!results.isEmpty());
    QVERIFY2(results.first().camera.isGlobalShutter(), qPrintable(results.first().camera.model));
}

void SelectionEngineTest::reflectiveSurfaceGetsCoaxialOrDome()
{
    SelectionRequest request;
    request.objectWidthMm = 25.0;
    request.objectHeightMm = 20.0;
    request.placementMarginMm = 3.0;
    request.minFeatureUm = 80.0;
    request.measurementToleranceUm = 25.0;
    request.workingDistanceMm = 110.0;
    request.detectionType = DetectionType::Measurement;
    request.surfaceType = SurfaceType::ReflectiveMetal;
    request.reflective = true;

    SelectionEngine engine;
    const QVector<SelectionResult> results = engine.select(request, m_catalog.cameras(), m_catalog.lenses(), m_catalog.lights(), 5);
    QVERIFY(!results.isEmpty());
    const LightType type = results.first().light.lightType;
    QVERIFY(type == LightType::Coaxial || type == LightType::Dome || type == LightType::TelecentricBacklight);
}

void SelectionEngineTest::chineseTextIsUnicode()
{
    const QString measurementText = QString::fromUtf8("\345\260\272\345\257\270\346\265\213\351\207\217");
    const QString biTelecentricText = QString::fromUtf8("\345\217\214\350\277\234\345\277\203\351\225\234\345\244\264");
    const QString telecentricWord = QString::fromUtf8("\350\277\234\345\277\203\351\225\234\345\244\264");

    QCOMPARE(detectionTypeLabel(DetectionType::Measurement), measurementText);
    QCOMPARE(lensTypeLabel(LensType::BiTelecentric), biTelecentricText);

    bool hasDecodedTelecentricUseCase = false;
    QStringList decodedLightUseCases;
    for (const LightSpec &light : m_catalog.lights())
    {
        hasDecodedTelecentricUseCase = hasDecodedTelecentricUseCase
            || light.bestFor.contains(telecentricWord);
        QStringList codepoints;
        for (const QChar ch : light.bestFor)
            codepoints.append(QString::number(ch.unicode(), 16));
        decodedLightUseCases.append(light.model + QStringLiteral("=") + codepoints.join(QLatin1Char(' ')));
    }
    QVERIFY2(hasDecodedTelecentricUseCase, qPrintable(decodedLightUseCases.join(QStringLiteral(" | "))));
}

void SelectionEngineTest::threeDCameraCatalogLoadsFromResource()
{
    ThreeDCameraRepository repository;
    QString error;
    QVERIFY2(repository.loadFromResource(QStringLiteral(":/data/three_d_cameras.json"), &error), qPrintable(error));
    QVERIFY(repository.cameras().size() >= 40);

    QSet<QString> brands;
    int lmiCount = 0;
    int keyenceCount = 0;
    int sinceVisionCount = 0;
    for (const ThreeDCameraSpec &camera : repository.cameras()) {
        QVERIFY2(!camera.manufacturer.trimmed().isEmpty(), qPrintable(camera.model));
        QVERIFY2(!camera.series.trimmed().isEmpty(), qPrintable(camera.model));
        QVERIFY2(!camera.model.trimmed().isEmpty(), qPrintable(camera.series));
        QVERIFY2(!camera.technologyLabel.trimmed().isEmpty(), qPrintable(camera.model));
        QVERIFY2(!camera.status.trimmed().isEmpty(), qPrintable(camera.model));
        QVERIFY2(!camera.sourceUrl.trimmed().isEmpty(), qPrintable(camera.model));
        QVERIFY2(!camera.sourceDate.trimmed().isEmpty(), qPrintable(camera.model));
        QVERIFY2(!camera.rawSpecs.isEmpty(), qPrintable(camera.model));
        brands.insert(camera.manufacturer);
        if (camera.manufacturer == QStringLiteral("LMI"))
            ++lmiCount;
        if (camera.manufacturer == QString::fromUtf8("基恩士"))
            ++keyenceCount;
        if (camera.manufacturer == QString::fromUtf8("深视智能"))
            ++sinceVisionCount;
    }

    QVERIFY(brands.contains(QStringLiteral("LMI")));
    QVERIFY(lmiCount >= 60);
    QVERIFY(brands.contains(QString::fromUtf8("深视智能")));
    QVERIFY(sinceVisionCount >= 50);
    QVERIFY(brands.contains(QString::fromUtf8("基恩士")));
    QVERIFY(keyenceCount >= 27);
    QVERIFY(brands.contains(QString::fromUtf8("海康机器人")));
}

void SelectionEngineTest::threeDCameraMatcherClassifiesRequirements()
{
    ThreeDCameraRepository repository;
    QString error;
    QVERIFY2(repository.loadFromResource(QStringLiteral(":/data/three_d_cameras.json"), &error), qPrintable(error));

    ThreeDCameraMatcher matcher;
    ThreeDCameraRequirement matchRequest;
    matchRequest.manufacturer = QStringLiteral("LMI");
    matchRequest.technologyLabel = threeDTechnologyLabel(ThreeDTechnology::LineLaserProfile);
    matchRequest.targetXCoverageMm = 100.0;
    matchRequest.zMeasurementRangeMm = 100.0;
    matchRequest.maxZRepeatabilityUm = 5.0;
    matchRequest.minSpeedHz = 5000.0;
    matchRequest.requireEncoder = true;
    matchRequest.interfaceText = QStringLiteral("Ethernet");
    const QVector<ThreeDCameraMatch> matching = matcher.match(matchRequest, repository.cameras());
    QVERIFY(!matching.isEmpty());
    bool hasMatch = false;
    for (const ThreeDCameraMatch &candidate : matching)
        hasMatch = hasMatch || candidate.status == ThreeDMatchStatus::Match;
    QVERIFY(hasMatch);

    ThreeDCameraRequirement missingRequest;
    missingRequest.manufacturer = QStringLiteral("LMI");
    missingRequest.technologyLabel = threeDTechnologyLabel(ThreeDTechnology::LineLaserProfile);
    missingRequest.targetYCoverageMm = 20.0;
    const QVector<ThreeDCameraMatch> missing = matcher.match(missingRequest, repository.cameras());
    bool hasMissing = false;
    for (const ThreeDCameraMatch &candidate : missing)
        hasMissing = hasMissing || candidate.status == ThreeDMatchStatus::MissingData;
    QVERIFY(hasMissing);

    ThreeDCameraRequirement impossibleRequest;
    impossibleRequest.targetXCoverageMm = 3000.0;
    impossibleRequest.zMeasurementRangeMm = 3000.0;
    const QVector<ThreeDCameraMatch> rejected = matcher.match(impossibleRequest, repository.cameras());
    QVERIFY(!rejected.isEmpty());
    bool hasNoMatch = false;
    bool hasImpossibleMatch = false;
    for (const ThreeDCameraMatch &candidate : rejected) {
        hasNoMatch = hasNoMatch || candidate.status == ThreeDMatchStatus::NoMatch;
        hasImpossibleMatch = hasImpossibleMatch || candidate.status == ThreeDMatchStatus::Match;
    }
    QVERIFY(hasNoMatch);
    QVERIFY(!hasImpossibleMatch);

    const QVector<ThreeDCameraMatch> empty = matcher.match(ThreeDCameraRequirement(), repository.cameras());
    QVERIFY(!empty.isEmpty());
    QCOMPARE(empty.first().status, ThreeDMatchStatus::Match);
}

void SelectionEngineTest::threeDCameraDataDoesNotAffect2DSelection()
{
    SelectionRequest request;
    request.objectWidthMm = 40.0;
    request.objectHeightMm = 30.0;
    request.placementMarginMm = 2.0;
    request.minFeatureUm = 100.0;
    request.measurementToleranceUm = 50.0;
    request.workingDistanceMm = 120.0;
    request.requiredFps = 30.0;
    request.detectionType = DetectionType::Positioning;

    SelectionEngine engine;
    const QVector<SelectionResult> before = engine.select(request, m_catalog.cameras(), m_catalog.lenses(), m_catalog.lights(), 5);
    ThreeDCameraRepository repository;
    QString error;
    QVERIFY2(repository.loadFromResource(QStringLiteral(":/data/three_d_cameras.json"), &error), qPrintable(error));
    const QVector<SelectionResult> after = engine.select(request, m_catalog.cameras(), m_catalog.lenses(), m_catalog.lights(), 5);
    QCOMPARE(after.size(), before.size());
    if (!before.isEmpty() && !after.isEmpty()) {
        QCOMPARE(after.first().camera.model, before.first().camera.model);
        QCOMPARE(after.first().lens.model, before.first().lens.model);
        QCOMPARE(after.first().light.model, before.first().light.model);
    }
}

void SelectionEngineTest::licenseValidationCoversSignatureMachineAndExpiry()
{
    LicenseManager manager;
    manager.setPublicKeyForTesting(
        QByteArray::fromBase64("rEp37pqaNzBLvrsO4nbwg0qe7RV26vXC877GLtSbovngzNhrMKAViHCfYWxh2UMJvAKy/Kh7A9MHcyklhAKi3z3LGQo3G2ha2Siww497ECFEo/kaTpGMNQ/d5F2nY96e3teM6fi2hNddwbFKCQQ3GyYwcBIi3XzKamglLBfl5bLQxeZ9zC5jTRqD19b1La+KZxACFPAWsWUBFG2da8N/5DGjo8DTSG51d5yxWFbKz9A5SaOim+bfLW2Rp8zVhg2W5OU5lcc0Tn6nm/CmrA5XJHtQ3fYVRfw2fOm/oexQdXYwcgTCspOFrlgQJFjkEGU86oFS9KOJDzIduvzHQ+9yXQ=="),
        QByteArray::fromBase64("AQAB"));
    const QString key = QStringLiteral("VS1-eyJwcm9kdWN0SWQiOiJWaXNpb25TZWxlY3QiLCJsaWNlbnNlZSI6IlVuaXQgVGVzdCIsInNlcmlhbCI6IlVULTAwMSIsIm1hY2hpbmVDb2RlIjoiQUJDRC1FRkdILUlKS0wtTU5PUCIsImlzc3VlZEF0IjoiMjAyNi0wMS0wMSIsImV4cGlyZXNBdCI6IjIwOTktMTItMzEiLCJmZWF0dXJlcyI6WyJzdGFuZGFyZCJdfQ==-ZIDJ4aU7bVs3mAFTiJUivtS+xcmM/xOSZh3BHrbh28lhTIw/p0wk2OVJKMJemq6sPcyBjSNwTq/3+9WwSTfh8D76EbYl44TpDgdIWCqFUfBKOM2u+mg8+Yvp2Rf85mUaDphOxpSVxFoPO5PZ8d8odVmUiw92bBYtje7vdxmG5rHKkhKr7r2rlKKE2cbuGrozDKjAfjVJSV3Q8SDnc6vbahBQeHKQLT0gYJI2bHSVokH+cV6UyzObSXmSS7U5jQjtTKIuL8qUvfWP306SGb/89EO+kb7dVqQB+0hQLmqhYmimow3nFY2KlWpmlDOHzIAVgny6mezxg+JDoFWT/2dkJQ==");

    LicenseStatus status = manager.validateKeyForMachine(key, QStringLiteral("ABCD-EFGH-IJKL-MNOP"), QDate(2026, 5, 31));
    QCOMPARE(status.code, LicenseStatusCode::Valid);
    QCOMPARE(status.info.licensee, QStringLiteral("Unit Test"));

    status = manager.validateKeyForMachine(key, QStringLiteral("0000-0000-0000-0000"), QDate(2026, 5, 31));
    QCOMPARE(status.code, LicenseStatusCode::MachineMismatch);

    status = manager.validateKeyForMachine(key, QStringLiteral("ABCD-EFGH-IJKL-MNOP"), QDate(2100, 1, 1));
    QCOMPARE(status.code, LicenseStatusCode::Expired);

    QString tampered = key;
    tampered.replace(QStringLiteral("VGVzdC"), QStringLiteral("VGVzdA"));
    status = manager.validateKeyForMachine(tampered, QStringLiteral("ABCD-EFGH-IJKL-MNOP"), QDate(2026, 5, 31));
    QVERIFY(status.code == LicenseStatusCode::InvalidFormat || status.code == LicenseStatusCode::BadSignature);
}

void SelectionEngineTest::licenseIssuerParsesXmlAndSignsCompatibleKey()
{
    LicenseIssuer issuer;
    QString error;
    QVERIFY2(issuer.loadPrivateKeyXml(QByteArray(kTestPrivateKeyXml), &error), qPrintable(error));
    QCOMPARE(issuer.publicModulus(), QByteArray::fromBase64("5hzYFnHq3/1l3dpJFHV8XBnUejhF6oIE5lVzrDcKm1rq5bLIOTKmRgJmjaa9had4v8w1W3jIX1E/OU5y50KE2YDqHJvAPkiOT7Zpka5U7+pypzLEH5zQfyeaKKgQsXxgoGq3z6DtKv/1mfz5xq0jv5Nr4Ouv/Xep5LNuk8eG7nE="));
    QCOMPARE(issuer.publicExponent(), QByteArray::fromBase64("AQAB"));

    LicenseIssueRequest request;
    request.licensee = QStringLiteral("Issuer Test");
    request.serial = QStringLiteral("VS-UNIT-001");
    request.machineCode = QStringLiteral(" abcd-efgh-ijkl-mnop ");
    request.issuedAt = QDate::currentDate();
    request.expiresAt = QDate::currentDate().addDays(30);
    request.features = QStringList() << QStringLiteral("standard") << QStringLiteral("pro");

    LicenseIssueResult result;
    QVERIFY2(issuer.issue(request, &result, &error), qPrintable(error));
    QVERIFY(result.licenseKey.startsWith(QStringLiteral("VS1-")));
    QCOMPARE(result.normalizedMachineCode, QStringLiteral("ABCD-EFGH-IJKL-MNOP"));
    QVERIFY(result.payloadJson.contains("\"licensee\":\"Issuer Test\""));
    QVERIFY(result.payloadJson.contains("\"features\":[\"standard\",\"pro\"]"));

    LicenseManager manager;
    manager.setPublicKeyForTesting(issuer.publicModulus(), issuer.publicExponent());
    LicenseStatus status = manager.validateKeyForMachine(result.licenseKey, result.normalizedMachineCode, QDate::currentDate());
    QCOMPARE(status.code, LicenseStatusCode::Valid);
    QCOMPARE(status.info.serial, request.serial);

    status = manager.validateKeyForMachine(result.licenseKey, QStringLiteral("0000-0000-0000-0000"), QDate::currentDate());
    QCOMPARE(status.code, LicenseStatusCode::MachineMismatch);

    status = manager.validateKeyForMachine(result.licenseKey, result.normalizedMachineCode, request.expiresAt.addDays(1));
    QCOMPARE(status.code, LicenseStatusCode::Expired);

    QString tampered = result.licenseKey;
    tampered[tampered.size() - 1] = tampered.endsWith(QLatin1Char('A')) ? QLatin1Char('B') : QLatin1Char('A');
    status = manager.validateKeyForMachine(tampered, result.normalizedMachineCode, QDate::currentDate());
    QVERIFY(status.code == LicenseStatusCode::InvalidFormat || status.code == LicenseStatusCode::BadSignature);
}

void SelectionEngineTest::licenseIssuerRejectsInvalidInput()
{
    LicenseIssuer issuer;
    QString error;
    LicenseIssueRequest request;
    LicenseIssueResult result;
    QVERIFY(!issuer.issue(request, &result, &error));
    QVERIFY(!error.isEmpty());

    QVERIFY(!issuer.loadPrivateKeyXml(QByteArray("<RSAKeyValue><Modulus>bad</Modulus></RSAKeyValue>"), &error));
    QVERIFY(!error.isEmpty());

    QVERIFY2(issuer.loadPrivateKeyXml(QByteArray(kTestPrivateKeyXml), &error), qPrintable(error));
    request.licensee = QStringLiteral("Issuer Test");
    request.serial = QStringLiteral("VS-UNIT-002");
    request.machineCode = QStringLiteral("ABCD-EFGH-IJKL-MNOP");
    request.expiresAt = QDate::currentDate().addDays(-1);
    QVERIFY(!issuer.issue(request, &result, &error));
    QVERIFY(error.contains(QStringLiteral("Expiration")));
}

void SelectionEngineTest::machineCodeGenerationIsStable()
{
    const QString codeA = LicenseManager::machineCodeForSeeds({QStringLiteral("machine-guid"), QStringLiteral("host")});
    const QString codeB = LicenseManager::machineCodeForSeeds({QStringLiteral("host"), QStringLiteral("machine-guid")});
    QCOMPARE(codeA, codeB);
    QCOMPARE(codeA.size(), 19);
    QVERIFY(codeA.contains(QLatin1Char('-')));
}

void SelectionEngineTest::languageManagerSwitchesAvailableLanguages()
{
    LanguageManager &manager = LanguageManager::instance();
    const QString previous = manager.currentLanguage();
    QVERIFY(manager.availableLanguages().contains(QStringLiteral("zh_CN")));
    QVERIFY(manager.availableLanguages().contains(QStringLiteral("en_US")));
    QVERIFY(manager.setLanguage(QStringLiteral("en_US")));
    QCOMPARE(manager.currentLanguage(), QStringLiteral("en_US"));
    QVERIFY(manager.setLanguage(QStringLiteral("zh_CN")));
    QCOMPARE(manager.currentLanguage(), QStringLiteral("zh_CN"));
    manager.setLanguage(previous);
}

void SelectionEngineTest::invalidTelecentricCsvFails()
{
    QTemporaryFile file;
    QVERIFY(file.open());
    file.write("model,lens_type,lens_mount,distortion_percent,image_circle_mm,focal_length_mm\n");
    file.write("BadTC,BiTelecentric,C,0.01,11,0\n");
    file.flush();

    CatalogRepository repo;
    QTemporaryDir storage;
    QVERIFY(storage.isValid());
    repo.setStorageDirectory(storage.path());
    QString error;
    QVERIFY2(repo.loadDefaults(&error), qPrintable(error));
    const int originalLensCount = repo.lenses().size();
    QVERIFY(!repo.loadLensCsv(file.fileName(), &error));
    QVERIFY2(error.contains(QStringLiteral("PMAG")), qPrintable(error));
    QCOMPARE(repo.lenses().size(), originalLensCount);
}

void SelectionEngineTest::invalidLightCsvFails()
{
    QTemporaryFile file;
    QVERIFY(file.open());
    file.write("model,manufacturer,light_type,color,wavelength_nm,mode,active_width_mm,active_height_mm,best_for\n");
    file.write(",BadMaker,Ring,White,0,Continuous,0,100,Bad row\n");
    file.flush();

    CatalogRepository repo;
    QTemporaryDir storage;
    QVERIFY(storage.isValid());
    repo.setStorageDirectory(storage.path());
    QString error;
    QVERIFY2(repo.loadDefaults(&error), qPrintable(error));
    const int originalLightCount = repo.lights().size();
    QVERIFY(!repo.loadLightCsv(file.fileName(), &error));
    QVERIFY2(error.contains(QString::fromUtf8("光源数据无效")), qPrintable(error));
    QCOMPARE(repo.lights().size(), originalLightCount);
}

void SelectionEngineTest::pdfReportWrites()
{
    SelectionRequest request;
    SelectionEngine engine;
    const QVector<SelectionResult> results = engine.select(request, m_catalog.cameras(), m_catalog.lenses(), m_catalog.lights(), 5);
    QVERIFY(!results.isEmpty());

    QTemporaryFile file(QDir::tempPath() + QStringLiteral("/visionselect_report_XXXXXX.pdf"));
    QVERIFY(file.open());
    const QString path = file.fileName();
    file.close();

    PdfReportWriter writer;
    QString error;
    QVERIFY2(writer.write(path, request, results, &error), qPrintable(error));
    QVERIFY(QFileInfo(path).size() > 1000);
}

QTEST_MAIN(SelectionEngineTest)
#include "test_selection.moc"
