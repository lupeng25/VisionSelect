#include "catalog/CatalogRepository.h"
#include "report/PdfReportWriter.h"
#include "selection/CalculationAssistant.h"
#include "selection/SelectionEngine.h"

#include <QDir>
#include <QFileInfo>
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
    void catalogPersistenceRoundTrip();
    void motionExposureAndStrobePreference();
    void dataThroughputAndInterfaceRisk();
    void fixedLensDofAndDistortionRisk();
    void lightCoverageAffectsScore();
    void telecentricMeasurementWins();
    void largeFovPrefersFixedFocal();
    void motionPrefersGlobalShutter();
    void reflectiveSurfaceGetsCoaxialOrDome();
    void chineseTextIsUnicode();
    void invalidTelecentricCsvFails();
    void invalidLightCsvFails();
    void pdfReportWrites();

private:
    QTemporaryDir m_catalogStorage;
    CatalogRepository m_catalog;
};

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
