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
    void telecentricMeasurementWins();
    void largeFovPrefersFixedFocal();
    void motionPrefersGlobalShutter();
    void reflectiveSurfaceGetsCoaxialOrDome();
    void chineseTextIsUnicode();
    void invalidTelecentricCsvFails();
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

    CatalogRepository reloaded;
    reloaded.setStorageDirectory(storage.path());
    QVERIFY2(reloaded.loadDefaults(&error), qPrintable(error));
    QCOMPARE(reloaded.cameras().size(), originalCameraCount + 1);
    QCOMPARE(reloaded.lenses().size(), originalLensCount + 1);
    QCOMPARE(reloaded.cameras().last().model, QStringLiteral("TEST-CAM-001"));
    QCOMPARE(reloaded.lenses().last().model, QStringLiteral("TEST-LENS-25"));

    CameraSpec editedCamera = reloaded.cameras().last();
    editedCamera.model = QStringLiteral("TEST-CAM-EDITED");
    QVERIFY2(reloaded.updateCamera(reloaded.cameras().size() - 1, editedCamera, &error), qPrintable(error));
    QVERIFY2(reloaded.removeCamera(reloaded.cameras().size() - 1, &error), qPrintable(error));
    QVERIFY2(reloaded.removeLens(reloaded.lenses().size() - 1, &error), qPrintable(error));

    CatalogRepository finalReload;
    finalReload.setStorageDirectory(storage.path());
    QVERIFY2(finalReload.loadDefaults(&error), qPrintable(error));
    QCOMPARE(finalReload.cameras().size(), originalCameraCount);
    QCOMPARE(finalReload.lenses().size(), originalLensCount);
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
