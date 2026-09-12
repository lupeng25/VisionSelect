#include "selection/CandidateValidator.h"
#include "report/BomCsvWriter.h"
#include <QBuffer>
#include "catalog/CatalogRepository.h"
#include "core/Localization.h"
#include "i18n/LanguageManager.h"
#include "license/LicenseIssuer.h"
#include "license/LicenseManager.h"
#include "report/PdfReportWriter.h"
#include "selection/CalculationAssistant.h"
#include "selection/ParameterCalculator.h"
#include "core/PixelFormat.h"
#include "selection/SelectionEngine.h"
#include "selection/SelectionService.h"
#include "three_d/ThreeDCalculation.h"
#include "three_d/ThreeDCameraMatcher.h"
#include "three_d/ThreeDCameraRepository.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTextStream>
#include <QtTest/QtTest>

class SelectionEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void defaultCatalogManufacturersAreLoaded();
    void calculationAssistantEstimatesRequirements();
    void measurementToleranceUsesPixelBudget();
    void nonMeasurementToleranceDoesNotTightenSampling();
    void lensAssistantEstimatesLenses();
    void pureCalculationFixedLens();
    void pureCalculationTelecentric();
    void calculationAuditRegressions();
    void candidateValidationMatchesAssistantAndPreservesUnknowns();
    void candidateRecallKeepsCompatibleCameraBeyond96();
    void parameterOpticsSolvesBothModelsAndAxes();
    void parameterSamplingAndCalibration();
    void parameterTelecentricRangeAndMotion();
    void parameterTransferUsesRoiPackingAndStorageFormat();
    void parameterSystemCheckPreservesUnknowns();
    void telecentricMissingCatalogDataIsRisk();
    void missingTelecentricityIsRisk();
    void lensTypeParsingRecognizesTelecentricAliases();
    void lensMountCompatibilityIsConservative();
    void fixedFocalTargetUsesLimitingAxis();
    void nonMeasurementRequirementsDoNotForceTelecentric();
    void catalogPersistenceRoundTrip();
    void sqliteInitializeDatabaseKeepsCompatibilitySnapshotsLazy();
    void sqliteInitializeDatabaseRemovesDeletedBuiltIns();
    void sqliteCatalogQueriesPageAndDistinctValues();
    void sqliteCatalogIdUpdateDeleteAndFilteredExport();
    void sqliteAddDuplicateProductsDoesNotReplaceExisting();
    void sqliteFilteredExportFailsWhenQueryFails();
    void sqliteSelectionCandidatesFilterBeforeLimit();
    void sqliteLensCandidateFocalRecallSurvivesImageCircleLimit();
    void multilineQuotedCsvImports();
    void sqliteMigrationPreservesLocalCsvRows();
    void sqliteLightCandidatesUseLensFeatureCache();
    void catalogPerformanceGate();
    void diagnosticPruningMatchesExhaustiveRanking();
    void sampleOnlyLightCatalogIsUpgraded();
    void motionExposureAndStrobePreference();
    void dataThroughputAndInterfaceRisk();
    void highResolutionFramePayloadDoesNotOverflow();
    void explicitPixelFormatAffectsPayloadAndBandwidth();
    void cameraEstimatePenalizesInsufficientBandwidth();
    void globalShutterAliasesAreRecognized();
    void lowAngleRingLightActsAsDarkField();
    void directionalDefectLightCoverageUsesLongAxis();
    void hardConstraintsPreferCompatibleResults();
    void hardConstraintFallbackKeepsDiagnosticResult();
    void fixedFocalRejectsInvalidWorkingDistance();
    void fixedLensDofAndDistortionRisk();
    void lightCoverageAffectsScore();
    void telecentricMeasurementWins();
    void defaultRequestHasCompatibleRecommendation();
    void defaultRecommendationsIncludeFixedLensAlternatives();
    void largeFovPrefersFixedFocal();
    void motionPrefersGlobalShutter();
    void reflectiveSurfaceGetsCoaxialOrDome();
    void chineseTextIsUnicode();
    void threeDCameraCatalogLoadsFromResource();
    void threeDCameraUserCatalogPersists();
    void threeDCameraCorruptUserCatalogIsQuarantined();
    void threeDCameraMatcherClassifiesRequirements();
    void threeDMotionSamplingMatchesSpreadsheetExample();
    void threeDMotionSamplingUsesCameraDataAndFlagsRisks();
    void threeDMotionSamplingChecksTriggerExposureAndEncoder();
    void threeDCameraDataDoesNotAffect2DSelection();
    void licenseValidationCoversSignatureMachineAndExpiry();
    void licenseIssuerParsesXmlAndSignsCompatibleKey();
    void licenseIssuerRejectsInvalidInput();
    void machineCodeGenerationIsStable();
    void languageManagerSwitchesAvailableLanguages();
    void generatedDiagnosticsFollowLanguage();
    void selectionUsesExplicitLanguageSnapshot();
    void licenseIssuerErrorsFollowLanguage();
    void invalidTelecentricCsvFails();
    void invalidLightCsvFails();
    void pdfReportWrites();
    void projectReviewNumericAndGeometryRegressions();
    void projectReviewImportPreservesDataAndMetadata();
    void projectReviewBuiltinUpdateAndRecall();
    void projectReviewDiagnosticsAndExports();

private:
    QTemporaryDir m_catalogStorage;
    CatalogRepository m_catalog;
};

namespace {
const char *kTestPrivateKeyXml =
    "<RSAKeyValue><Modulus>5hzYFnHq3/1l3dpJFHV8XBnUejhF6oIE5lVzrDcKm1rq5bLIOTKmRgJmjaa9had4v8w1W3jIX1E/OU5y50KE2YDqHJvAPkiOT7Zpka5U7+pypzLEH5zQfyeaKKgQsXxgoGq3z6DtKv/1mfz5xq0jv5Nr4Ouv/Xep5LNuk8eG7nE=</Modulus><Exponent>AQAB</Exponent><P>9Ml93yaFiAybA4/iVgWhudDbZRLiE9tO042H97yuw5oXeEBF4KNFrsPXT5hjAutCaiMpxXeFbDZlwtleEiLm6w==</P><Q>8KdFsZm6HVMz2nkuB1UHAjhoVwlfQeUvJbZbVjsSTF9Rh5+BeloVC+U/K9C8/C0o1odg5fa3IBxl6gueUaKhEw==</Q><DP>4dmiVCijnWIcCA5SQxIRJHNqaXghtTZsJU55O/8PtBNRQjbzAg9CtLumxZ6RA9lyPqFQ4gujw7Lw8vVBETS4nw==</DP><DQ>0AYYJaSYED9a5HC5zBbA3zd5Yjs0v5ZoQfY3T/vyHliK9mx4FRaHeOfqymo+4tH6qi8OINs6gyRpKH5wlWq6Rw==</DQ><InverseQ>xbFUUJeQ4X+QFiIQCq/YmJcCgbj5Qz/+5gbH1sz1Food2SipieeYQojiMERCAmD8VgSOKVXdKHmzEjeikvBPbA==</InverseQ><D>g3HejZOtEx3wXnYeYK1ryECI+vfCGF8E5X3SgYE/cdbRbzxc2y9vg3ZDlo60m/A6LXU81W99JdWHQ/jn8eoxb+fDvXVnHdGg8sCm/7d9/8MnOEXDRllZbxNE/ICm1k9V9nX1yWQJxPKQ7l3Ify3UEurivZ4e8VB9hDITzoKRKzE=</D></RSAKeyValue>";

bool containsCjk(const QString &value)
{
    for (const QChar ch : value) {
        const ushort u = ch.unicode();
        if ((u >= 0x3400 && u <= 0x9fff) || (u >= 0xf900 && u <= 0xfaff))
            return true;
    }
    return false;
}

class LanguageGuard
{
public:
    LanguageGuard()
        : m_previous(LanguageManager::instance().currentLanguage())
    {
    }

    ~LanguageGuard()
    {
        LanguageManager::instance().setLanguage(m_previous);
    }

    bool setLanguage(const QString &languageCode)
    {
        return LanguageManager::instance().setLanguage(languageCode);
    }

private:
    QString m_previous;
};
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
    for (const CameraSpec &camera : m_catalog.cameras()) {
        QVERIFY2(!camera.sensorFormat.contains(QStringLiteral("CMOS")), qPrintable(camera.model + QStringLiteral(": ") + camera.sensorFormat));
        QVERIFY2(!camera.sensorFormat.contains(QString::fromUtf8("Ã")), qPrintable(camera.model + QStringLiteral(": ") + camera.sensorFormat));
        QVERIFY2(!camera.sensorFormat.contains(QString::fromUtf8("â")), qPrintable(camera.model + QStringLiteral(": ") + camera.sensorFormat));
        QVERIFY2(!camera.sensorFormat.contains(QString::fromUtf8("脳")), qPrintable(camera.model + QStringLiteral(": ") + camera.sensorFormat));
        QVERIFY2(!camera.interfaceType.contains(QString::fromUtf8("Ã")), qPrintable(camera.model + QStringLiteral(": ") + camera.interfaceType));
        QVERIFY2(!camera.interfaceType.contains(QString::fromUtf8("â")), qPrintable(camera.model + QStringLiteral(": ") + camera.interfaceType));
        QVERIFY2(!camera.interfaceType.contains(QString::fromUtf8("脳")), qPrintable(camera.model + QStringLiteral(": ") + camera.interfaceType));
        QVERIFY2(!camera.lensMount.contains(QChar(0x00C3)), qPrintable(camera.model + QStringLiteral(": ") + camera.lensMount));
        QVERIFY2(!camera.lensMount.contains(QChar(0x0097)), qPrintable(camera.model + QStringLiteral(": ") + camera.lensMount));
        QVERIFY2(!camera.lensMount.contains(QChar(0x00D7)), qPrintable(camera.model + QStringLiteral(": ") + camera.lensMount));
        QVERIFY2(!camera.lensMount.contains(QChar(0x8133)), qPrintable(camera.model + QStringLiteral(": ") + camera.lensMount));
        QVERIFY2(!camera.sensorFormat.contains(QStringLiteral("''")), qPrintable(camera.model + QStringLiteral(": ") + camera.sensorFormat));
        QVERIFY2(camera.colorMode != QString::fromUtf8("黑白"), qPrintable(camera.model));
    }
    for (const LensSpec &lens : m_catalog.lenses()) {
        QVERIFY2(!lens.manufacturer.trimmed().isEmpty(), qPrintable(lens.model));
        QVERIFY2(!lens.lensMount.contains(QChar(0x00C3)), qPrintable(lens.model + QStringLiteral(": ") + lens.lensMount));
        QVERIFY2(!lens.lensMount.contains(QChar(0x0097)), qPrintable(lens.model + QStringLiteral(": ") + lens.lensMount));
        QVERIFY2(!lens.lensMount.contains(QChar(0x00D7)), qPrintable(lens.model + QStringLiteral(": ") + lens.lensMount));
        QVERIFY2(!lens.lensMount.contains(QChar(0x8133)), qPrintable(lens.model + QStringLiteral(": ") + lens.lensMount));
    }
    int mvotemLightCount = 0;
    for (const LightSpec &light : m_catalog.lights()) {
        QVERIFY2(!light.manufacturer.trimmed().isEmpty(), qPrintable(light.model));
        if (light.manufacturer == QString::fromUtf8("慕藤光"))
            ++mvotemLightCount;
    }
    QVERIFY(mvotemLightCount >= 180);
}

void SelectionEngineTest::calculationAssistantEstimatesRequirements()
{
    SelectionRequest request;
    request.objectWidthMm = 20.0;
    request.objectHeightMm = 10.0;
    request.placementMarginMm = 2.0;
    request.minFeatureUm = 50.0;
    request.measurementToleranceUm = 25.0;
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

    SelectionRequest broadMeasurement = request;
    broadMeasurement.measurementToleranceUm = 100.0;
    broadMeasurement.heightVariationMm = 0.0;
    QVERIFY(!CalculationAssistant::estimateRequirement(broadMeasurement).telecentricPreferred);

    const QVector<CameraCalculationEstimate> estimates = CalculationAssistant::estimateCameras(request, m_catalog.cameras(), 5);
    QVERIFY(!estimates.isEmpty());
    QVERIFY(estimates.first().fixedFocalLengthMm > 0.0);
    QVERIFY(estimates.first().sensorDiagonalMm > 0.0);
}

void SelectionEngineTest::measurementToleranceUsesPixelBudget()
{
    SelectionRequest request;
    request.objectWidthMm = 20.0;
    request.objectHeightMm = 10.0;
    request.placementMarginMm = 2.0;
    request.minFeatureUm = 1000.0;
    request.measurementToleranceUm = 10.0;
    request.detectionType = DetectionType::Measurement;

    const RequirementEstimate requirement = CalculationAssistant::estimateRequirement(request);
    QCOMPARE(requirement.targetObjectPixelUm, 2.0);
    QCOMPARE(requirement.requiredResolutionX, 12000);
    QCOMPARE(requirement.requiredResolutionY, 7000);
    QVERIFY(requirement.telecentricPreferred);

    request.measurementToleranceUm = 1.0;
    const auto strict = CalculationAssistant::estimateRequirement(request);
    QCOMPARE(strict.targetObjectPixelUm, 0.2);
    QCOMPARE(strict.requiredResolutionX, 120000);
    QCOMPARE(strict.requiredResolutionY, 70000);
    QCOMPARE(strict.requiredMegapixels, 8400.0);
}

void SelectionEngineTest::candidateValidationMatchesAssistantAndPreservesUnknowns()
{
    SelectionRequest request;
    CameraSpec camera;
    camera.model = "CAM";
    camera.resolutionX = camera.resolutionY = 5120;
    camera.pixelSizeUm = 2.5;
    camera.maxFps = 100;
    camera.colorMode = "Mono8";
    camera.interfaceType = "CoaXPress";
    camera.bandwidthMBps = 2400;
    camera.lensMount = "C";
    LensSpec lens;
    lens.model = "像圈不足";
    lens.lensType = LensType::ObjectTelecentric;
    lens.lensMount = "C";
    lens.pmag = 0.5;
    lens.imageCircleMm = lens.maxSensorDiagonalMm = 11;
    lens.nominalWorkingDistanceMm = 110;
    lens.workingDistanceToleranceMm = 3;
    lens.dofMm = 6.4;
    LightSpec light;
    light.activeWidthMm = light.activeHeightMm = 100;
    SelectionEngine engine;
    auto assistant = CalculationAssistant::estimateLenses(request, camera, {lens});
    auto result = engine.select(request, {camera}, {lens}, {light});
    QVERIFY(assistant.first().checks.failed());
    QVERIFY(!result.first().hardConstraintsPassed);
    for (int i = 0; i <= static_cast<int>(CandidateCheck::DepthOfField); ++i)
        QCOMPARE(assistant.first().checks[static_cast<CandidateCheck>(i)], result.first().checks[static_cast<CandidateCheck>(i)]);
    LensSpec compatible = lens;
    compatible.model = "完整覆盖";
    compatible.imageCircleMm = compatible.maxSensorDiagonalMm = 20;
    assistant = CalculationAssistant::estimateLenses(request, camera, {lens, compatible});
    QCOMPARE(assistant.first().lens.model, compatible.model);
    QVERIFY(!assistant.first().checks.failed());
    compatible.dofMm = 0.9;
    compatible.dofConditionsConfirmed = true;
    result = engine.select(request, {camera}, {compatible}, {light});
    QVERIFY(!result.first().hardConstraintsPassed);
    QCOMPARE(result.first().checks[CandidateCheck::DepthOfField], CandidateCheckState::Failed);
    compatible.dofMm = 0;
    compatible.workingDistanceToleranceMm = 0;
    result = engine.select(request, {camera}, {compatible}, {light});
    QVERIFY(result.first().hardConstraintsPassed);
    QVERIFY(result.first().checks.unknown());
    QCOMPARE(result.first().checks[CandidateCheck::WorkingDistance], CandidateCheckState::Unknown);
    QCOMPARE(result.first().checks[CandidateCheck::DepthOfField], CandidateCheckState::Unknown);

    camera.resolutionX = 9344; camera.resolutionY = 7000; camera.pixelSizeUm = 3.2;
    compatible.lensType = LensType::FixedFocal;
    compatible.focalLengthMm = 50;
    compatible.imageCircleMm = compatible.maxSensorDiagonalMm = 46;
    compatible.dofMm = 10;
    compatible.minWorkingDistanceMm = 0;
    result = engine.select(request, {camera}, {compatible}, {light});
    QVERIFY(result.first().hardConstraintsPassed);
    QCOMPARE(result.first().checks[CandidateCheck::WorkingDistance], CandidateCheckState::Unknown);
    assistant = CalculationAssistant::estimateLenses(request, camera, {compatible});
    QCOMPARE(assistant.first().checks[CandidateCheck::WorkingDistance], CandidateCheckState::Unknown);
    QVERIFY(!assistant.first().workingDistanceOk);
}

