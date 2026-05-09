#include "catalog/CatalogRepository.h"
#include "report/PdfReportWriter.h"
#include "selection/SelectionEngine.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QtTest/QtTest>

class SelectionEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void telecentricMeasurementWins();
    void largeFovPrefersFixedFocal();
    void motionPrefersGlobalShutter();
    void reflectiveSurfaceGetsCoaxialOrDome();
    void chineseTextIsUnicode();
    void invalidTelecentricCsvFails();
    void pdfReportWrites();

private:
    CatalogRepository m_catalog;
};

void SelectionEngineTest::initTestCase()
{
    QString error;
    QVERIFY2(m_catalog.loadDefaults(&error), qPrintable(error));
    QVERIFY(!m_catalog.cameras().isEmpty());
    QVERIFY(!m_catalog.lenses().isEmpty());
    QVERIFY(!m_catalog.lights().isEmpty());
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
    QString error;
    QVERIFY(!repo.loadLensCsv(file.fileName(), &error));
    QVERIFY2(error.contains(QStringLiteral("PMAG")), qPrintable(error));
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