void SelectionEngineTest::candidateRecallKeepsCompatibleCameraBeyond96()
{
    SelectionRequest request;
    CameraSpec camera;
    camera.resolutionX = camera.resolutionY = 4800;
    camera.pixelSizeUm = 5;
    camera.maxFps = 100;
    camera.colorMode = "Mono8";
    camera.interfaceType = "CoaXPress";
    camera.bandwidthMBps = 10000;
    camera.lensMount = "C";
    QVector<CameraSpec> cameras;
    for (int i = 0; i < 96; ++i) {
        camera.model = QString("不兼容%1").arg(i);
        cameras.append(camera);
    }
    camera.model = "唯一可行相机";
    camera.lensMount = "M58";
    camera.resolutionX = camera.resolutionY = 5000;
    cameras.append(camera);
    LensSpec lens;
    lens.lensType = LensType::ObjectTelecentric;
    lens.lensMount = "M58";
    lens.pmag = 1;
    lens.imageCircleMm = lens.maxSensorDiagonalMm = 40;
    lens.nominalWorkingDistanceMm = 110;
    lens.workingDistanceToleranceMm = 5;
    lens.dofMm = 20;
    LightSpec light;
    light.activeWidthMm = light.activeHeightMm = 100;
    SelectionEngine engine;
    const auto results = engine.select(request, cameras, {lens}, {light}, 20);
    QVERIFY(!results.isEmpty());
    QVERIFY(results.first().hardConstraintsPassed);
    QCOMPARE(results.first().camera.model, camera.model);
}

void SelectionEngineTest::calculationAuditRegressions()
{
    PureCalculationInput input;
    input.request.objectWidthMm = input.request.objectHeightMm = 10.0;
    input.request.placementMarginMm = input.request.heightVariationMm = 0.0;
    input.request.requiredFps = 60.0;
    input.camera.resolutionX = 2448;
    input.camera.resolutionY = 2048;
    input.camera.pixelSizeUm = 3.45;
    input.camera.colorMode = QStringLiteral("Mono12p");
    input.camera.bandwidthMBps = 1000.0;
    input.camera.maxFps = 10.0;
    input.lens.focalLengthMm = 16.0;
    auto result = CalculationAssistant::estimatePure(input);
    QCOMPARE(result.geometryStatus, CalculationStatus::Passed);
    QCOMPARE(result.samplingStatus, CalculationStatus::Failed);
    QCOMPARE(result.fpsStatus, CalculationStatus::Failed);
    QVERIFY(result.cameraObjectPixelSizeUm < result.requirement.targetObjectPixelUm);
    QVERIFY(result.lensObjectPixelSizeUm > 20.0);
    QVERIFY(result.risks.join(QLatin1Char(';')).contains(QStringLiteral("实际物方像素")));
    input.camera.maxFps = 100.0;
    QCOMPARE(CalculationAssistant::estimatePure(input).fpsStatus, CalculationStatus::Passed);
    input.camera.maxFps = 0.0;
    QCOMPARE(CalculationAssistant::estimatePure(input).fpsStatus, CalculationStatus::Unknown);
    input.request.workingDistanceMm = input.lens.focalLengthMm;
    QCOMPARE(CalculationAssistant::estimatePure(input).geometryStatus, CalculationStatus::Invalid);
    input.telecentricMode = true;
    input.lens.pmag = 0.2;
    result = CalculationAssistant::estimatePure(input);
    QCOMPARE(result.telecentricErrorStatus, CalculationStatus::Unknown);

    CameraSpec camera;
    camera.resolutionX = 2000;
    camera.resolutionY = 1000;
    camera.colorMode = QStringLiteral("Mono12");
    QCOMPARE(SelectionEngine::framePayloadMB(camera), 4.0);
    camera.colorMode = QStringLiteral("Mono12p");
    QCOMPARE(SelectionEngine::framePayloadMB(camera), 3.0);
    QCOMPARE(SelectionEngine::storagePerHourGB(camera, 10.0), 108.0);
    camera.colorMode = QStringLiteral("BayerRG12");
    QCOMPARE(SelectionEngine::framePayloadMB(camera), 4.0);
    camera.colorMode = QStringLiteral("BayerRG12p");
    QCOMPARE(SelectionEngine::framePayloadMB(camera), 3.0);
    camera.resolutionX = 3;
    camera.resolutionY = 2;
    QCOMPARE(SelectionEngine::framePayloadMB(camera), 10.0 / 1000000.0);
}

void SelectionEngineTest::parameterOpticsSolvesBothModelsAndAxes()
{
    using namespace Parameters;
    OpticsInput input;
    QCOMPARE(optics(input).status, CalculationStatus::Unknown);
    QVERIFY(!optics(input).fovWidthMm);
    input.sensor = {2448.0, 2048.0, 3.45};
    input.distanceMm = 300.0;
    input.targetFovWidthMm = 120.0;
    input.targetFovHeightMm = 80.0;
    auto result = optics(input);
    QCOMPARE(result.status, CalculationStatus::Passed);
    QVERIFY(qAbs(*result.focalLengthMm - 21.114) < 1e-8);
    QVERIFY(qAbs(*result.fovWidthMm - 120.0) < 1e-8);
    QVERIFY(*result.fovHeightMm > 80.0);
    input.solve = OpticsSolve::FieldOfView;
    input.focalLengthMm = 20.0;
    result = optics(input);
    QVERIFY(qAbs(*result.fovWidthMm - 126.684) < 1e-8);
    QVERIFY(qAbs(*result.fovHeightMm - 105.984) < 1e-8);
    QVERIFY(qAbs(*result.objectPixelUm - 51.75) < 1e-8);
    QCOMPARE(checkUpperBound(result.objectPixelUm, 50.0), CalculationStatus::Failed);
    input.solve = OpticsSolve::Distance;
    result = optics(input);
    QVERIFY(qAbs(*result.distanceMm - 284.171639670361) < 1e-6);
    for (OpticsModel model : {OpticsModel::Paraxial, OpticsModel::ThinLens}) {
        input.model = model;
        input.targetFovWidthMm = 80.0; input.targetFovHeightMm = 120.0;
        input.solve = OpticsSolve::FocalLength;
        const auto focal = optics(input);
        QCOMPARE(focal.status, CalculationStatus::Passed);
        QCOMPARE(focal.coverage, CalculationStatus::Passed);
        QVERIFY(qAbs(*focal.fovHeightMm - 120.0) < 1e-7);
        input.focalLengthMm = focal.focalLengthMm;
        input.solve = OpticsSolve::Distance;
        QVERIFY(qAbs(*optics(input).distanceMm - 300.0) < 1e-7);
    }
    input.solve = OpticsSolve::FieldOfView;
    input.model = OpticsModel::ThinLens;
    input.distanceMm = input.focalLengthMm;
    result = optics(input);
    QCOMPARE(result.status, CalculationStatus::Invalid);
    QVERIFY(!result.objectPixelUm && !result.fovWidthMm);
    input.distanceMm.reset();
    QCOMPARE(optics(input).status, CalculationStatus::Unknown);
}

void SelectionEngineTest::parameterSamplingAndCalibration()
{
    using namespace Parameters;
    SamplingInput input;
    input.fovWidthMm = 120.0; input.fovHeightMm = 80.0;
    input.featureUm = 200.0; input.pixelsPerFeature = 4.0;
    auto result = sampling(input);
    QCOMPARE(result.status, CalculationStatus::Passed);
    QCOMPARE(*result.targetObjectPixelUm, 50.0);
    QCOMPARE(*result.requiredResolutionX, 2400.0);
    QCOMPARE(*result.requiredResolutionY, 1600.0);
    input.measurementBudget = true;
    QCOMPARE(sampling(input).status, CalculationStatus::Unknown);
    input.toleranceUm = 1.0; input.pixelsPerTolerance = 5.0;
    result = sampling(input);
    QCOMPARE(*result.targetObjectPixelUm, 0.2);
    QCOMPARE(*result.requiredResolutionX, 600000.0);
    input.featureUm.reset(); input.pixelsPerFeature.reset();
    QCOMPARE(sampling(input).status, CalculationStatus::Passed);
    input.solve = SamplingSolve::Actual;
    input.sensor = {2448.0, 2048.0, {}};
    result = sampling(input);
    QCOMPARE(result.status, CalculationStatus::Passed);
    QVERIFY(qAbs(*result.objectPixelXUm - 49.0196078431) < 1e-8);
    QCOMPARE(*result.objectPixelYUm, 39.0625);
    input.solve = SamplingSolve::Calibration;
    input.calibrationLengthMm = 10.0; input.calibrationPixels = 200.0;
    QCOMPARE(*sampling(input).calibratedPixelUm, 50.0);
    input.calibrationPixels = 0.0;
    QCOMPARE(sampling(input).status, CalculationStatus::Invalid);
    QVERIFY(!sampling(input).calibratedPixelUm);
    input.calibrationPixels.reset();
    QCOMPARE(sampling(input).status, CalculationStatus::Unknown);
}

void SelectionEngineTest::parameterTelecentricRangeAndMotion()
{
    using namespace Parameters;
    TelecentricInput input;
    input.sensor = {2448.0, 2048.0, 3.45};
    input.targetFovWidthMm = 120.0; input.targetFovHeightMm = 80.0; input.targetObjectPixelUm = 50.0;
    auto result = telecentric(input);
    QCOMPARE(result.status, CalculationStatus::Passed);
    QVERIFY(qAbs(*result.minMagnification - 0.069) < 1e-9);
    QVERIFY(qAbs(*result.maxMagnification - 0.07038) < 1e-9);
    input.targetObjectPixelUm = 40.0;
    result = telecentric(input);
    QCOMPARE(result.status, CalculationStatus::Failed);
    QVERIFY(result.minMagnification && result.maxMagnification);
    QVERIFY(!result.magnification && !result.fovWidthMm);
    input.solve = TelecentricSolve::FieldOfView; input.magnification = 0.2;
    result = telecentric(input);
    QVERIFY(qAbs(*result.fovWidthMm - 42.228) < 1e-8);
    QVERIFY(qAbs(*result.objectPixelUm - 17.25) < 1e-8);

    ExposureInput motion;
    motion.objectPixelUm = 50.0; motion.speedMmS = 500.0; motion.blurPixels = 0.5;
    QCOMPARE(*exposure(motion).exposureUs, 50.0);
    motion.solve = ExposureSolve::Blur; motion.exposureUs = 50.0;
    QCOMPARE(*exposure(motion).blurPixels, 0.5);
    motion.solve = ExposureSolve::Speed;
    QCOMPARE(*exposure(motion).speedMmS, 500.0);
    motion.solve = ExposureSolve::Exposure; motion.speedMmS = 0.0;
    QCOMPARE(exposure(motion).status, CalculationStatus::NotApplicable);
    QVERIFY(!exposure(motion).exposureUs);
    motion.speedMmS = -1.0;
    QCOMPARE(exposure(motion).status, CalculationStatus::Invalid);
    motion.speedMmS.reset();
    QCOMPARE(exposure(motion).status, CalculationStatus::Unknown);
}

void SelectionEngineTest::parameterTransferUsesRoiPackingAndStorageFormat()
{
    using namespace Parameters;
    TransferInput input;
    input.width = 2448; input.height = 2048; input.fps = 30; input.pixelFormat = "Mono8";
    auto result = transfer(input);
    QCOMPARE(result.status, CalculationStatus::Passed);
    QCOMPARE(*result.frameBytes, 5013504.0);
    QVERIFY(qAbs(*result.transportMBps - 150.40512) < 1e-8);
    QCOMPARE(result.capacityStatus, CalculationStatus::Unknown);
    input.width = 2000; input.height = 1000; input.pixelFormat = "Mono12";
    input.cameraCount = 2; input.hours = 2; input.overheadPercent = 10; input.capacityMBps = 250;
    result = transfer(input);
    QCOMPARE(*result.frameBytes, 4000000.0);
    QCOMPARE(*result.payloadMBps, 240.0);
    QVERIFY(qAbs(*result.transportMBps - 264.0) < 1e-8);
    QCOMPARE(*result.storageGB, 1728.0);
    QCOMPARE(result.capacityStatus, CalculationStatus::Failed);
    input.pixelFormat = "Mono12p";
    QCOMPARE(*transfer(input).frameBytes, 3000000.0);
    input.storageFormat = "RGB8";
    result = transfer(input);
    QCOMPARE(*result.storageGB, 2592.0);
    QCOMPARE(*result.frameBytes, 3000000.0);
    input.roiWidth = 1000; input.roiHeight = 500;
    QCOMPARE(*transfer(input).frameBytes, 750000.0);
    input.roiHeight = 1001;
    QCOMPARE(transfer(input).status, CalculationStatus::Invalid);
    QVERIFY(!transfer(input).frameBytes);
    input.roiHeight = 500; input.pixelFormat.clear();
    QCOMPARE(transfer(input).status, CalculationStatus::Unknown);
    input.pixelFormat = "UnknownFormat";
    QCOMPARE(transfer(input).status, CalculationStatus::Invalid);
    input.pixelFormat = "Mono8"; input.cameraCount = 1.5;
    QCOMPARE(transfer(input).status, CalculationStatus::Invalid);
}

void SelectionEngineTest::parameterSystemCheckPreservesUnknowns()
{
    using namespace Parameters;
    SystemInput input;
    input.sensor = {2448.0, 2048.0, 3.45};
    input.model = OpticsModel::ThinLens;
    input.distanceMm = 110.0; input.focalLengthMm = 16.0;
    input.targetFovWidthMm = input.targetFovHeightMm = 10.0;
    input.targetObjectPixelUm = 5.0; input.fps = 60.0; input.maxFps = 10.0;
    auto result = checkSystem(input);
    const auto find = [](const SystemResult &result, const QString &key) {
        for (const auto &check : result.checks) if (check.key == key) return check;
        return CheckItem();
    };
    QCOMPARE(result.status, CalculationStatus::Failed);
    QCOMPARE(find(result, "samplingX").status, CalculationStatus::Failed);
    QCOMPARE(find(result, "samplingY").status, CalculationStatus::Failed);
    QCOMPARE(find(result, "fps").status, CalculationStatus::Failed);
    QCOMPARE(find(result, "imageCircle").status, CalculationStatus::Unknown);
    QCOMPARE(find(result, "bandwidth").status, CalculationStatus::Unknown);
    input.maxFps = 0.0;
    QCOMPARE(find(checkSystem(input), "fps").status, CalculationStatus::Unknown);
    input.distanceMm = 16.0;
    result = checkSystem(input);
    QCOMPARE(find(result, "samplingX").status, CalculationStatus::Invalid);
    QVERIFY(!result.actualFovWidthMm && !result.objectPixelXUm);
    input.measuredFov = true; input.measuredFovWidthMm = 10.0; input.measuredFovHeightMm = 10.0;
    result = checkSystem(input);
    QCOMPARE(find(result, "samplingX").status, CalculationStatus::Passed);
    QCOMPARE(find(result, "samplingY").status, CalculationStatus::Passed);
    input.telecentric = true; input.heightVariationMm = 2.0; input.measurementToleranceUm = 25.0;
    result = checkSystem(input);
    QCOMPARE(find(result, "telecentricity").status, CalculationStatus::Unknown);
    QVERIFY(!find(result, "telecentricity").actual);
    input.telecentricityDeg = 0.0;
    result = checkSystem(input);
    QCOMPARE(find(result, "telecentricity").status, CalculationStatus::Passed);
    QCOMPARE(*find(result, "telecentricity").actual, 0.0);
    input.dofMm = 3.0;
    QCOMPARE(find(checkSystem(input), "dof").status, CalculationStatus::Unknown);
    input.dofConditionsConfirmed = true;
    QCOMPARE(find(checkSystem(input), "dof").status, CalculationStatus::Passed);
    input.dofMm = 1.0;
    QCOMPARE(find(checkSystem(input), "dof").status, CalculationStatus::Failed);
}

void SelectionEngineTest::nonMeasurementToleranceDoesNotTightenSampling()
{
    SelectionRequest request;
    request.objectWidthMm = 30.0;
    request.objectHeightMm = 20.0;
    request.placementMarginMm = 0.0;
    request.minFeatureUm = 300.0;
    request.measurementToleranceUm = 1.0;

    request.detectionType = DetectionType::DefectInspection;
    QCOMPARE(SelectionEngine::targetObjectPixelUm(request), 100.0);
    QCOMPARE(CalculationAssistant::estimateRequirement(request).targetObjectPixelUm, 100.0);

    request.detectionType = DetectionType::OcrCode;
    QCOMPARE(SelectionEngine::targetObjectPixelUm(request), 75.0);
    QCOMPARE(CalculationAssistant::estimateRequirement(request).targetObjectPixelUm, 75.0);

    request.detectionType = DetectionType::Positioning;
    QCOMPARE(SelectionEngine::targetObjectPixelUm(request), 75.0);
    QCOMPARE(CalculationAssistant::estimateRequirement(request).targetObjectPixelUm, 75.0);
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
    QVERIFY2(lenses.first().mountOk, qPrintable(lenses.first().lens.model));
    if (!lenses.first().samplingOk) {
        const QString risks = lenses.first().risks.join(QStringLiteral(";"));
        QVERIFY2(risks.contains(QString::fromUtf8("物方像素")), qPrintable(risks));
    }

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
    input.request.motionMode = MotionMode::Continuous;
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

void SelectionEngineTest::telecentricMissingCatalogDataIsRisk()
{
    SelectionRequest request;
    request.objectWidthMm = 5.0;
    request.objectHeightMm = 5.0;
    request.placementMarginMm = 0.0;
    request.minFeatureUm = 500.0;
    request.measurementToleranceUm = 100.0;
    request.workingDistanceMm = 110.0;
    request.heightVariationMm = 2.0;
    request.requiredFps = 10.0;
    request.detectionType = DetectionType::Measurement;
    request.surfaceType = SurfaceType::Matte;
    request.reflective = false;

    CameraSpec camera;
    camera.model = QStringLiteral("TEST-CAM");
    camera.manufacturer = QStringLiteral("Test");
    camera.resolutionX = 2448;
    camera.resolutionY = 2048;
    camera.pixelSizeUm = 3.45;
    camera.shutterType = QStringLiteral("Global");
    camera.maxFps = 30.0;
    camera.interfaceType = QStringLiteral("USB3");
    camera.bandwidthMBps = 500.0;
    camera.bitDepth = 8.0;
    camera.lensMount = QStringLiteral("C");

    LensSpec lens;
    lens.model = QStringLiteral("TEST-MISSING-WD-DOF");
    lens.manufacturer = QStringLiteral("Test");
    lens.lensType = LensType::ObjectTelecentric;
    lens.lensMount = QStringLiteral("C");
    lens.pmag = 0.2;
    lens.nominalWorkingDistanceMm = 110.0;
    lens.workingDistanceToleranceMm = 0.0;
    lens.dofMm = 0.0;
    lens.telecentricityDeg = 0.1;
    lens.distortionPercent = 0.01;
    lens.imageCircleMm = 12.0;
    lens.maxSensorDiagonalMm = 12.0;
    lens.megapixelRating = 12.0;
    lens.recommendedMinPixelUm = 3.45;

    LightSpec light;
    light.model = QStringLiteral("TEST-TBL");
    light.manufacturer = QStringLiteral("Test");
    light.lightType = LightType::TelecentricBacklight;
    light.mode = QStringLiteral("Strobe");
    light.activeWidthMm = 60.0;
    light.activeHeightMm = 60.0;

    SelectionEngine engine;
    QVector<CameraSpec> cameras;
    cameras.append(camera);
    QVector<LensSpec> lenses;
    lenses.append(lens);
    QVector<LightSpec> lights;
    lights.append(light);
    const QVector<SelectionResult> results = engine.select(request, cameras, lenses, lights, 1);
    QCOMPARE(results.size(), 1);
    QVERIFY(results.first().hardConstraintsPassed);
    const QString resultRisks = results.first().score.risks.join(QStringLiteral(";"));
    QVERIFY2(resultRisks.contains(QStringLiteral("WD")), qPrintable(resultRisks));
    QVERIFY2(resultRisks.contains(QStringLiteral("DOF")), qPrintable(resultRisks));

    const QVector<LensCalculationEstimate> estimates = CalculationAssistant::estimateLenses(request, camera, lenses, 1);
    QCOMPARE(estimates.size(), 1);
    QVERIFY(!estimates.first().workingDistanceOk);
    QVERIFY(!estimates.first().dofOk);
    const QString estimateRisks = estimates.first().risks.join(QStringLiteral(";"));
    QVERIFY2(estimateRisks.contains(QStringLiteral("WD")), qPrintable(estimateRisks));
    QVERIFY2(estimateRisks.contains(QStringLiteral("DOF")), qPrintable(estimateRisks));
}

void SelectionEngineTest::missingTelecentricityIsRisk()
{
    SelectionRequest request;
    request.objectWidthMm = 5.0;
    request.objectHeightMm = 5.0;
    request.placementMarginMm = 0.0;
    request.minFeatureUm = 500.0;
    request.measurementToleranceUm = 100.0;
    request.workingDistanceMm = 110.0;
    request.heightVariationMm = 2.0;
    request.requiredFps = 10.0;
    request.detectionType = DetectionType::Measurement;

    CameraSpec camera;
    camera.model = QStringLiteral("TEST-CAM");
    camera.manufacturer = QStringLiteral("Test");
    camera.resolutionX = 2448;
    camera.resolutionY = 2048;
    camera.pixelSizeUm = 3.45;
    camera.shutterType = QStringLiteral("Global");
    camera.maxFps = 30.0;
    camera.interfaceType = QStringLiteral("USB3");
    camera.bandwidthMBps = 500.0;
    camera.bitDepth = 8.0;
    camera.lensMount = QStringLiteral("C");

    LensSpec lens;
    lens.model = QStringLiteral("TEST-MISSING-TELECENTRICITY");
    lens.manufacturer = QStringLiteral("Test");
    lens.lensType = LensType::ObjectTelecentric;
    lens.lensMount = QStringLiteral("C");
    lens.pmag = 0.2;
    lens.nominalWorkingDistanceMm = 110.0;
    lens.workingDistanceToleranceMm = 5.0;
    lens.dofMm = 5.0;
    lens.telecentricityDeg = -1.0;
    lens.distortionPercent = 0.01;
    lens.imageCircleMm = 12.0;
    lens.maxSensorDiagonalMm = 12.0;
    lens.megapixelRating = 12.0;
    lens.recommendedMinPixelUm = 3.45;

    LightSpec light;
    light.model = QStringLiteral("TEST-TBL");
    light.manufacturer = QStringLiteral("Test");
    light.lightType = LightType::TelecentricBacklight;
    light.mode = QStringLiteral("Strobe");
    light.activeWidthMm = 60.0;
    light.activeHeightMm = 60.0;

    SelectionEngine engine;
    const QVector<SelectionResult> results = engine.select(request, {camera}, {lens}, {light}, 1);
    QCOMPARE(results.size(), 1);
    QVERIFY(results.first().hardConstraintsPassed);
    QCOMPARE(results.first().residualTelecentricErrorUm, 0.0);
    const QString resultRisks = results.first().score.risks.join(QStringLiteral(";"));
    QVERIFY2(!resultRisks.isEmpty(), qPrintable(resultRisks));

    const QVector<LensCalculationEstimate> estimates = CalculationAssistant::estimateLenses(request, camera, {lens}, 1);
    QCOMPARE(estimates.size(), 1);
    QCOMPARE(estimates.first().residualTelecentricErrorUm, 0.0);
    const QString estimateRisks = estimates.first().risks.join(QStringLiteral(";"));
    QVERIFY2(estimateRisks.contains(QString::fromUtf8("远心度")), qPrintable(estimateRisks));

    PureCalculationInput input;
    input.request = request;
    input.camera = camera;
    input.lens = lens;
    input.light = light;
    input.telecentricMode = true;
    const PureCalculationResult pure = CalculationAssistant::estimatePure(input);
    QCOMPARE(pure.residualTelecentricErrorUm, 0.0);
    const QString pureRisks = pure.risks.join(QStringLiteral(";"));
    QVERIFY2(pureRisks.contains(QString::fromUtf8("远心度")), qPrintable(pureRisks));
}

void SelectionEngineTest::lensTypeParsingRecognizesTelecentricAliases()
{
    QCOMPARE(lensTypeFromString(QStringLiteral("Telecentric")), LensType::ObjectTelecentric);
    QCOMPARE(lensTypeFromString(QStringLiteral("Object-side telecentric")), LensType::ObjectTelecentric);
    QCOMPARE(lensTypeFromString(QStringLiteral("Bi-telecentric")), LensType::BiTelecentric);
    QCOMPARE(lensTypeFromString(QStringLiteral("Non-telecentric fixed focal")), LensType::FixedFocal);
    QCOMPARE(lensTypeFromString(QString::fromUtf8("\350\277\234\345\277\203")), LensType::ObjectTelecentric);
    QCOMPARE(lensTypeFromString(QString::fromUtf8("\345\217\214\350\277\234\345\277\203")), LensType::BiTelecentric);
}

void SelectionEngineTest::lensMountCompatibilityIsConservative()
{
    QVERIFY(mountsCompatible(QStringLiteral("C"), QStringLiteral("C")));
    QVERIFY(mountsCompatible(QStringLiteral("C-mount"), QStringLiteral("C")));
    QVERIFY(mountsCompatible(QStringLiteral("M72*0.75, flange back length 19.55 mm"), QStringLiteral("M72 x P0.75")));
    QVERIFY(!mountsCompatible(QStringLiteral("M72"), QStringLiteral("M72 x P0.75")));
    QVERIFY(!mountsCompatible(QStringLiteral("M72 x 1"), QStringLiteral("M72 x P0.75")));
    QVERIFY(!mountsCompatible(QString(), QStringLiteral("C")));
    QVERIFY(!mountsCompatible(QStringLiteral("C"), QString()));
    QVERIFY(!mountsCompatible(QStringLiteral("None"), QStringLiteral("C")));
    QVERIFY(!mountsCompatible(QStringLiteral("M42"), QStringLiteral("C")));
}

void SelectionEngineTest::fixedFocalTargetUsesLimitingAxis()
{
    SelectionRequest request;
    request.objectWidthMm = 20.0;
    request.objectHeightMm = 80.0;
    request.placementMarginMm = 0.0;
    request.minFeatureUm = 500.0;
    request.measurementToleranceUm = 500.0;
    request.workingDistanceMm = 100.0;
    request.heightVariationMm = 0.0;
    request.requiredFps = 10.0;
    request.detectionType = DetectionType::DefectInspection;
    request.surfaceType = SurfaceType::Matte;
    request.reflective = false;

    CameraSpec camera;
    camera.model = QStringLiteral("TALL-FOV-CAM");
    camera.manufacturer = QStringLiteral("Test");
    camera.resolutionX = 4000;
    camera.resolutionY = 2000;
    camera.pixelSizeUm = 5.0;
    camera.colorMode = QStringLiteral("Mono");
    camera.shutterType = QStringLiteral("Global");
    camera.maxFps = 60.0;
    camera.interfaceType = QStringLiteral("USB3");
    camera.bandwidthMBps = 500.0;
    camera.bitDepth = 8.0;
    camera.lensMount = QStringLiteral("C");

    const double expectedFocalLength = CalculationAssistant::estimatedFixedFocalLengthMm(request, camera);

    LensSpec lens;
    lens.model = QStringLiteral("LIMITING-AXIS-LENS");
    lens.manufacturer = QStringLiteral("Test");
    lens.lensType = LensType::FixedFocal;
    lens.lensMount = QStringLiteral("C");
    lens.focalLengthMm = expectedFocalLength;
    lens.minWorkingDistanceMm = 20.0;
    lens.imageCircleMm = 25.0;
    lens.megapixelRating = 10.0;
    lens.recommendedMinPixelUm = 5.0;
    lens.fNumber = 4.0;

    LightSpec light;
    light.model = QStringLiteral("LIGHT");
    light.manufacturer = QStringLiteral("Test");
    light.lightType = LightType::Bar;
    light.mode = QStringLiteral("Continuous");
    light.activeWidthMm = 200.0;
    light.activeHeightMm = 120.0;

    SelectionEngine engine;
    const QVector<SelectionResult> results = engine.select(request, {camera}, {lens}, {light}, 1);
    QVERIFY(!results.isEmpty());
    QVERIFY(results.first().hardConstraintsPassed);
    QVERIFY(qAbs(results.first().estimatedFocalLengthMm - expectedFocalLength) < 0.001);
}

void SelectionEngineTest::nonMeasurementRequirementsDoNotForceTelecentric()
{
    SelectionRequest request;
    request.objectWidthMm = 10.0;
    request.objectHeightMm = 10.0;
    request.placementMarginMm = 1.0;
    request.minFeatureUm = 5.0;
    request.measurementToleranceUm = 10.0;
    request.heightVariationMm = 5.0;
    request.detectionType = DetectionType::DefectInspection;

    QVERIFY(!CalculationAssistant::estimateRequirement(request).telecentricPreferred);
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

void SelectionEngineTest::sqliteInitializeDatabaseKeepsCompatibilitySnapshotsLazy()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());

    CatalogRepository repo;
    repo.setStorageDirectory(storage.path());
    QString error;
    QVERIFY2(repo.initializeDatabase(&error), qPrintable(error));
    QVERIFY(repo.cameras().isEmpty());
    QVERIFY(repo.lenses().isEmpty());
    QVERIFY(repo.lights().isEmpty());
    QVERIFY(repo.productCount(CatalogDomain::Camera, &error) > 0);
    QVERIFY(repo.productCount(CatalogDomain::Lens, &error) > 0);
    QVERIFY(repo.productCount(CatalogDomain::Light, &error) > 0);

    CatalogQuery query;
    query.limit = 1;
    const CatalogPageResult<CameraSpec> page = repo.queryCameras(query, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(page.items.size(), 1);
    CameraSpec camera;
    QVERIFY2(repo.cameraById(page.ids.first(), &camera, &error), qPrintable(error));
    QCOMPARE(camera.model, page.items.first().model);

    SelectionService service(&repo);
    const QVector<SelectionResult> results = service.select(SelectionRequest(), 5, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(!results.isEmpty());
    QVERIFY(repo.cameras().isEmpty());
    QVERIFY(repo.lenses().isEmpty());
    QVERIFY(repo.lights().isEmpty());
}

void SelectionEngineTest::sqliteInitializeDatabaseRemovesDeletedBuiltIns()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());

    CatalogRepository repo;
    repo.setStorageDirectory(storage.path());
    QString error;
    QVERIFY2(repo.initializeDatabase(&error), qPrintable(error));

    const QString connectionName = QStringLiteral("sqliteInitializeDatabaseRemovesDeletedBuiltIns");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(QDir(storage.path()).filePath(QStringLiteral("catalog.db")));
    QVERIFY2(db.open(), qPrintable(db.lastError().text()));

    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    const QString insertCameraSql = QStringLiteral(
        "INSERT INTO camera_products (model, manufacturer, manufacturer_key, model_key, resolution_x, resolution_y,"
        " pixel_size_um, sensor_format, color_mode, shutter_type, max_fps, interface, bandwidth_mbps, bit_depth,"
        " dynamic_range_db, lens_mount, search_text, source_kind, source_version, created_at, updated_at)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    const auto insertCameraRow = [&](const QString &model, const QString &manufacturer, const QString &sourceKind) {
        QSqlQuery insertCamera(db);
        insertCamera.prepare(insertCameraSql);
        insertCamera.addBindValue(model);
        insertCamera.addBindValue(manufacturer);
        insertCamera.addBindValue(manufacturer.trimmed().toLower());
        insertCamera.addBindValue(model.toLower());
        insertCamera.addBindValue(1280);
        insertCamera.addBindValue(1024);
        insertCamera.addBindValue(4.8);
        insertCamera.addBindValue(QStringLiteral("1/2in"));
        insertCamera.addBindValue(QStringLiteral("Mono"));
        insertCamera.addBindValue(QStringLiteral("Global"));
        insertCamera.addBindValue(60.0);
        insertCamera.addBindValue(QStringLiteral("USB3"));
        insertCamera.addBindValue(380.0);
        insertCamera.addBindValue(8.0);
        insertCamera.addBindValue(60.0);
        insertCamera.addBindValue(QStringLiteral("C"));
        insertCamera.addBindValue(model.toLower() + QLatin1Char(' ') + manufacturer.trimmed().toLower());
        insertCamera.addBindValue(sourceKind);
        insertCamera.addBindValue(QStringLiteral("1"));
        insertCamera.addBindValue(now);
        insertCamera.addBindValue(now);
        if (!insertCamera.exec()) {
            error = insertCamera.lastError().text();
            return false;
        }
        return true;
    };
    QVERIFY2(insertCameraRow(QStringLiteral("STALE-BUILTIN-CAM"), QStringLiteral("DeletedMaker"), QStringLiteral("builtin")), qPrintable(error));
    QVERIFY2(insertCameraRow(QStringLiteral("LOCAL-CAM-KEEP"), QStringLiteral("LocalMaker"), QStringLiteral("local")), qPrintable(error));
    QVERIFY2(insertCameraRow(QStringLiteral("RETIRED-LOCAL-CAM"), QStringLiteral("Opto Engineering"), QStringLiteral("local")), qPrintable(error));

    QSqlQuery insertLens(db);
    insertLens.prepare(QStringLiteral(
        "INSERT INTO lens_products (model, manufacturer, manufacturer_key, model_key, lens_type, lens_mount,"
        " focal_length_mm, min_wd_mm, distortion_percent, image_circle_mm, megapixel_rating, recommended_min_pixel_um,"
        " pmag, nominal_wd_mm, wd_tolerance_mm, max_sensor_diagonal_mm, telecentricity_deg, dof_mm,"
        " numerical_aperture, f_number, coaxial_illumination, notes, search_text, source_kind, source_version,"
        " created_at, updated_at)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    insertLens.addBindValue(QStringLiteral("STALE-BUILTIN-LENS"));
    insertLens.addBindValue(QStringLiteral("DeletedMaker"));
    insertLens.addBindValue(QStringLiteral("deletedmaker"));
    insertLens.addBindValue(QStringLiteral("stale-builtin-lens"));
    insertLens.addBindValue(QStringLiteral("FixedFocal"));
    insertLens.addBindValue(QStringLiteral("C"));
    insertLens.addBindValue(25.0);
    insertLens.addBindValue(100.0);
    insertLens.addBindValue(0.05);
    insertLens.addBindValue(12.0);
    insertLens.addBindValue(5.0);
    insertLens.addBindValue(3.45);
    insertLens.addBindValue(0.0);
    insertLens.addBindValue(0.0);
    insertLens.addBindValue(0.0);
    insertLens.addBindValue(0.0);
    insertLens.addBindValue(0.0);
    insertLens.addBindValue(5.0);
    insertLens.addBindValue(0.0);
    insertLens.addBindValue(2.8);
    insertLens.addBindValue(0);
    insertLens.addBindValue(QStringLiteral("stale built-in lens"));
    insertLens.addBindValue(QStringLiteral("stale-builtin-lens deletedmaker"));
    insertLens.addBindValue(QStringLiteral("builtin"));
    insertLens.addBindValue(QStringLiteral("1"));
    insertLens.addBindValue(now);
    insertLens.addBindValue(now);
    QVERIFY2(insertLens.exec(), qPrintable(insertLens.lastError().text()));
    insertLens.finish();
    insertLens = QSqlQuery();

    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);

    error.clear();
    QVERIFY2(repo.initializeDatabase(&error), qPrintable(error));

    CatalogQuery query;
    query.limit = 10;
    query.search = QStringLiteral("STALE-BUILTIN-CAM");
    CatalogPageResult<CameraSpec> cameraPage = repo.queryCameras(query, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(cameraPage.totalCount, 0);

    query.search = QStringLiteral("STALE-BUILTIN-LENS");
    CatalogPageResult<LensSpec> lensPage = repo.queryLenses(query, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(lensPage.totalCount, 0);

    query.search = QStringLiteral("LOCAL-CAM-KEEP");
    cameraPage = repo.queryCameras(query, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(cameraPage.totalCount, 1);
    QCOMPARE(cameraPage.items.first().manufacturer, QStringLiteral("LocalMaker"));

    query.search = QStringLiteral("RETIRED-LOCAL-CAM");
    cameraPage = repo.queryCameras(query, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(cameraPage.totalCount, 0);
}

void SelectionEngineTest::sqliteCatalogQueriesPageAndDistinctValues()
{
    CatalogQuery query;
    query.limit = 2;
    query.sort.field = QStringLiteral("model");
    query.sort.ascending = true;
    const CatalogPageResult<CameraSpec> page = m_catalog.queryCameras(query);
    QCOMPARE(page.items.size(), 2);
    QCOMPARE(page.ids.size(), 2);
    QVERIFY(page.totalCount >= m_catalog.cameras().size());
    QVERIFY(page.ids.first() > 0);

    QStringList manufacturers = m_catalog.distinctValues(CatalogDomain::Camera, QStringLiteral("manufacturer"));
    QVERIFY(!manufacturers.isEmpty());
    CatalogQuery filtered;
    filtered.manufacturer = manufacturers.first();
    filtered.limit = 5;
    const CatalogPageResult<CameraSpec> filteredPage = m_catalog.queryCameras(filtered);
    QVERIFY(filteredPage.totalCount > 0);
    for (const CameraSpec &camera : filteredPage.items)
        QCOMPARE(camera.manufacturer, manufacturers.first());

    QStringList lightTypes = m_catalog.distinctValues(CatalogDomain::Light, QStringLiteral("light_type"));
    QVERIFY(!lightTypes.isEmpty());
}

void SelectionEngineTest::sqliteCatalogIdUpdateDeleteAndFilteredExport()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());

    CatalogRepository repo;
    repo.setStorageDirectory(storage.path());
    QString error;
    QVERIFY2(repo.loadDefaults(&error), qPrintable(error));

    CameraSpec camera;
    camera.model = QStringLiteral("SQL-CAM-001");
    camera.manufacturer = QStringLiteral("SqlTest");
    camera.resolutionX = 1920;
    camera.resolutionY = 1080;
    camera.pixelSizeUm = 3.45;
    camera.sensorFormat = QStringLiteral("1/2\"");
    camera.colorMode = QStringLiteral("Mono");
    camera.shutterType = QStringLiteral("Global");
    camera.maxFps = 60.0;
    camera.interfaceType = QStringLiteral("USB3");
    camera.bandwidthMBps = 380.0;
    camera.bitDepth = 12.0;
    camera.lensMount = QStringLiteral("C");
    QVERIFY2(repo.addCamera(camera, &error), qPrintable(error));

    CatalogQuery query;
    query.manufacturer = QStringLiteral("SqlTest");
    query.limit = 1;
    CatalogPageResult<CameraSpec> page = repo.queryCameras(query, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(page.totalCount, 1);
    QCOMPARE(page.items.first().model, QStringLiteral("SQL-CAM-001"));
    const qint64 id = page.ids.first();

    camera.model = QStringLiteral("SQL-CAM-EDITED");
    QVERIFY2(repo.updateCameraById(id, camera, &error), qPrintable(error));
    CameraSpec edited;
    QVERIFY2(repo.cameraById(id, &edited, &error), qPrintable(error));
    QCOMPARE(edited.model, QStringLiteral("SQL-CAM-EDITED"));

    const QString exportPath = QDir(storage.path()).filePath(QStringLiteral("filtered-cameras.csv"));
    QVERIFY2(repo.exportCameraCsvByQuery(exportPath, query, &error), qPrintable(error));
    QFile exported(exportPath);
    QVERIFY(exported.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString exportedText = QString::fromUtf8(exported.readAll());
    QVERIFY(exportedText.contains(QStringLiteral("SQL-CAM-EDITED")));

    QVERIFY2(repo.removeCameraById(id, &error), qPrintable(error));
    page = repo.queryCameras(query, &error);
    QCOMPARE(page.totalCount, 0);
}

void SelectionEngineTest::sqliteAddDuplicateProductsDoesNotReplaceExisting()
{
    LanguageGuard language;
    QVERIFY(language.setLanguage(QStringLiteral("en_US")));

    QTemporaryDir storage;
    QVERIFY(storage.isValid());

    CatalogRepository repo;
    repo.setStorageDirectory(storage.path());
    QString error;
    QVERIFY2(repo.loadDefaults(&error), qPrintable(error));

    CameraSpec camera;
    camera.model = QStringLiteral("DUP-CAM-001");
    camera.manufacturer = QStringLiteral("DupMaker");
    camera.resolutionX = 1280;
    camera.resolutionY = 1024;
    camera.pixelSizeUm = 3.45;
    camera.sensorFormat = QStringLiteral("1/2\"");
    camera.colorMode = QStringLiteral("Mono");
    camera.shutterType = QStringLiteral("Global");
    camera.maxFps = 60.0;
    camera.interfaceType = QStringLiteral("USB3");
    camera.bandwidthMBps = 380.0;
    camera.bitDepth = 12.0;
    camera.lensMount = QStringLiteral("C");
    QVERIFY2(repo.addCamera(camera, &error), qPrintable(error));

    CameraSpec duplicateCamera = camera;
    duplicateCamera.resolutionX = 4096;
    QVERIFY(!repo.addCamera(duplicateCamera, &error));
    QVERIFY(error.contains(QStringLiteral("Duplicate camera product")));

    CatalogQuery cameraQuery;
    cameraQuery.manufacturer = QStringLiteral("DupMaker");
    cameraQuery.search = QStringLiteral("DUP-CAM-001");
    cameraQuery.limit = 10;
    error.clear();
    CatalogPageResult<CameraSpec> cameraPage = repo.queryCameras(cameraQuery, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(cameraPage.totalCount, 1);
    QCOMPARE(cameraPage.items.first().resolutionX, 1280);

    LensSpec lens;
    lens.model = QStringLiteral("DUP-LENS-001");
    lens.manufacturer = QStringLiteral("DupMaker");
    lens.lensType = LensType::FixedFocal;
    lens.lensMount = QStringLiteral("C");
    lens.focalLengthMm = 25.0;
    lens.minWorkingDistanceMm = 100.0;
    lens.distortionPercent = 0.05;
    lens.imageCircleMm = 12.0;
    lens.megapixelRating = 12.0;
    QVERIFY2(repo.addLens(lens, &error), qPrintable(error));

    LensSpec duplicateLens = lens;
    duplicateLens.imageCircleMm = 30.0;
    QVERIFY(!repo.addLens(duplicateLens, &error));
    QVERIFY(error.contains(QStringLiteral("Duplicate lens product")));

    CatalogQuery lensQuery;
    lensQuery.manufacturer = QStringLiteral("DupMaker");
    lensQuery.search = QStringLiteral("DUP-LENS-001");
    lensQuery.limit = 10;
    error.clear();
    CatalogPageResult<LensSpec> lensPage = repo.queryLenses(lensQuery, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(lensPage.totalCount, 1);
    QCOMPARE(lensPage.items.first().imageCircleMm, 12.0);

    LightSpec light;
    light.model = QStringLiteral("DUP-LIGHT-001");
    light.manufacturer = QStringLiteral("DupMaker");
    light.lightType = LightType::Backlight;
    light.color = QStringLiteral("White");
    light.mode = QStringLiteral("Continuous");
    light.activeWidthMm = 50.0;
    light.activeHeightMm = 40.0;
    QVERIFY2(repo.addLight(light, &error), qPrintable(error));

    LightSpec duplicateLight = light;
    duplicateLight.activeWidthMm = 200.0;
    QVERIFY(!repo.addLight(duplicateLight, &error));
    QVERIFY(error.contains(QStringLiteral("Duplicate light product")));

    CatalogQuery lightQuery;
    lightQuery.manufacturer = QStringLiteral("DupMaker");
    lightQuery.search = QStringLiteral("DUP-LIGHT-001");
    lightQuery.limit = 10;
    error.clear();
    CatalogPageResult<LightSpec> lightPage = repo.queryLights(lightQuery, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(lightPage.totalCount, 1);
    QCOMPARE(lightPage.items.first().activeWidthMm, 50.0);
}

void SelectionEngineTest::sqliteFilteredExportFailsWhenQueryFails()
{
    QTemporaryFile storageFile;
    QVERIFY(storageFile.open());
    const QString invalidStoragePath = storageFile.fileName();
    storageFile.close();

    QTemporaryDir output;
    QVERIFY(output.isValid());

    CatalogRepository repo;
    repo.setStorageDirectory(invalidStoragePath);
    QString error;
    CatalogQuery query;
    const QString exportPath = QDir(output.path()).filePath(QStringLiteral("cameras.csv"));
    QVERIFY(!repo.exportCameraCsvByQuery(exportPath, query, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFileInfo::exists(exportPath));
}

void SelectionEngineTest::sqliteSelectionCandidatesFilterBeforeLimit()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());

    CatalogRepository repo;
    repo.setStorageDirectory(storage.path());
    QString error;
    QVERIFY2(repo.initializeDatabase(&error), qPrintable(error));

    QTemporaryFile cameraCsv;
    QVERIFY(cameraCsv.open());
    {
        QTextStream out(&cameraCsv);
        out.setEncoding(QStringConverter::Utf8);
        out << "model,manufacturer,resolution_x,resolution_y,pixel_size_um,sensor_format,color_mode,shutter_type,max_fps,interface,bandwidth_mbps,bit_depth,dynamic_range_db,lens_mount\n";
        for (int i = 0; i < 350; ++i) {
            out << "LOW-RES-CAM-" << i << ",CandidateFixture,640,480,3.45,1/3in,Mono,Global,500,USB3,380,12,60,C\n";
        }
        out << "VALID-HIGH-RES-CAM,CandidateFixture,4096,3000,3.45,1.1in,Mono,Global,25,USB3,380,12,60,C\n";
    }
    cameraCsv.close();
    QVERIFY2(repo.loadCameraCsv(cameraCsv.fileName(), &error), qPrintable(error));

    SelectionRequest request;
    request.objectWidthMm = 40.0;
    request.objectHeightMm = 30.0;
    request.placementMarginMm = 0.0;
    request.minFeatureUm = 50.0;
    request.measurementToleranceUm = 50.0;
    request.requiredFps = 20.0;
    request.detectionType = DetectionType::Measurement;

    error.clear();
    const QVector<CameraSpec> cameraCandidates = repo.selectionCandidateCameras(request, 300, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    bool foundCamera = false;
    for (const CameraSpec &candidate : cameraCandidates)
        foundCamera = foundCamera || candidate.model == QStringLiteral("VALID-HIGH-RES-CAM");
    QVERIFY(foundCamera);

    QTemporaryFile lensCsv;
    QVERIFY(lensCsv.open());
    {
        QTextStream out(&lensCsv);
        out.setEncoding(QStringConverter::Utf8);
        out << "model,manufacturer,lens_type,lens_mount,focal_length_mm,min_wd_mm,distortion_percent,image_circle_mm,megapixel_rating,recommended_min_pixel_um,pmag,nominal_wd_mm,wd_tolerance_mm,max_sensor_diagonal_mm,telecentricity_deg,dof_mm,numerical_aperture,f_number,coaxial_illumination,notes\n";
        for (int i = 0; i < 600; ++i) {
            out << "SMALL-CIRCLE-LENS-" << i << ",CandidateFixture,FixedFocal,C,8,50,0.05,4,5,3.45,0,0,0,0,0,4,0.03,2.8,false,small image circle\n";
        }
        out << "VALID-LARGE-CIRCLE-LENS,CandidateFixture,FixedFocal,C,25,50,0.05,30,12,3.45,0,0,0,0,0,8,0.03,2.8,false,large image circle\n";
    }
    lensCsv.close();
    QVERIFY2(repo.loadLensCsv(lensCsv.fileName(), &error), qPrintable(error));

    request.allowTelecentric = false;
    request.workingDistanceMm = 110.0;
    error.clear();
    const QVector<LensSpec> lensCandidates = repo.selectionCandidateLenses(request, 500, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    bool foundLens = false;
    for (const LensSpec &candidate : lensCandidates)
        foundLens = foundLens || candidate.model == QStringLiteral("VALID-LARGE-CIRCLE-LENS");
    QVERIFY(foundLens);
}

void SelectionEngineTest::sqliteLensCandidateFocalRecallSurvivesImageCircleLimit()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());

    CatalogRepository repo;
    repo.setStorageDirectory(storage.path());
    QString error;
    QVERIFY2(repo.initializeDatabase(&error), qPrintable(error));

    QTemporaryFile lensCsv;
    QVERIFY(lensCsv.open());
    {
        QTextStream out(&lensCsv);
        out.setEncoding(QStringConverter::Utf8);
        out << "model,manufacturer,lens_type,lens_mount,focal_length_mm,min_wd_mm,distortion_percent,image_circle_mm,megapixel_rating,recommended_min_pixel_um,pmag,nominal_wd_mm,wd_tolerance_mm,max_sensor_diagonal_mm,telecentricity_deg,dof_mm,numerical_aperture,f_number,coaxial_illumination,notes\n";
        for (int i = 1; i <= 600; ++i) {
            if (i == 300)
                continue;
            out << "NOISE-LENS-" << i << ",CandidateFixture,FixedFocal,C,8,50,0.05," << i
                << ",12,3.45,0,0,0,0,0,8,0.03,2.8,false,image circle ordering noise\n";
        }
        out << "MID-CIRCLE-TARGET-FOCAL,CandidateFixture,FixedFocal,C,16,50,0.05,300,12,3.45,0,0,0,0,0,8,0.03,2.8,false,target focal in middle image-circle band\n";
    }
    lensCsv.close();
    QVERIFY2(repo.loadLensCsv(lensCsv.fileName(), &error), qPrintable(error));

    SelectionRequest request;
    request.objectWidthMm = 40.0;
    request.objectHeightMm = 30.0;
    request.placementMarginMm = 0.0;
    request.workingDistanceMm = 110.0;
    request.allowTelecentric = false;

    error.clear();
    const QVector<LensSpec> lensCandidates = repo.selectionCandidateLenses(request, 500, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    bool foundTarget = false;
    for (const LensSpec &candidate : lensCandidates)
        foundTarget = foundTarget || candidate.model == QStringLiteral("MID-CIRCLE-TARGET-FOCAL");
    QVERIFY(foundTarget);
}

void SelectionEngineTest::multilineQuotedCsvImports()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());

    CatalogRepository repo;
    repo.setStorageDirectory(storage.path());
    QString error;
    QVERIFY2(repo.initializeDatabase(&error), qPrintable(error));

    QTemporaryFile lensCsv;
    QVERIFY(lensCsv.open());
    {
        QTextStream out(&lensCsv);
        out.setEncoding(QStringConverter::Utf8);
        out << "model,manufacturer,lens_type,lens_mount,focal_length_mm,min_wd_mm,distortion_percent,image_circle_mm,megapixel_rating,recommended_min_pixel_um,pmag,nominal_wd_mm,wd_tolerance_mm,max_sensor_diagonal_mm,telecentricity_deg,dof_mm,numerical_aperture,f_number,coaxial_illumination,notes\n";
        out << "MULTILINE-NOTES,ImportFixture,FixedFocal,C,16,50,0.05,18,12,3.45,0,0,0,0,0,8,0.03,2.8,false,\"line one\nline two\"\n";
    }
    lensCsv.close();
    QVERIFY2(repo.loadLensCsv(lensCsv.fileName(), &error), qPrintable(error));

    CatalogQuery query;
    query.search = QStringLiteral("MULTILINE-NOTES");
    query.limit = 10;
    error.clear();
    const CatalogPageResult<LensSpec> page = repo.queryLenses(query, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(page.items.size(), 1);
    QCOMPARE(page.items.first().notes, QStringLiteral("line one\nline two"));
}

void SelectionEngineTest::sqliteLightCandidatesUseLensFeatureCache()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());

    CatalogRepository repo;
    repo.setStorageDirectory(storage.path());
    QString error;
    QVERIFY2(repo.initializeDatabase(&error), qPrintable(error));

    SelectionRequest request;
    request.objectWidthMm = 20.0;
    request.objectHeightMm = 20.0;
    request.placementMarginMm = 2.0;
    request.surfaceType = SurfaceType::ReflectiveMetal;
    request.reflective = true;

    const QVector<LightSpec> coaxialCandidates = repo.selectionCandidateLights(request, false, true, 20, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(!coaxialCandidates.isEmpty());
    QCOMPARE(coaxialCandidates.first().lightType, LightType::Coaxial);

    const QVector<LightSpec> cached = repo.selectionCandidateLights(request, false, true, 20, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(cached.size(), coaxialCandidates.size());
    QCOMPARE(cached.first().model, coaxialCandidates.first().model);

    QTemporaryFile lightCsv;
    QVERIFY(lightCsv.open());
    {
        QTextStream out(&lightCsv);
        out.setEncoding(QStringConverter::Utf8);
        out << "model,manufacturer,light_type,color,wavelength_nm,mode,active_width_mm,active_height_mm,best_for\n";
        out << "SMALL-RING,CacheFixture,Ring,White,0,Continuous,30,30,general\n";
        out << "LARGE-COAXIAL,CacheFixture,Coaxial,White,0,Continuous,60,60,reflective\n";
    }
    lightCsv.close();
    QVERIFY2(repo.loadLightCsv(lightCsv.fileName(), &error), qPrintable(error));

    request.surfaceType = SurfaceType::Matte;
    request.reflective = false;
    const QVector<LightSpec> matteCandidates = repo.selectionCandidateLights(request, false, false, 1, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(matteCandidates.size(), 1);
    QCOMPARE(matteCandidates.first().model, QStringLiteral("SMALL-RING"));

    request.reflective = true;
    const QVector<LightSpec> reflectiveCandidates = repo.selectionCandidateLights(request, false, false, 1, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(reflectiveCandidates.size(), 1);
    QCOMPARE(reflectiveCandidates.first().model, QStringLiteral("LARGE-COAXIAL"));
}

void SelectionEngineTest::diagnosticPruningMatchesExhaustiveRanking()
{
    QVector<CameraSpec> cameras;
    QVector<LensSpec> lenses;
    for (int i = 0; i < 12; ++i) {
        CameraSpec camera;
        camera.model = QStringLiteral("BOUND-CAM-%1").arg(i);
        camera.manufacturer = QStringLiteral("Fixture");
        camera.resolutionX = 2000 + (i % 3) * 100; camera.resolutionY = 2000;
        camera.pixelSizeUm = 3.45; camera.colorMode = QStringLiteral("Mono");
        camera.pixelFormat = QStringLiteral("Mono8"); camera.maxFps = 100;
        camera.lensMount = QStringLiteral("C"); camera.bandwidthMBps = 1000;
        cameras.append(camera);
        LensSpec lens;
        lens.model = QStringLiteral("BOUND-LENS-%1").arg(i);
        lens.manufacturer = QStringLiteral("Fixture"); lens.lensMount = QStringLiteral("C");
        lens.lensType = LensType::FixedFocal; lens.focalLengthMm = 8 + i;
        lens.minWorkingDistanceMm = 10; lens.imageCircleMm = 25;
        lens.fNumber = 2.8; lens.megapixelRating = 10;
        lenses.append(lens);
    }
    LightSpec light; light.model = QStringLiteral("BOUND-LIGHT");
    light.lightType = LightType::Backlight; light.activeWidthMm = light.activeHeightMm = 100;
    SelectionRequest request; request.allowTelecentric = false;
    SelectionEngine engine;
    for (const double feature : {50.0, 2000.0}) {
        request.minFeatureUm = feature; request.measurementToleranceUm = feature;
        const auto exhaustive = engine.select(request, cameras, lenses, {light}, 0);
        const auto bounded = engine.select(request, cameras, lenses, {light}, 7);
        QCOMPARE(bounded.size(), 7);
        QVERIFY(exhaustive.size() > bounded.size());
        for (int i = 0; i < bounded.size(); ++i) {
            QCOMPARE(bounded[i].camera.model, exhaustive[i].camera.model);
            QCOMPARE(bounded[i].lens.model, exhaustive[i].lens.model);
            QCOMPARE(bounded[i].score.score, exhaustive[i].score.score);
            QCOMPARE(bounded[i].hardConstraintsPassed, exhaustive[i].hardConstraintsPassed);
        }
    }
}

void SelectionEngineTest::catalogPerformanceGate()
{
    if (qgetenv("VISIONSELECT_PERF_GATE") != "1")
        QSKIP("Set VISIONSELECT_PERF_GATE=1 to run the optional 100k-row catalog performance gate.");

    QTemporaryDir storage;
    QVERIFY(storage.isValid());

    CatalogRepository repo;
    repo.setStorageDirectory(storage.path());
    QString error;
    QVERIFY2(repo.initializeDatabase(&error), qPrintable(error));

    const int cameraTarget = 20000;
    const int lensTarget = 30000;
    const int lightTarget = 50000;

    QTemporaryFile cameraCsv;
    QVERIFY(cameraCsv.open());
    {
        QTextStream out(&cameraCsv);
        out.setEncoding(QStringConverter::Utf8);
        out << "model,manufacturer,resolution_x,resolution_y,pixel_size_um,sensor_format,color_mode,shutter_type,max_fps,interface,bandwidth_mbps,bit_depth,dynamic_range_db,lens_mount\n";
        for (int i = 0; i < cameraTarget; ++i) {
            out << "PERF-CAM-" << i << ",PerfCam," << (1600 + (i % 8) * 320) << "," << (1200 + (i % 6) * 240)
                << ",3.45,1/1.8in,Mono,Global," << (30 + (i % 12) * 10) << ",USB3,380,12,60,C\n";
        }
    }
    cameraCsv.close();
    QVERIFY2(repo.loadCameraCsv(cameraCsv.fileName(), &error), qPrintable(error));

    QTemporaryFile lensCsv;
    QVERIFY(lensCsv.open());
    {
        QTextStream out(&lensCsv);
        out.setEncoding(QStringConverter::Utf8);
        out << "model,manufacturer,lens_type,lens_mount,focal_length_mm,min_wd_mm,distortion_percent,image_circle_mm,megapixel_rating,recommended_min_pixel_um,pmag,nominal_wd_mm,wd_tolerance_mm,max_sensor_diagonal_mm,telecentricity_deg,dof_mm,numerical_aperture,f_number,coaxial_illumination,notes\n";
        for (int i = 0; i < lensTarget; ++i) {
            const bool telecentric = (i % 5) == 0;
            out << "PERF-LENS-" << i << ",PerfLens," << (telecentric ? "ObjectTelecentric" : "FixedFocal")
                << ",C," << (12 + (i % 8) * 4) << ",100,0.05," << (11 + (i % 12))
                << ",12,3.45," << (telecentric ? "0.2" : "0") << ",110,5,22,0.08,8,0.03,2.8,"
                << ((i % 7) == 0 ? "true" : "false") << ",performance fixture\n";
        }
    }
    lensCsv.close();
    QVERIFY2(repo.loadLensCsv(lensCsv.fileName(), &error), qPrintable(error));

    QTemporaryFile lightCsv;
    QVERIFY(lightCsv.open());
    {
        QTextStream out(&lightCsv);
        out.setEncoding(QStringConverter::Utf8);
        out << "model,manufacturer,light_type,color,wavelength_nm,mode,active_width_mm,active_height_mm,best_for\n";
        for (int i = 0; i < lightTarget; ++i) {
            const char *type = (i % 6 == 0) ? "Coaxial" : (i % 6 == 1) ? "Dome" : (i % 6 == 2) ? "DarkField" : "Backlight";
            out << "PERF-LIGHT-" << i << ",PerfLight," << type << ",White,0,"
                << ((i % 4) == 0 ? "Strobe" : "Continuous") << "," << (40 + (i % 20) * 10)
                << "," << (40 + (i % 16) * 10) << ",performance fixture\n";
        }
    }
    lightCsv.close();
    QVERIFY2(repo.loadLightCsv(lightCsv.fileName(), &error), qPrintable(error));

    QElapsedTimer timer;
    CatalogQuery query;
    query.manufacturer = QStringLiteral("PerfCam");
    query.limit = 500;
    timer.start();
    const CatalogPageResult<CameraSpec> cameraPage = repo.queryCameras(query, &error);
    const qint64 filterMs = timer.elapsed();
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(cameraPage.items.size(), 500);
    QVERIFY2(filterMs < 300, qPrintable(QStringLiteral("Camera filter query took %1 ms").arg(filterMs)));

    query.offset = 5000;
    timer.restart();
    repo.queryCameras(query, &error);
    const qint64 pageMs = timer.elapsed();
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY2(pageMs < 200, qPrintable(QStringLiteral("Camera page query took %1 ms").arg(pageMs)));

    SelectionRequest request;
    timer.restart();
    SelectionService service(&repo);
    const QVector<SelectionResult> results = service.select(request, 20, &error);
    const qint64 selectMs = timer.elapsed();
    qInfo().noquote() << QStringLiteral("十万条目录：筛选 %1 ms，翻页 %2 ms，选型 %3 ms").arg(filterMs).arg(pageMs).arg(selectMs);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(!results.isEmpty());
    QVERIFY2(selectMs < 2000, qPrintable(QStringLiteral("Selection took %1 ms").arg(selectMs)));
}

void SelectionEngineTest::sqliteMigrationPreservesLocalCsvRows()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());

    QFile builtInCameraFile(QStringLiteral(":/data/cameras.csv"));
    QVERIFY2(builtInCameraFile.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(builtInCameraFile.errorString()));
    QTextStream builtInIn(&builtInCameraFile);
    builtInIn.setEncoding(QStringConverter::Utf8);
    const QString header = builtInIn.readLine();
    const QString builtInRow = builtInIn.readLine();
    const auto unquoteCsvCell = [](QString value) {
        value = value.trimmed();
        if (value.startsWith(QLatin1Char('"')))
            value.remove(0, 1);
        if (value.endsWith(QLatin1Char('"')))
            value.chop(1);
        value.replace(QStringLiteral("\"\""), QStringLiteral("\""));
        return value;
    };
    const QStringList builtInCells = builtInRow.split(QStringLiteral("\",\""));
    QVERIFY(builtInCells.size() >= 2);
    const QString builtInModel = unquoteCsvCell(builtInCells.at(0));
    const QString builtInManufacturer = unquoteCsvCell(builtInCells.at(1));

    QFile cameraFile(QDir(storage.path()).filePath(QStringLiteral("cameras.csv")));
    QVERIFY(cameraFile.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&cameraFile);
    out.setEncoding(QStringConverter::Utf8);
    out << header << "\n";
    out << builtInRow << "\n";
    out << "LOCAL-CAM-001,LocalMaker,1280,1024,4.8,1/2in,Mono,Global,100,USB3,380,12,60,C\n";
    cameraFile.close();

    CatalogRepository repo;
    repo.setStorageDirectory(storage.path());
    QString error;
    QVERIFY2(repo.loadDefaults(&error), qPrintable(error));
    QVERIFY(QFileInfo(QDir(storage.path()).filePath(QStringLiteral("catalog.db"))).exists());

    CatalogQuery query;
    query.search = QStringLiteral("LOCAL-CAM-001");
    query.limit = 10;
    const CatalogPageResult<CameraSpec> page = repo.queryCameras(query, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(page.totalCount, 1);
    QCOMPARE(page.items.first().manufacturer, QStringLiteral("LocalMaker"));

    const QStringList backups = QDir(storage.path()).entryList(QStringList() << QStringLiteral("catalog_migration_backup_*"), QDir::Dirs);
    QVERIFY(!backups.isEmpty());

    const QString connectionName = QStringLiteral("sqliteMigrationPreservesLocalCsvRows");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(QDir(storage.path()).filePath(QStringLiteral("catalog.db")));
    QVERIFY2(db.open(), qPrintable(db.lastError().text()));
    QSqlQuery sourceQuery(db);
    sourceQuery.prepare(QStringLiteral("SELECT source_kind FROM camera_products WHERE manufacturer=? AND model=?"));
    sourceQuery.addBindValue(builtInManufacturer);
    sourceQuery.addBindValue(builtInModel);
    QVERIFY2(sourceQuery.exec(), qPrintable(sourceQuery.lastError().text()));
    QVERIFY(sourceQuery.next());
    QCOMPARE(sourceQuery.value(0).toString(), QStringLiteral("builtin"));
    sourceQuery.finish();

    sourceQuery.prepare(QStringLiteral("SELECT source_kind FROM camera_products WHERE manufacturer='LocalMaker' AND model='LOCAL-CAM-001'"));
    QVERIFY2(sourceQuery.exec(), qPrintable(sourceQuery.lastError().text()));
    QVERIFY(sourceQuery.next());
    QCOMPARE(sourceQuery.value(0).toString(), QStringLiteral("local"));
    sourceQuery = QSqlQuery();
    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

void SelectionEngineTest::sampleOnlyLightCatalogIsUpgraded()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());

    QFile source(QStringLiteral(":/data/lights.csv"));
    QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(source.errorString()));
    QFile target(QDir(storage.path()).filePath(QStringLiteral("lights.csv")));
    QVERIFY2(target.open(QIODevice::WriteOnly | QIODevice::Text), qPrintable(target.errorString()));

    QTextStream in(&source);
    in.setEncoding(QStringConverter::Utf8);
    QTextStream out(&target);
    out.setEncoding(QStringConverter::Utf8);
    for (int i = 0; i < 7 && !in.atEnd(); ++i)
        out << in.readLine() << "\n";
    target.close();

    CatalogRepository repo;
    repo.setStorageDirectory(storage.path());
    QString error;
    QVERIFY2(repo.loadDefaults(&error), qPrintable(error));
    QVERIFY(repo.lights().size() >= 180);
}

void SelectionEngineTest::motionExposureAndStrobePreference()
{
    LanguageGuard language;
    QVERIFY(language.setLanguage(QStringLiteral("en_US")));

    SelectionRequest request;
    request.objectWidthMm = 30.0;
    request.objectHeightMm = 20.0;
    request.placementMarginMm = 2.0;
    request.minFeatureUm = 100.0;
    request.measurementToleranceUm = 50.0;
    request.workingDistanceMm = 120.0;
    request.motionMode = MotionMode::Continuous;
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

    CameraSpec rollingCamera = camera;
    rollingCamera.shutterType = QStringLiteral("Rolling");
    SelectionRequest stopAndGo = request;
    stopAndGo.motionMode = MotionMode::StopAndGo;
    const QVector<SelectionResult> stopAndGoResults = engine.select(stopAndGo, {rollingCamera}, {lens}, {continuous}, 1);
    QVERIFY(!stopAndGoResults.isEmpty());
    QCOMPARE(stopAndGoResults.first().maxExposureUsForOnePixelBlur, 0.0);
    QCOMPARE(stopAndGoResults.first().checks[CandidateCheck::GlobalShutter], CandidateCheckState::NotApplicable);

    const QVector<SelectionResult> rollingResults = engine.select(request, {rollingCamera}, {lens}, {continuous}, 1);
    QVERIFY(!rollingResults.isEmpty());
    QVERIFY(!rollingResults.first().hardConstraintsPassed);
    QCOMPARE(rollingResults.first().checks[CandidateCheck::GlobalShutter], CandidateCheckState::Failed);
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
    camera.pixelFormat = QStringLiteral("Mono12");
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
    camera.pixelFormat.clear();
    const auto unknownFormat = engine.select(request, {camera}, {lens}, {light}, 1);
    QVERIFY(!unknownFormat.isEmpty());
    QCOMPARE(unknownFormat.first().checks[CandidateCheck::PixelFormat], CandidateCheckState::Unknown);
    QCOMPARE(unknownFormat.first().checks[CandidateCheck::Bandwidth], CandidateCheckState::Unknown);
}

void SelectionEngineTest::highResolutionFramePayloadDoesNotOverflow()
{
    CameraSpec camera;
    camera.model = QStringLiteral("MAX-RES-CAM");
    camera.resolutionX = 200000;
    camera.resolutionY = 200000;
    camera.bitDepth = 12.0;

    const double payloadMB = SelectionEngine::framePayloadMB(camera);
    QVERIFY(qAbs(payloadMB - 60000.0) < 0.001);
    QVERIFY(qAbs(SelectionEngine::bandwidthRequiredMBps(camera, 2.0) - 129600.0) < 0.001);
}

void SelectionEngineTest::explicitPixelFormatAffectsPayloadAndBandwidth()
{
    CameraSpec camera;
    camera.model = QStringLiteral("PIXEL-FORMAT-CAM");
    camera.resolutionX = 1000;
    camera.resolutionY = 1000;
    camera.bitDepth = 8.0;
    camera.interfaceType = QStringLiteral("USB3");

    camera.colorMode = QStringLiteral("Mono8");
    QVERIFY(qAbs(SelectionEngine::framePayloadMB(camera) - 1.0) < 0.001);
    QVERIFY(qAbs(SelectionEngine::bandwidthRequiredMBps(camera, 100.0) - 105.0) < 0.001);

    camera.colorMode = QStringLiteral("RGB8");
    QVERIFY(qAbs(SelectionEngine::framePayloadMB(camera) - 3.0) < 0.001);
    QVERIFY(qAbs(SelectionEngine::bandwidthRequiredMBps(camera, 100.0) - 315.0) < 0.001);

    camera.colorMode = QStringLiteral("Mono16");
    QVERIFY(qAbs(SelectionEngine::framePayloadMB(camera) - 2.0) < 0.001);

    camera.colorMode = QStringLiteral("YUV422");
    QVERIFY(qAbs(SelectionEngine::framePayloadMB(camera) - 2.0) < 0.001);
}

void SelectionEngineTest::cameraEstimatePenalizesInsufficientBandwidth()
{
    SelectionRequest request;
    request.objectWidthMm = 20.0;
    request.objectHeightMm = 15.0;
    request.placementMarginMm = 1.0;
    request.minFeatureUm = 300.0;
    request.measurementToleranceUm = 100.0;
    request.requiredFps = 200.0;
    request.detectionType = DetectionType::Positioning;

    CameraSpec constrained;
    constrained.model = QStringLiteral("BANDWIDTH-LIMITED");
    constrained.manufacturer = QStringLiteral("Test");
    constrained.resolutionX = 4096;
    constrained.resolutionY = 4096;
    constrained.pixelSizeUm = 3.45;
    constrained.colorMode = QStringLiteral("Mono");
    constrained.shutterType = QStringLiteral("Global");
    constrained.maxFps = 250.0;
    constrained.interfaceType = QStringLiteral("GigE");
    constrained.bandwidthMBps = 120.0;
    constrained.bitDepth = 12.0;
    constrained.lensMount = QStringLiteral("C");

    CameraSpec balanced = constrained;
    balanced.model = QStringLiteral("BALANCED");
    balanced.resolutionX = 1280;
    balanced.resolutionY = 1024;
    balanced.pixelSizeUm = 4.8;
    balanced.interfaceType = QStringLiteral("USB3");
    balanced.bandwidthMBps = 380.0;
    balanced.bitDepth = 8.0;

    CameraSpec unknownInterface = balanced;
    unknownInterface.model = QStringLiteral("UNKNOWN-BANDWIDTH");
    unknownInterface.interfaceType = QStringLiteral("CustomBus");
    unknownInterface.bandwidthMBps = 0.0;

    const QVector<CameraCalculationEstimate> estimates =
        CalculationAssistant::estimateCameras(request, {constrained, balanced, unknownInterface}, 3);
    QCOMPARE(estimates.size(), 3);
    QCOMPARE(estimates.first().camera.model, QStringLiteral("BALANCED"));
    QVERIFY(estimates.first().meetsBandwidth);

    bool sawLimited = false;
    bool sawUnknown = false;
    for (const CameraCalculationEstimate &estimate : estimates) {
        if (estimate.camera.model == QStringLiteral("BANDWIDTH-LIMITED")) {
            sawLimited = true;
            QVERIFY(!estimate.meetsBandwidth);
            QVERIFY(estimate.bandwidthUtilizationPercent > 100.0);
        }
        if (estimate.camera.model == QStringLiteral("UNKNOWN-BANDWIDTH")) {
            sawUnknown = true;
            QCOMPARE(estimate.interfaceCapacityMBps, 0.0);
            QVERIFY(!estimate.meetsBandwidth);
            QCOMPARE(estimate.bandwidthUtilizationPercent, 0.0);
        }
    }
    QVERIFY(sawLimited);
    QVERIFY(sawUnknown);
}

void SelectionEngineTest::globalShutterAliasesAreRecognized()
{
    CameraSpec camera;
    camera.shutterType = QStringLiteral("Global Shutter");
    QVERIFY(camera.isGlobalShutter());

    camera.shutterType = QStringLiteral("global shutter CMOS");
    QVERIFY(camera.isGlobalShutter());

    camera.shutterType = QStringLiteral("GlobalResetRelease");
    QVERIFY(!camera.isGlobalShutter());

    camera.shutterType = QStringLiteral("Rolling/GlobalResetRelease");
    QVERIFY(!camera.isGlobalShutter());

    camera.shutterType = QStringLiteral("Global shutter / Rolling shutter");
    QVERIFY(camera.isGlobalShutter());

    camera.shutterType = QStringLiteral("Rolling/Global");
    QVERIFY(camera.isGlobalShutter());

    camera.shutterType = QStringLiteral("Rolling / Global");
    QVERIFY(camera.isGlobalShutter());

    camera.shutterType = QStringLiteral("Global / Rolling");
    QVERIFY(camera.isGlobalShutter());
}

void SelectionEngineTest::lowAngleRingLightActsAsDarkField()
{
    QCOMPARE(lightTypeFromString(QStringLiteral("Low Angle Ring")), LightType::DarkField);
    QCOMPARE(lightTypeFromString(QStringLiteral("Dark-field Ring")), LightType::DarkField);

    LightSpec normal;
    normal.model = QStringLiteral("RING");
    normal.manufacturer = QStringLiteral("Test");
    normal.lightType = LightType::Ring;
    normal.mode = QStringLiteral("Strobe");
    normal.activeWidthMm = 100.0;
    normal.activeHeightMm = 100.0;
    normal.bestFor = QString::fromUtf8("\350\247\222\345\272\246 45\302\260");
    QVERIFY(!normal.isDarkFieldLike());

    LightSpec lowAngle = normal;
    lowAngle.model = QStringLiteral("LOW-ANGLE");
    lowAngle.bestFor = QString::fromUtf8("\350\247\222\345\272\246 90\302\260");
    QVERIFY(lowAngle.isDarkFieldLike());

    SelectionRequest request;
    request.objectWidthMm = 40.0;
    request.objectHeightMm = 30.0;
    request.placementMarginMm = 2.0;
    request.minFeatureUm = 100.0;
    request.measurementToleranceUm = 50.0;
    request.workingDistanceMm = 120.0;
    request.detectionType = DetectionType::DefectInspection;
    request.surfaceType = SurfaceType::ReflectiveMetal;
    request.reflective = true;

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

    SelectionEngine engine;
    const QVector<SelectionResult> results = engine.select(request, {camera}, {lens}, {normal, lowAngle}, 1);
    QVERIFY(!results.isEmpty());
    QCOMPARE(results.first().light.model, QStringLiteral("LOW-ANGLE"));
}

void SelectionEngineTest::directionalDefectLightCoverageUsesLongAxis()
{
    SelectionRequest request;
    request.objectWidthMm = 100.0;
    request.objectHeightMm = 60.0;
    request.placementMarginMm = 5.0;
    request.detectionType = DetectionType::DefectInspection;

    LightSpec bar;
    bar.model = QStringLiteral("BAR");
    bar.manufacturer = QStringLiteral("Test");
    bar.lightType = LightType::Bar;
    bar.activeWidthMm = 750.0;
    bar.activeHeightMm = 16.0;

    QVERIFY(SelectionEngine::lightCoverageMarginPercent(request, bar) < 0.0);

    request.detectionType = DetectionType::Measurement;
    QVERIFY(SelectionEngine::lightCoverageMarginPercent(request, bar) < 0.0);

    request.detectionType = DetectionType::DefectInspection;
    bar.activeHeightMm = 90.0;
    QVERIFY(SelectionEngine::lightCoverageMarginPercent(request, bar) > 0.0);
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

void SelectionEngineTest::fixedFocalRejectsInvalidWorkingDistance()
{
    SelectionRequest request;
    request.objectWidthMm = 10.0;
    request.objectHeightMm = 10.0;
    request.placementMarginMm = 1.0;
    request.minFeatureUm = 300.0;
    request.measurementToleranceUm = 100.0;
    request.workingDistanceMm = 40.0;
    request.requiredFps = 10.0;
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
    lens.model = QStringLiteral("INVALID-WD-F");
    lens.manufacturer = QStringLiteral("Test");
    lens.lensType = LensType::FixedFocal;
    lens.lensMount = QStringLiteral("C");
    lens.focalLengthMm = 50.0;
    lens.minWorkingDistanceMm = 10.0;
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
    QCOMPARE(results.size(), 1);
    QVERIFY(!results.first().hardConstraintsPassed);
    QVERIFY(results.first().hardFailures.join(QStringLiteral(";")).contains(QStringLiteral("WD")));
    QVERIFY(!results.first().score.risks.isEmpty());

    const QVector<LensCalculationEstimate> estimates = CalculationAssistant::estimateLenses(request, camera, {lens}, 1);
    QCOMPARE(estimates.size(), 1);
    QCOMPARE(estimates.first().effectiveFovWidthMm, 0.0);
    QVERIFY(!estimates.first().fovOk);
    QVERIFY(estimates.first().risks.join(QStringLiteral(";")).contains(QStringLiteral("WD")));

    PureCalculationInput input;
    input.request = request;
    input.camera = camera;
    input.lens = lens;
    input.light = light;
    input.telecentricMode = false;
    const PureCalculationResult pure = CalculationAssistant::estimatePure(input);
    QCOMPARE(pure.effectiveFovWidthMm, 0.0);
    QVERIFY(pure.risks.join(QStringLiteral(";")).contains(QStringLiteral("WD")));
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
    request.measurementToleranceUm = 25.0;
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

void SelectionEngineTest::defaultRequestHasCompatibleRecommendation()
{
    SelectionRequest request;
    QCOMPARE(request.measurementToleranceUm, 25.0);

    SelectionEngine engine;
    const QVector<SelectionResult> results = engine.select(request, m_catalog.cameras(), m_catalog.lenses(), m_catalog.lights(), 10);
    QVERIFY(!results.isEmpty());

    bool hasCompatible = false;
    QStringList diagnostics;
    for (const SelectionResult &result : results) {
        diagnostics.append(QStringLiteral("%1/%2: %3")
            .arg(result.camera.model, result.lens.model, result.hardFailures.join(QStringLiteral(";"))));
        if (result.hardConstraintsPassed) {
            hasCompatible = true;
            break;
        }
    }

    QVERIFY2(hasCompatible, qPrintable(diagnostics.join(QStringLiteral("\n"))));
}

void SelectionEngineTest::defaultRecommendationsIncludeFixedLensAlternatives()
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
    const QVector<SelectionResult> results = engine.select(request, m_catalog.cameras(), m_catalog.lenses(), m_catalog.lights(), 20);
    QVERIFY(!results.isEmpty());

    bool hasTelecentric = false;
    bool hasFixedFocal = false;
    bool hasDiagnosticFixedFocal = false;
    QStringList lensModels;
    for (const SelectionResult &result : results) {
        lensModels.append(result.lens.model);
        if (result.isTelecentric())
            hasTelecentric = true;
        else if (result.hardConstraintsPassed)
            hasFixedFocal = true;
        else
            hasDiagnosticFixedFocal = true;
    }

    QVERIFY(hasTelecentric);
    QVERIFY2(hasFixedFocal || hasDiagnosticFixedFocal, qPrintable(lensModels.join(QStringLiteral(", "))));
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
    request.motionMode = MotionMode::Continuous;
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
    const QString officialWord = QString::fromUtf8("\345\256\230\347\275\221");

    QCOMPARE(detectionTypeLabel(DetectionType::Measurement), measurementText);
    QCOMPARE(lensTypeLabel(LensType::BiTelecentric), biTelecentricText);

    bool hasDecodedOfficialUseCase = false;
    QStringList decodedLightUseCases;
    for (const LightSpec &light : m_catalog.lights())
    {
        hasDecodedOfficialUseCase = hasDecodedOfficialUseCase
            || light.bestFor.contains(officialWord);
        QStringList codepoints;
        for (const QChar ch : light.bestFor)
            codepoints.append(QString::number(ch.unicode(), 16));
        decodedLightUseCases.append(light.model + QStringLiteral("=") + codepoints.join(QLatin1Char(' ')));
    }
    QVERIFY2(hasDecodedOfficialUseCase, qPrintable(decodedLightUseCases.join(QStringLiteral(" | "))));
}

void SelectionEngineTest::threeDCameraCatalogLoadsFromResource()
{
    ThreeDCameraRepository repository;
    QString error;
    QVERIFY2(repository.loadFromResource(QStringLiteral(":/data/three_d_cameras.json"), &error), qPrintable(error));
    QCOMPARE(threeDTechnologyFromLabel(QString::fromUtf8("光谱共焦")), ThreeDTechnology::SpectralConfocal);
    QCOMPARE(threeDTechnologyFromLabel(QStringLiteral("Spectral confocal")), ThreeDTechnology::SpectralConfocal);
    QVERIFY(threeDTechnologyLabels().contains(threeDTechnologyLabel(ThreeDTechnology::SpectralConfocal)));

    QSet<QString> brands;
    int lmiCount = 0;
    int keyenceCount = 0;
    int sinceVisionCount = 0;
    int keyenceSpectralConfocalCount = 0;
    int sinceVisionSpectralConfocalCount = 0;
    bool hasXRepeatability = false;
    bool hasProfileDataInterval = false;
    bool hasZLinearity = false;
    bool hasReferenceDistance = false;
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
        if (camera.technology == ThreeDTechnology::SpectralConfocal) {
            if (camera.manufacturer == QString::fromUtf8("基恩士"))
                ++keyenceSpectralConfocalCount;
            if (camera.manufacturer == QString::fromUtf8("深视智能"))
                ++sinceVisionSpectralConfocalCount;
            if (camera.manufacturer == QString::fromUtf8("基恩士") || camera.manufacturer == QString::fromUtf8("深视智能")) {
                QCOMPARE(camera.sourceDate, QStringLiteral("2026-06-24"));
                QVERIFY2(threeDHasValue(camera.zMeasurementRangeMm), qPrintable(camera.model));
                QVERIFY2(threeDHasValue(camera.zResolutionUm), qPrintable(camera.model));
                QVERIFY2(threeDHasValue(camera.measurementAccuracyUm), qPrintable(camera.model));
                QVERIFY2(threeDHasValue(camera.xyResolutionUm), qPrintable(camera.model));
                QVERIFY2(!threeDHasValue(camera.xFovReferenceMm), qPrintable(camera.model));
                QVERIFY2(!threeDHasValue(camera.yFovReferenceMm), qPrintable(camera.model));
            }
        }
        hasXRepeatability = hasXRepeatability || threeDHasValue(camera.xRepeatabilityUm);
        hasProfileDataInterval = hasProfileDataInterval || threeDHasValue(camera.profileDataIntervalUm);
        hasZLinearity = hasZLinearity || threeDHasValue(camera.zLinearityPercentOfRange);
        hasReferenceDistance = hasReferenceDistance || threeDHasValue(camera.referenceDistanceMm);
    }

    QVERIFY(brands.contains(QStringLiteral("LMI")));
    QVERIFY(lmiCount >= 63);
    QVERIFY(brands.contains(QString::fromUtf8("深视智能")));
    QVERIFY(sinceVisionCount >= 50);
    QVERIFY(brands.contains(QString::fromUtf8("基恩士")));
    QVERIFY(keyenceCount >= 32);
    QVERIFY(keyenceSpectralConfocalCount >= 30);
    QVERIFY(sinceVisionSpectralConfocalCount >= 9);
    QVERIFY(brands.contains(QString::fromUtf8("海康机器人")));
    QVERIFY2(hasXRepeatability, "Expected at least one camera with X repeatability.");
    QVERIFY2(hasProfileDataInterval, "Expected at least one camera with X data interval.");
    QVERIFY2(hasZLinearity, "Expected at least one camera with Z linearity.");
    QVERIFY2(hasReferenceDistance, "Expected at least one camera with reference distance.");
}

void SelectionEngineTest::threeDCameraUserCatalogPersists()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());

    ThreeDCameraRepository repository;
    repository.setStorageDirectory(storage.path());
    QString error;
    QVERIFY2(repository.loadFromResource(QStringLiteral(":/data/three_d_cameras.json"), &error), qPrintable(error));
    const int builtInCount = repository.cameras().size();

    ThreeDCameraSpec custom;
    custom.manufacturer = QStringLiteral("Unit3D");
    custom.series = QStringLiteral("Custom");
    custom.model = QStringLiteral("UT-1000");
    custom.technologyLabel = threeDTechnologyLabel(ThreeDTechnology::LineLaserProfile);
    custom.technology = ThreeDTechnology::LineLaserProfile;
    custom.status = QString::fromUtf8("用户录入");
    custom.sourceDate = QStringLiteral("2026-06-08");
    custom.xFovReferenceMm = 120.0;
    custom.zMeasurementRangeMm = 30.0;
    custom.zRepeatabilityUm = 2.0;
    custom.profileDataIntervalUm = 20.0;
    custom.profilePoints = 1600;
    custom.scanRateMaxHz = 5000.0;
    custom.encoderRateMaxHz = 100000.0;
    custom.exposureTimeMinUs = 5.0;
    custom.exposureTimeMaxUs = 900.0;
    custom.supportsEncoder = 1;
    custom.supportsExternalTrigger = 1;
    custom.interfaces = QStringList() << QStringLiteral("GigE") << QString::fromUtf8("编码器");
    custom.materialScenarios = QStringList() << QString::fromUtf8("金属");
    QVERIFY2(repository.addCamera(custom, &error), qPrintable(error));
    QCOMPARE(repository.cameras().size(), builtInCount + 1);

    ThreeDCameraRepository reloaded;
    reloaded.setStorageDirectory(storage.path());
    QVERIFY2(reloaded.loadFromResource(QStringLiteral(":/data/three_d_cameras.json"), &error), qPrintable(error));
    int customIndex = -1;
    for (int i = 0; i < reloaded.cameras().size(); ++i) {
        if (reloaded.cameras().at(i).manufacturer == custom.manufacturer
            && reloaded.cameras().at(i).model == custom.model) {
            customIndex = i;
            break;
        }
    }
    QVERIFY(customIndex >= 0);
    QVERIFY(reloaded.cameras().at(customIndex).userDefined);
    QCOMPARE(reloaded.cameras().at(customIndex).scanRateMaxHz, 5000.0);

    ThreeDCameraSpec edited = reloaded.cameras().at(customIndex);
    edited.zMeasurementRangeMm = 45.0;
    QVERIFY2(reloaded.updateCamera(customIndex, edited, &error), qPrintable(error));

    ThreeDCameraRepository editedReloaded;
    editedReloaded.setStorageDirectory(storage.path());
    QVERIFY2(editedReloaded.loadFromResource(QStringLiteral(":/data/three_d_cameras.json"), &error), qPrintable(error));
    int editedIndex = -1;
    for (int i = 0; i < editedReloaded.cameras().size(); ++i) {
        if (editedReloaded.cameras().at(i).manufacturer == custom.manufacturer
            && editedReloaded.cameras().at(i).model == custom.model) {
            editedIndex = i;
            break;
        }
    }
    QVERIFY(editedIndex >= 0);
    QCOMPARE(editedReloaded.cameras().at(editedIndex).zMeasurementRangeMm, 45.0);
    QVERIFY2(editedReloaded.removeCamera(editedIndex, &error), qPrintable(error));

    ThreeDCameraRepository removedReloaded;
    removedReloaded.setStorageDirectory(storage.path());
    QVERIFY2(removedReloaded.loadFromResource(QStringLiteral(":/data/three_d_cameras.json"), &error), qPrintable(error));
    QCOMPARE(removedReloaded.cameras().size(), builtInCount);
}

void SelectionEngineTest::threeDCameraCorruptUserCatalogIsQuarantined()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());

    const QString userPath = QDir(storage.path()).filePath(QStringLiteral("three_d_cameras.json"));
    QFile file(userPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("{\"cameras\":[");
    file.close();

    ThreeDCameraRepository repository;
    repository.setStorageDirectory(storage.path());
    QString error;
    QVERIFY(!repository.loadFromResource(QStringLiteral(":/data/three_d_cameras.json"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFileInfo::exists(userPath));

    const QStringList quarantined = QDir(storage.path()).entryList(
        QStringList() << QStringLiteral("three_d_cameras.json.corrupt-*"),
        QDir::Files);
    QCOMPARE(quarantined.size(), 1);
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

void SelectionEngineTest::threeDMotionSamplingMatchesSpreadsheetExample()
{
    ThreeDMotionSamplingInput input;
    input.scanDistanceMm = 300.0;
    input.profileIntervalMm = 0.05;
    input.axisTravelMm = 10.0;
    input.pulseCount = 10000;
    input.refinementPoints = 50;
    input.samplingRateHz = 1000.0;
    input.safetyFactor = 0.8;
    input.overrideXPixelPitchMm = 0.005;

    const ThreeDMotionSamplingResult result = ThreeDCalculation::estimateMotionSampling(input);
    QVERIFY(result.valid);
    QCOMPARE(result.status, ThreeDCalculationStatus::Warning);
    QCOMPARE(result.profileCount, 6000.0);
    QCOMPARE(result.pulseIntervalMm, 0.001);
    QCOMPARE(result.yPixelPitchMm, 0.05);
    QCOMPARE(result.xPixelPitchMm, 0.005);
    QCOMPARE(result.maxAxisSpeedMmS, 40.0);
    QCOMPARE(result.xyPitchRatio, 10.0);
    QVERIFY(result.usesManualXPixelPitch);
}

void SelectionEngineTest::threeDMotionSamplingUsesCameraDataAndFlagsRisks()
{
    ThreeDCameraSpec camera;
    camera.model = QStringLiteral("TEST-3D");
    camera.profileDataIntervalUm = 5.0;
    camera.scanRateMaxHz = 800.0;

    ThreeDMotionSamplingInput input;
    input.scanDistanceMm = 300.0;
    input.profileIntervalMm = 0.05;
    input.axisTravelMm = 10.0;
    input.pulseCount = 10000;
    input.refinementPoints = 50;
    input.samplingRateHz = 1000.0;
    input.safetyFactor = 0.8;

    const ThreeDMotionSamplingResult result = ThreeDCalculation::estimateMotionSampling(input, &camera);
    QVERIFY(!result.valid);
    QCOMPARE(result.status, ThreeDCalculationStatus::Infeasible);
    QCOMPARE(result.xPixelPitchMm, 0.005);
    QVERIFY(!result.usesManualXPixelPitch);
    QVERIFY(result.xPixelPitchKnown);
    QVERIFY(result.samplingRateKnown);
    QVERIFY(!result.samplingRateWithinCameraLimit);
    QVERIFY(!result.risks.isEmpty());

    input.profileIntervalMm = 0.0;
    const ThreeDMotionSamplingResult invalid = ThreeDCalculation::estimateMotionSampling(input, &camera);
    QVERIFY(!invalid.valid);
    QCOMPARE(invalid.status, ThreeDCalculationStatus::InvalidInput);
    QVERIFY(!invalid.risks.isEmpty());
}

void SelectionEngineTest::threeDMotionSamplingChecksTriggerExposureAndEncoder()
{
    ThreeDCameraSpec camera;
    camera.model = QStringLiteral("TRIGGER-3D");
    camera.profileDataIntervalUm = 50.0;
    camera.scanRateMaxHz = 1500.0;
    camera.encoderRateMaxHz = 60000.0;
    camera.exposureTimeMinUs = 5.0;
    camera.exposureTimeMaxUs = 1200.0;
    camera.supportsExternalTrigger = 1;

    ThreeDMotionSamplingInput input;
    input.scanDistanceMm = 300.0;
    input.profileIntervalMm = 0.05;
    input.targetAxisSpeedMmS = 40.0;
    input.axisTravelMm = 10.0;
    input.pulseCount = 10000;
    input.refinementPoints = 50;
    input.samplingRateHz = 1000.0;
    input.safetyFactor = 0.8;
    input.triggerMode = ThreeDTriggerMode::Encoder;
    input.encoderPulseFrequencyHz = 50000.0;
    input.encoderPulsesPerProfile = 50;
    input.exposureTimeUs = 900.0;
    input.readoutMarginUs = 3.0;

    const ThreeDMotionSamplingResult result = ThreeDCalculation::estimateMotionSampling(input, &camera);
    QVERIFY(result.valid);
    QCOMPARE(result.effectiveProfileRateHz, 1000.0);
    QCOMPARE(result.requiredProfileRateHz, 1000.0);
    QCOMPARE(result.profilePeriodUs, 1000.0);
    QCOMPARE(result.maxExposureTimeUs, 997.0);
    QCOMPARE(result.encoderProfileIntervalMm, 0.05);
    QCOMPARE(result.encoderAxisSpeedMmS, 50.0);
    QVERIFY(result.exposureWithinProfilePeriod);
    QVERIFY(result.encoderRateWithinCameraLimit);
    QVERIFY(result.effectiveRateMeetsTarget);
    QVERIFY(result.samplingRateWithinCameraLimit);

    input.exposureTimeUs = 1200.0;
    const ThreeDMotionSamplingResult exposureRisk = ThreeDCalculation::estimateMotionSampling(input, &camera);
    QVERIFY(!exposureRisk.exposureWithinProfilePeriod);
    QCOMPARE(exposureRisk.status, ThreeDCalculationStatus::Infeasible);
    QVERIFY(!exposureRisk.risks.isEmpty());

    input.exposureTimeUs = 900.0;
    input.encoderPulseFrequencyHz = 80000.0;
    const ThreeDMotionSamplingResult encoderRisk = ThreeDCalculation::estimateMotionSampling(input, &camera);
    QVERIFY(!encoderRisk.encoderRateWithinCameraLimit);
    QCOMPARE(encoderRisk.status, ThreeDCalculationStatus::Infeasible);
    QVERIFY(!encoderRisk.risks.isEmpty());
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
    const QString key = QStringLiteral("VS2-eyJwcm9kdWN0SWQiOiJWaXNpb25TZWxlY3QiLCJsaWNlbnNlZSI6IlVuaXQgVGVzdCIsInNlcmlhbCI6IlVULTAwMSIsIm1hY2hpbmVDb2RlIjoiQUJDRC1FRkdILUlKS0wtTU5PUCIsImlzc3VlZEF0IjoiMjAyNi0wMS0wMSIsImV4cGlyZXNBdCI6IjIwOTktMTItMzEiLCJmZWF0dXJlcyI6WyJzdGFuZGFyZCJdfQ==-ZIDJ4aU7bVs3mAFTiJUivtS+xcmM/xOSZh3BHrbh28lhTIw/p0wk2OVJKMJemq6sPcyBjSNwTq/3+9WwSTfh8D76EbYl44TpDgdIWCqFUfBKOM2u+mg8+Yvp2Rf85mUaDphOxpSVxFoPO5PZ8d8odVmUiw92bBYtje7vdxmG5rHKkhKr7r2rlKKE2cbuGrozDKjAfjVJSV3Q8SDnc6vbahBQeHKQLT0gYJI2bHSVokH+cV6UyzObSXmSS7U5jQjtTKIuL8qUvfWP306SGb/89EO+kb7dVqQB+0hQLmqhYmimow3nFY2KlWpmlDOHzIAVgny6mezxg+JDoFWT/2dkJQ==");

    LicenseStatus status = manager.validateKeyForMachine(key, QStringLiteral("ABCD-EFGH-IJKL-MNOP"), QDate(2026, 5, 31));
    QCOMPARE(status.code, LicenseStatusCode::Valid);
    QCOMPARE(status.info.licensee, QStringLiteral("Unit Test"));

    status = manager.validateKeyForMachine(key, QStringLiteral("0000-0000-0000-0000"), QDate(2026, 5, 31));
    QCOMPARE(status.code, LicenseStatusCode::MachineMismatch);

    status = manager.validateKeyForMachine(key, QStringLiteral("ABCD-EFGH-IJKL-MNOP"), QDate(2100, 1, 1));
    QCOMPARE(status.code, LicenseStatusCode::Expired);

    status = manager.validateKeyForMachine(key, QStringLiteral("ABCD-EFGH-IJKL-MNOP"),
                                           QDate(2026, 5, 31), QDate(2026, 6, 1));
    QCOMPARE(status.code, LicenseStatusCode::ClockRollback);

    status = manager.validateKeyForMachine(key, QStringLiteral("ABCD-EFGH-IJKL-MNOP"), QDate(2025, 12, 31));
    QCOMPARE(status.code, LicenseStatusCode::ClockRollback);

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

    LicenseIssueResult result;
    QVERIFY2(issuer.issue(request, &result, &error), qPrintable(error));
    QVERIFY(result.licenseKey.startsWith(QStringLiteral("VS2-")));
    QCOMPARE(result.normalizedMachineCode, QStringLiteral("ABCD-EFGH-IJKL-MNOP"));
    QVERIFY(result.payloadJson.contains("\"licensee\":\"Issuer Test\""));
    QVERIFY(!result.payloadJson.contains("\"features\""));

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
    LanguageGuard language;
    QVERIFY(language.setLanguage(QStringLiteral("en_US")));

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
    LanguageGuard language;
    LanguageManager &manager = LanguageManager::instance();
    QVERIFY(manager.availableLanguages().contains(QStringLiteral("zh_CN")));
    QVERIFY(manager.availableLanguages().contains(QStringLiteral("en_US")));
    QVERIFY(manager.setLanguage(QStringLiteral("en_US")));
    QCOMPARE(manager.currentLanguage(), QStringLiteral("en_US"));
    QVERIFY(manager.setLanguage(QStringLiteral("zh_CN")));
    QCOMPARE(manager.currentLanguage(), QStringLiteral("zh_CN"));
}

void SelectionEngineTest::generatedDiagnosticsFollowLanguage()
{
    LanguageGuard language;
    QVERIFY(language.setLanguage(QStringLiteral("en_US")));

    SelectionRequest request;
    request.objectWidthMm = 20.0;
    request.objectHeightMm = 20.0;
    request.placementMarginMm = 2.0;
    request.requiredFps = 20.0;
    request.motionMode = MotionMode::Continuous;
    request.motionSpeedMmS = 100.0;

    SelectionEngine engine;
    const QVector<SelectionResult> results = engine.select(request, m_catalog.cameras(), m_catalog.lenses(), m_catalog.lights(), 3);
    QVERIFY(!results.isEmpty());
    const SelectionResult &top = results.first();
    QStringList selectionParts;
    selectionParts << top.schemeTitle << top.formulaSummary << top.hardFailures << top.score.reasons << top.score.risks;
    const QString selectionText = selectionParts.join(QStringLiteral("; "));
    QVERIFY2(!containsCjk(selectionText), qPrintable(selectionText));

    const QVector<LensCalculationEstimate> lensEstimates = CalculationAssistant::estimateLenses(request, top.camera, m_catalog.lenses(), 5);
    QVERIFY(!lensEstimates.isEmpty());
    QStringList lensText;
    for (const LensCalculationEstimate &estimate : lensEstimates) {
        lensText << estimate.formulaSummary << estimate.reasons << estimate.risks;
        for (const QString &part : estimate.reasons)
            QVERIFY2(!containsCjk(part), qPrintable(part));
        for (const QString &part : estimate.risks)
            QVERIFY2(!containsCjk(part), qPrintable(part));
    }
    QVERIFY2(!containsCjk(lensText.join(QStringLiteral("; "))), qPrintable(lensText.join(QStringLiteral("; "))));

    PureCalculationInput input;
    input.request = request;
    input.camera = top.camera;
    input.lens = top.lens;
    input.light = top.light;
    input.telecentricMode = top.lens.isTelecentric();
    const PureCalculationResult pure = CalculationAssistant::estimatePure(input);
    const QString pureText = (QStringList() << pure.lensFormulaSummary << pure.reasons << pure.risks).join(QStringLiteral("; "));
    QVERIFY2(!containsCjk(pureText), qPrintable(pureText));

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
    QVERIFY(!repo.loadLightCsv(file.fileName(), &error));
    QVERIFY2(error.contains(QStringLiteral("Light data is invalid")), qPrintable(error));
    QVERIFY2(!containsCjk(error), qPrintable(error));

    const QString dynamicValueError = CoreI18n::localizedDiagnostic(
        QString::fromUtf8("中文品牌 光源数据无效：型号和有效照明尺寸必须有效"));
    QVERIFY2(dynamicValueError.contains(QStringLiteral("Light data is invalid")), qPrintable(dynamicValueError));
    QVERIFY2(dynamicValueError.contains(QString::fromUtf8("中文品牌")), qPrintable(dynamicValueError));
}

void SelectionEngineTest::selectionUsesExplicitLanguageSnapshot()
{
    LanguageGuard language;
    QVERIFY(language.setLanguage(QStringLiteral("zh_CN")));

    SelectionEngine engine;
    const QVector<SelectionResult> results = engine.select(
        SelectionRequest(), m_catalog.cameras(), m_catalog.lenses(), m_catalog.lights(), 3,
        QStringLiteral("en_US"));
    QVERIFY(!results.isEmpty());
    const SelectionResult &top = results.first();
    QStringList parts;
    parts << top.schemeTitle << top.formulaSummary << top.hardFailures
          << top.score.reasons << top.score.risks;
    const QString text = parts.join(QStringLiteral("; "));
    QVERIFY2(!containsCjk(text), qPrintable(text));
}

void SelectionEngineTest::licenseIssuerErrorsFollowLanguage()
{
    LanguageGuard language;

    LicenseIssuer issuer;
    LicenseIssueRequest request;
    LicenseIssueResult result;
    QString error;

    QVERIFY(language.setLanguage(QStringLiteral("zh_CN")));
    QVERIFY(!issuer.issue(request, &result, &error));
    QVERIFY2(error.contains(QString::fromUtf8("私钥")), qPrintable(error));
    QVERIFY2(!error.contains(QStringLiteral("Private key")), qPrintable(error));

    QVERIFY(language.setLanguage(QStringLiteral("en_US")));
    QVERIFY(!issuer.issue(request, &result, &error));
    QVERIFY2(error.contains(QStringLiteral("Private key")), qPrintable(error));
    QVERIFY2(!containsCjk(error), qPrintable(error));

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
    LanguageGuard language;
    QVERIFY(language.setLanguage(QStringLiteral("zh_CN")));

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
    request.projectNotes = QString::fromUtf8("这是一个需要自动换行的长项目备注，用于验证 PDF 段落会按实际字体测量高度，")
        + QString::fromUtf8("并完整保留项目上下文。").repeated(40);
    SelectionEngine engine;
    const QVector<SelectionResult> results = engine.select(request, m_catalog.cameras(), m_catalog.lenses(), m_catalog.lights(), 5);
    QVERIFY(!results.isEmpty());

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("report.pdf"));
    {
        QFile existing(path);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.write("previous report");
    }

    PdfReportWriter writer;
    QString error;
    QVERIFY2(writer.write(path, request, results, &error), qPrintable(error));
    QVERIFY(QFileInfo(path).size() > 1000);
}

QTEST_MAIN(SelectionEngineTest)
void SelectionEngineTest::projectReviewNumericAndGeometryRegressions()
{
    CameraSpec camera; camera.resolutionX=100000;camera.resolutionY=100000;
    QCOMPARE(camera.megapixels(),10000.0);
    LensSpec lens;lens.distortionPercent=-2;
    QCOMPARE(SelectionEngine::distortionErrorUm(lens,100,100),2000.0);
    ThreeDCameraSpec spec;spec.referenceDistanceMm=150;spec.workingDistanceMinMm=100;spec.workingDistanceMaxMm=200;
    spec.xFovNearMm=100;spec.xFovReferenceMm=150;spec.xFovFarMm=200;
    ThreeDCameraRequirement req;req.workingDistanceMm=100;req.targetXCoverageMm=180;
    QCOMPARE(ThreeDCameraMatcher().match(req,{spec}).first().status,ThreeDMatchStatus::NoMatch);
    req.workingDistanceMm=125;
    QCOMPARE(ThreeDCameraMatcher().match(req,{spec}).first().status,ThreeDMatchStatus::MissingData);
    req.workingDistanceMm=200;
    QCOMPARE(ThreeDCameraMatcher().match(req,{spec}).first().status,ThreeDMatchStatus::Match);
    spec.workingDistanceMinMm=spec.workingDistanceMaxMm=-1;spec.referenceDistanceMm=100;
    req.workingDistanceMm=110;req.targetXCoverageMm=-1;
    QCOMPARE(ThreeDCameraMatcher().match(req,{spec}).first().status,ThreeDMatchStatus::MissingData);
    ThreeDMotionSamplingInput input;input.scanDistanceMm=1;input.profileIntervalMm=.3;
    QCOMPARE(ThreeDCalculation::estimateMotionSampling(input).profileCount,qint64(4));
    for(auto mode:{ThreeDTriggerMode::Encoder,ThreeDTriggerMode::ExternalTrigger}) {
        input=ThreeDMotionSamplingInput();input.triggerMode=mode;
        spec.supportsEncoder=spec.supportsExternalTrigger=0;
        const auto unsupported=ThreeDCalculation::estimateMotionSampling(input,&spec);
        QVERIFY(!unsupported.valid);QCOMPARE(unsupported.status,ThreeDCalculationStatus::Infeasible);
        spec.supportsEncoder=spec.supportsExternalTrigger=-1;
        const auto unknown=ThreeDCalculation::estimateMotionSampling(input,&spec);
        QVERIFY(unknown.valid);QCOMPARE(unknown.status,ThreeDCalculationStatus::Warning);
    }
    SelectionRequest request;request.heightVariationMm=1;
    lens.dofConditionsConfirmed=true;
    const auto checks=CandidateValidator::lens(request,camera,lens,24,24,5,1.2);
    QCOMPARE(checks[CandidateCheck::DepthOfField],CandidateCheckState::Failed);
    Parameters::SystemInput system;system.heightVariationMm=1;system.dofMm=1.2;system.dofConditionsConfirmed=true;
    for(const auto &check:Parameters::checkSystem(system).checks) if(check.key=="dof") QCOMPARE(check.status,CalculationStatus::Failed);
}

void SelectionEngineTest::projectReviewImportPreservesDataAndMetadata()
{
    QTemporaryDir directory;CatalogRepository repo;repo.setStorageDirectory(directory.path());QString error;
    QVERIFY2(repo.initializeDatabase(&error),qPrintable(error));
    const int before=repo.productCount(CatalogDomain::Camera,&error);
    QFile csv(directory.filePath("incoming.csv"));QVERIFY(csv.open(QIODevice::WriteOnly));
    csv.write("model,manufacturer,resolution_x,resolution_y,pixel_size_um,color_mode,shutter_type,max_fps,interface,bandwidth_mbps,bit_depth,lens_mount,bandwidth_source\nAUDIT-MONO,audit,2000,2000,5,Mono12,Global,20,GigE,120,12,C,specified\n");csv.close();
    CatalogImportPreview preview;
    QVERIFY2(repo.previewImport(CatalogDomain::Camera,csv.fileName(),CatalogImportMode::Merge,&preview,&error),qPrintable(error));
    QCOMPARE(preview.added,1);QCOMPARE(preview.removed,0);
    QString backup;
    QVERIFY2(repo.importCsv(CatalogDomain::Camera,csv.fileName(),CatalogImportMode::Merge,&backup,&error),qPrintable(error));
    QVERIFY(QFileInfo::exists(backup));QCOMPARE(repo.productCount(CatalogDomain::Camera,&error),before+1);
    CatalogQuery query;query.manufacturer="audit";
    const auto camera=repo.queryCameras(query,&error).items.first();
    QCOMPARE(camera.colorMode,QStringLiteral("Mono"));QCOMPARE(camera.pixelFormat,QStringLiteral("Mono12"));
    QCOMPARE(camera.bandwidthSource,QStringLiteral("specified"));QCOMPARE(SelectionEngine::framePayloadMB(camera),8.0);
    QVERIFY(repo.previewImport(CatalogDomain::Camera,csv.fileName(),CatalogImportMode::Replace,&preview,&error));
    QCOMPARE(preview.updated,1);QCOMPARE(preview.removed,before);
    // 替换必须通过显式模式，合并和替换都保留可恢复备份。
    QVERIFY(repo.importCsv(CatalogDomain::Camera,csv.fileName(),CatalogImportMode::Replace,&backup,&error));
    QCOMPARE(repo.productCount(CatalogDomain::Camera,&error),1);
    const QString exported=directory.filePath("roundtrip.csv");QVERIFY(repo.exportCameraCsvByQuery(exported,query,&error));
    QVERIFY(repo.loadCameraCsv(exported,&error));
    QCOMPARE(repo.queryCameras(query,&error).items.first().pixelFormat,QStringLiteral("Mono12"));
    CameraSpec estimated = camera;
    estimated.bandwidthSource = QStringLiteral("estimated");
    CandidateChecks checks;
    CandidateValidator::camera(SelectionRequest(), estimated, 1000, 120, &checks);
    QCOMPARE(checks[CandidateCheck::Bandwidth], CandidateCheckState::Unknown);
    QCOMPARE(checks[CandidateCheck::PixelFormat], CandidateCheckState::Passed);
    // 8 MB/帧 × 20 fps，再保留既有 10% 传输开销。
    QCOMPARE(CalculationAssistant::estimateCameras(SelectionRequest(), {camera}, 1).first().bandwidthRequiredMBps, 176.0);
    PureCalculationInput input; input.camera = camera;
    QVERIFY(!CalculationAssistant::estimatePure(input).payloadEstimated);
    LensSpec lens = repo.queryLenses(CatalogQuery(), &error).items.first();
    lens.manufacturer = QStringLiteral("audit"); lens.model = QStringLiteral("CONFIRMED-DOF");
    lens.dofMm = 1.2; lens.dofConditionsConfirmed = true;
    QVERIFY2(repo.addLens(lens, &error), qPrintable(error));
    const auto storedLens = repo.queryLenses(query, &error);
    QVERIFY(!storedLens.items.isEmpty());
    QVERIFY(storedLens.items.first().dofConditionsConfirmed);
    const QString lensExport = directory.filePath(QStringLiteral("lenses-roundtrip.csv"));
    QVERIFY(repo.exportLensCsvByQuery(lensExport, query, &error));
    QVERIFY(repo.loadLensCsv(lensExport, &error));
    QVERIFY(repo.queryLenses(query, &error).items.first().dofConditionsConfirmed);
}

void SelectionEngineTest::projectReviewBuiltinUpdateAndRecall()
{
    QTemporaryDir directory;CatalogRepository repo;repo.setStorageDirectory(directory.path());QString error;
    QVERIFY2(repo.initializeDatabase(&error),qPrintable(error));
    CatalogQuery all;all.limit=1;
    const auto original=repo.queryCameras(all,&error);
    const auto id=original.ids.first();const auto fps=original.items.first().maxFps;
    const QString connection="review-upgrade";
    {
        auto db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(directory.filePath("catalog.db"));QVERIFY(db.open());
        QSqlQuery q(db);QVERIFY(q.exec(QStringLiteral("UPDATE camera_products SET max_fps=0.1,source_version='1' WHERE id=%1").arg(id)));
        q.finish();db.close();
    }
    QSqlDatabase::removeDatabase(connection);
    QVERIFY2(repo.initializeDatabase(&error),qPrintable(error));
    CameraSpec camera;QVERIFY(repo.cameraById(id,&camera,&error));QCOMPARE(camera.maxFps,fps);
    camera.maxFps=321;QVERIFY(repo.updateCameraById(id,camera,&error));QVERIFY(repo.initializeDatabase(&error));
    QVERIFY(repo.cameraById(id,&camera,&error));QCOMPARE(camera.maxFps,321.0);
    QFile csv(directory.filePath("cameras.csv"));QVERIFY(csv.open(QIODevice::WriteOnly));
    QByteArray data="model,manufacturer,resolution_x,resolution_y,pixel_size_um,color_mode,shutter_type,max_fps,interface,bandwidth_mbps,bit_depth,lens_mount\n";
    for(int i=0;i<301;i++) data+=QString("AUDIT-%1,audit,2000,2000,5,Mono8,Global,100,GigE,120,8,%2\n").arg(i,3,10,QChar('0')).arg(i==300?"C":"F").toUtf8();
    csv.write(data);csv.close();QVERIFY(repo.loadCameraCsv(csv.fileName(),&error));
    QFile lensCsv(directory.filePath("lenses.csv"));QVERIFY(lensCsv.open(QIODevice::WriteOnly));
    lensCsv.write("model,manufacturer,lens_type,lens_mount,distortion_percent,image_circle_mm,pmag,nominal_wd_mm,wd_tolerance_mm\nAUDIT-LENS,audit,ObjectTelecentric,C,0,30,1,100,10\n");lensCsv.close();
    QVERIFY2(repo.loadLensCsv(lensCsv.fileName(),&error),qPrintable(error));
    SelectionRequest request;request.objectWidthMm=request.objectHeightMm=10;request.placementMarginMm=0;request.minFeatureUm=request.measurementToleranceUm=50;
    request.workingDistanceMm=100;request.heightVariationMm=0;
    const auto results=SelectionService(&repo).select(request,20,&error);
    QVERIFY2(error.isEmpty(),qPrintable(error));QVERIFY(!results.isEmpty());QVERIFY(results.first().hardConstraintsPassed);
    QCOMPARE(results.first().camera.model,QStringLiteral("AUDIT-300"));QCOMPARE(results.first().searchedCameras,301);
}

void SelectionEngineTest::projectReviewDiagnosticsAndExports()
{
    SelectionResult source;source.hasDiagnosticSource=true;source.diagnosticScheme=QString::fromUtf8("远心镜头方案");
    source.checks[CandidateCheck::Mount]=CandidateCheckState::Failed;source.hardConstraintsPassed=false;
    source.diagnosticRisks={QString::fromUtf8("远心镜头缺少远心度数据，无法估算高度波动带来的残余视差")};
    const auto english=localizedResult(source,"en_US");
    QVERIFY(!containsCjk(english.hardFailures.join(';')));QVERIFY(!containsCjk(english.score.risks.join(';')));
    class FailedDevice final:public QIODevice {
    public:FailedDevice(){open(QIODevice::WriteOnly);}
    protected:qint64 readData(char*,qint64)override{return -1;}qint64 writeData(const char*,qint64)override{setErrorString("injected write failure");return -1;}
    } failed;
    QString error;SelectionRequest request;
    QVERIFY(!BomCsvWriter().writeToDevice(&failed,request,{source},&error));QVERIFY(!error.isEmpty());
    QBuffer buffer;QVERIFY(buffer.open(QIODevice::WriteOnly));source.camera.model="FULL-MODEL-ABCDEFGHIJKLMNOPQRSTUVWXYZ-0123456789";
    QVERIFY(BomCsvWriter().writeToDevice(&buffer,request,{source},&error));
    QVERIFY(buffer.data().contains("failed"));QVERIFY(buffer.data().contains(source.camera.model.toUtf8()));QVERIFY(buffer.data().contains("request_json"));
    const QString capture=qEnvironmentVariable("VISIONSELECT_REPORT_CAPTURE");
    if(!capture.isEmpty()) {
        request.projectNotes=QString::fromUtf8("分页回归：完整型号、方案状态、缺失规格和长备注均应保留。\n").repeated(25);
        source.camera.manufacturer="Audit Camera";source.lens.model=source.camera.model;source.lens.manufacturer="Audit Lens";
        QVERIFY2(PdfReportWriter().write(capture,request,{source,source},&error),qPrintable(error));
    }
}

#include "test_selection.moc"
