#include "ui/MainWindow.h"
#include "ui/ResultPresentation.h"
#include "ui/UiSettings.h"
#include "ui/UiThemeManager.h"
#include "i18n/LanguageManager.h"
#include "ui/pages/CatalogPage.h"
#include "ui/pages/InputPage.h"
#include "ui/pages/ResultsPage.h"
#include "ui/UiHelpers.h"
#include "ui/pages/PureCalculationPage.h"
#include "ui/pages/ThreeDCameraPage.h"
#include "ui/ParameterNumberField.h"
#include "ui/ParameterCatalogDialog.h"
#include "ui/ParameterUi.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QDir>
#include <QDialog>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QJsonDocument>
#include <QLayout>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSplitter>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTabBar>
#include <QTextEdit>
#include <QTextBrowser>
#include <QTabWidget>
#include <QToolButton>
#include <QtTest>
#include <memory>

class VisionSelectUiTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void relativeMatchAndRiskPresentation();
    void noPositiveScoreHasNoMatchPercentage();
    void inputPageUsesSingleRunPathAndAccessibleFields();
    void f9ActionUsesInputPageRunSignal();
    void settingsRoundTripAndCorruptStateFallback();
    void catalogToolbarInitialStateAndMenuSignals();
    void navigationSupportsKeyboardFocus();
    void redesignedWorkspaceKeepsInputsAndNavigation();
    void parameterFieldSupportsEmptyAndUnitConversion();
    void parameterWorkbenchKeepsTasksAndIndependentSnapshots();
    void parameterWorkbenchImportsAndChecksUnknownSpecifications();
    void parameterCatalogPickersSupportSearchAndImport();
    void parameterWorkbenchLayoutLanguageAndPersistence();
    void threeDCameraPrioritizesResultsAndKeepsFilters();
    void calculationSelectionUsesKeyboardAndLensIdentity();
    void resultsSortNumericallyAndShowChecks();
    void resultSnapshotSurvivesInputNavigationAndLanguage();
    void navigationReusesPagesAndKeepsState();
    void riskSummaryUsesCurrentLanguageWithoutDuplicates();
};

void VisionSelectUiTests::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("VisionSelectUiTests"));
    QCoreApplication::setApplicationName(QStringLiteral("VisionSelectUiTests"));
    QSettings().clear();
    UiSettings::instance().initialize();
    UiThemeManager::instance().initialize(qobject_cast<QApplication *>(QCoreApplication::instance()));
}

void VisionSelectUiTests::relativeMatchAndRiskPresentation()
{
    QVector<SelectionResult> results(4);
    results[0].hardConstraintsPassed = true;
    results[0].score.score = 200.0;
    results[0].score.risks = {QStringLiteral("带宽风险"), QStringLiteral("带宽风险")};
    results[1].hardConstraintsPassed = true;
    results[1].score.score = 99.0;
    results[2].hardConstraintsPassed = true;
    results[2].score.score = -8.0;
    results[3].hardConstraintsPassed = false;
    results[3].score.score = 500.0;
    results[3].hardFailures = {QStringLiteral("视场不足")};
    results[3].score.risks = {QStringLiteral("视场不足"), QStringLiteral("接口风险")};

    const QVector<ResultPresentation> presentation = buildResultPresentations(results);
    QCOMPARE(presentation.size(), 4);
    QVERIFY(presentation[0].matchAvailable);
    QCOMPARE(presentation[0].relativeMatchPercent, 100);
    QCOMPARE(presentation[1].relativeMatchPercent, 50);
    QCOMPARE(presentation[2].relativeMatchPercent, 0);
    QVERIFY(!presentation[3].matchAvailable);
    QCOMPARE(presentation[0].riskItems, QStringList({QStringLiteral("带宽风险")}));
    QCOMPARE(presentation[3].riskItems,
             QStringList({QStringLiteral("视场不足"), QStringLiteral("接口风险")}));
    QCOMPARE(presentation[3].riskLevel, ResultRiskLevel::Error);
}

void VisionSelectUiTests::noPositiveScoreHasNoMatchPercentage()
{
    QVector<SelectionResult> results(2);
    results[0].score.score = 0.0;
    results[1].score.score = -1.0;
    const QVector<ResultPresentation> presentation = buildResultPresentations(results);
    QVERIFY(!presentation[0].matchAvailable);
    QVERIFY(!presentation[1].matchAvailable);
}

void VisionSelectUiTests::riskSummaryUsesCurrentLanguageWithoutDuplicates()
{
    const QString originalLanguage = LanguageManager::instance().currentLanguage();
    const auto restoreLanguage = qScopeGuard([&] { LanguageManager::instance().setLanguage(originalLanguage); });
    SelectionResult result;
    result.hardConstraintsPassed = false;
    result.checks[CandidateCheck::Mount] = CandidateCheckState::Failed;
    result.checks[CandidateCheck::LightCoverage] = CandidateCheckState::Unknown;
    result.hasDiagnosticSource = true;
    for (const QString &language : {QStringLiteral("zh_CN"), QStringLiteral("en_US"), QStringLiteral("zh_CN")}) {
        QVERIFY(LanguageManager::instance().setLanguage(language));
        result = localizedResult(result);
        const QString summary = UiHelpers::riskSummary(result);
        const auto failures = candidateCheckMessages(result.checks, CandidateCheckState::Failed);
        const auto unknowns = candidateCheckMessages(result.checks, CandidateCheckState::Unknown);
        QCOMPARE(summary.count(failures.first()), 1);
        QCOMPARE(summary.count(unknowns.first()), 1);
        QVERIFY(!summary.contains(QStringLiteral("Hard mismatch")));
        if (language == QLatin1String("en_US"))
            for (const auto character : summary) QVERIFY(character.unicode() < 0x4e00 || character.unicode() > 0x9fff);
    }
}

void VisionSelectUiTests::inputPageUsesSingleRunPathAndAccessibleFields()
{
    InputPage page;
    QPushButton *runButton = page.findChild<QPushButton *>(QStringLiteral("RunSelectionButton"));
    QVERIFY(runButton);
    QSignalSpy runSpy(&page, &InputPage::runSelectionRequested);
    QTest::mouseClick(runButton, Qt::LeftButton);
    QCOMPARE(runSpy.count(), 1);

    page.setBusy(true);
    QVERIFY(!runButton->isEnabled());
    page.setBusy(false);
    QVERIFY(runButton->isEnabled());

    const QList<QDoubleSpinBox *> spins = page.findChildren<QDoubleSpinBox *>();
    QVERIFY(spins.size() >= 9);
    for (QDoubleSpinBox *spin : spins)
        QVERIFY2(!spin->accessibleName().isEmpty(), "数值输入缺少无障碍名称");
    for (QComboBox *combo : page.findChildren<QComboBox *>())
        QVERIFY2(!combo->accessibleName().isEmpty(), "组合框缺少无障碍名称");

    int buddyLabels = 0;
    for (QLabel *label : page.findChildren<QLabel *>()) {
        if (label->buddy())
            ++buddyLabels;
    }
    QVERIFY(buddyLabels >= 10);
    QTextEdit *notes = page.findChild<QTextEdit *>();
    QVERIFY(notes && !notes->accessibleName().isEmpty());
}

void VisionSelectUiTests::f9ActionUsesInputPageRunSignal()
{
    MainWindow window;
    InputPage *input = window.findChild<InputPage *>();
    QAction *action = window.findChild<QAction *>(QStringLiteral("RunSelectionAction"));
    QVERIFY(input);
    QVERIFY(action);
    QCOMPARE(action->shortcut(), QKeySequence(Qt::Key_F9));

    QSignalSpy runSpy(input, &InputPage::runSelectionRequested);
    action->trigger();
    QCOMPARE(runSpy.count(), 1);

    QPushButton *runButton = input->findChild<QPushButton *>(QStringLiteral("RunSelectionButton"));
    QVERIFY(runButton);
    QTRY_VERIFY_WITH_TIMEOUT(runButton->isEnabled(), 30000);
}

void VisionSelectUiTests::settingsRoundTripAndCorruptStateFallback()
{
    UiSettings &settings = UiSettings::instance();
    settings.setDensity(UiDensity::Compact);
    settings.setPreferredSidebarExpanded(true);
    QCOMPARE(settings.density(), UiDensity::Compact);
    QVERIFY(settings.preferredSidebarExpanded());

    QSplitter source(Qt::Horizontal);
    source.addWidget(new QWidget);
    source.addWidget(new QWidget);
    source.resize(600, 100);
    source.setSizes({180, 420});
    settings.saveSplitter(QStringLiteral("tests/splitter"), &source);

    QSplitter restored(Qt::Horizontal);
    restored.addWidget(new QWidget);
    restored.addWidget(new QWidget);
    restored.resize(600, 100);
    settings.restoreSplitter(QStringLiteral("tests/splitter"), &restored);
    QCOMPARE(restored.sizes().size(), 2);
    QVERIFY(qAbs(restored.sizes().first() - source.sizes().first()) <= 2);

    QTableWidget sourceTable(1, 3);
    sourceTable.horizontalHeader()->resizeSection(0, 180);
    sourceTable.horizontalHeader()->moveSection(2, 0);
    sourceTable.setColumnHidden(1, true);
    settings.saveHeader(QStringLiteral("tests/header"), sourceTable.horizontalHeader());
    QTableWidget restoredTable(1, 3);
    settings.restoreHeader(QStringLiteral("tests/header"), restoredTable.horizontalHeader());
    QCOMPARE(restoredTable.horizontalHeader()->visualIndex(2), 0);
    QVERIFY(restoredTable.isColumnHidden(1));

    settings.setValue(QStringLiteral("ui/splitters/tests/corrupt"), QByteArray("broken"));
    QSplitter corrupt(Qt::Horizontal);
    corrupt.addWidget(new QWidget);
    corrupt.addWidget(new QWidget);
    corrupt.resize(400, 100);
    settings.restoreSplitter(QStringLiteral("tests/corrupt"), &corrupt);
    QCOMPARE(corrupt.sizes().size(), 2);
    QVERIFY(!QSettings().contains(QStringLiteral("ui/splitters/tests/corrupt")));
}

void VisionSelectUiTests::catalogToolbarInitialStateAndMenuSignals()
{
    CatalogPage page;
    QPushButton *editButton = page.findChild<QPushButton *>(QStringLiteral("CatalogCameraEditButton"));
    QPushButton *deleteButton = page.findChild<QPushButton *>(QStringLiteral("CatalogCameraDeleteButton"));
    QVERIFY(editButton && !editButton->isEnabled());
    QVERIFY(deleteButton && !deleteButton->isEnabled());

    QToolButton *dataMenuButton = page.findChild<QToolButton *>(QStringLiteral("CatalogCameraDataMenu"));
    QVERIFY(dataMenuButton && dataMenuButton->menu());
    QVERIFY(dataMenuButton->menu()->actions().size() >= 3);
    QSignalSpy importSpy(&page, &CatalogPage::cameraImportRequested);
    QSignalSpy exportSpy(&page, &CatalogPage::cameraExportRequested);
    QSignalSpy filteredSpy(&page, &CatalogPage::cameraExportFilteredRequested);
    dataMenuButton->menu()->actions().at(0)->trigger();
    dataMenuButton->menu()->actions().at(1)->trigger();
    dataMenuButton->menu()->actions().at(2)->trigger();
    QCOMPARE(importSpy.count(), 1);
    QCOMPARE(exportSpy.count(), 1);
    QCOMPARE(filteredSpy.count(), 1);
}

void VisionSelectUiTests::navigationSupportsKeyboardFocus()
{
    MainWindow window;
    const QList<QPushButton *> buttons = window.findChildren<QPushButton *>(QStringLiteral("NavButton"));
    QVERIFY(buttons.size() >= 6);
    for (QPushButton *button : buttons) {
        QCOMPARE(button->focusPolicy(), Qt::StrongFocus);
        QVERIFY(!button->accessibleName().isEmpty() || !button->toolTip().isEmpty());
    }
}

void VisionSelectUiTests::redesignedWorkspaceKeepsInputsAndNavigation()
{
    UiSettings::instance().setDensity(UiDensity::Comfortable);
    UiSettings::instance().setPreferredSidebarExpanded(true);
    LanguageManager::instance().setLanguage(QStringLiteral("zh_CN"));
    MainWindow window;
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(1440, 900);
    window.show();
    QTest::qWait(100);
    InputPage *input = window.findChild<InputPage *>();
    QVERIFY(input);
    SelectionRequest request = input->request();
    request.objectWidthMm = 86.0;
    request.projectNotes = QStringLiteral("界面重构回归：保留输入和项目备注");
    input->setRequest(request);
    QPushButton *toggle = input->findChild<QPushButton *>(QStringLiteral("SummaryToggleButton"));
    QScrollArea *summary = input->findChild<QScrollArea *>(QStringLiteral("ConstraintScroll"));
    QVERIFY(toggle && summary);
    QTest::mouseClick(toggle, Qt::LeftButton);
    QVERIFY(summary->isHidden());
    QTest::mouseClick(toggle, Qt::LeftButton);
    QVERIFY(!summary->isHidden());
    QCOMPARE(input->request().objectWidthMm, 86.0);
    QCOMPARE(input->request().projectNotes, request.projectNotes);

    // 设置环境变量时输出真实 QWidget 渲染图，供人工检查排版。
    const QString captureDirectory = qEnvironmentVariable("VISIONSELECT_UI_CAPTURE_DIR");
    if (!captureDirectory.isEmpty())
        QVERIFY(QDir().mkpath(captureDirectory));
    const auto capture = [&window, &captureDirectory](const QString &name) {
        QCoreApplication::processEvents();
        if (!captureDirectory.isEmpty()) {
            QStackedWidget *stack = window.findChild<QStackedWidget *>(QStringLiteral("WorkspacePages"));
            qInfo() << name << window.size() << window.minimumSize()
                    << stack->currentWidget()->minimumSizeHint();
        }
        return captureDirectory.isEmpty()
            || window.grab().save(QDir(captureDirectory).filePath(name + QStringLiteral(".png")));
    };
    QVERIFY(capture(QStringLiteral("01-requirements")));
    QStackedWidget *pages = window.findChild<QStackedWidget *>(QStringLiteral("WorkspacePages"));
    QVERIFY(pages);
    const QList<QPushButton *> navigation = window.findChildren<QPushButton *>(QStringLiteral("NavButton"));
    for (QPushButton *button : navigation) {
        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(pages->currentIndex(), button->property("pageIndex").toInt());
        if (pages->currentIndex() == 4) {
            QPushButton *run = input->findChild<QPushButton *>(QStringLiteral("RunSelectionButton"));
            QTRY_VERIFY_WITH_TIMEOUT(run->isEnabled(), 60000);
        }
        QVERIFY(capture(QStringLiteral("page-%1").arg(pages->currentIndex())));
    }
    for (QPushButton *button : navigation) {
        if (button->property("pageIndex").toInt() == 0)
            QTest::mouseClick(button, Qt::LeftButton);
    }
    QCOMPARE(input->request().objectWidthMm, 86.0);
    window.resize(1080, 700);
    QTest::qWait(50);
    QVERIFY(window.width() <= 1080);
    QVERIFY(window.height() <= 700);
    QPushButton *run = input->findChild<QPushButton *>(QStringLiteral("RunSelectionButton"));
    QVERIFY(run && run->isVisible());
    QVERIFY(window.rect().contains(QRect(run->mapTo(&window, QPoint()), run->size())));
    QVERIFY(capture(QStringLiteral("07-narrow")));
    for (QPushButton *button : navigation) {
        QTest::mouseClick(button, Qt::LeftButton);
        QTest::qWait(20);
        QVERIFY(window.width() <= 1080 && window.height() <= 700);
        QVERIFY(capture(QStringLiteral("narrow-page-%1").arg(pages->currentIndex())));
    }
    for (QPushButton *button : navigation) {
        if (button->property("pageIndex").toInt() == 0)
            QTest::mouseClick(button, Qt::LeftButton);
    }
    UiSettings::instance().setDensity(UiDensity::Compact);
    QVERIFY(capture(QStringLiteral("08-compact")));
    window.resize(1440, 900);
    LanguageManager::instance().setLanguage(QStringLiteral("en_US"));
    QTest::qWait(50);
    InputPage *englishInput = pages->currentWidget()->findChild<InputPage *>();
    if (!englishInput)
        englishInput = qobject_cast<InputPage *>(pages->currentWidget());
    QVERIFY(englishInput);
    QCOMPARE(englishInput->request().objectWidthMm, 86.0);
    QCOMPARE(englishInput->request().projectNotes, request.projectNotes);
    QVERIFY(capture(QStringLiteral("09-english")));
    // 模拟拖动缩窄窗口，先经过导航自动折叠阈值。
    window.resize(1176, 700);
    QTest::qWait(50);
    window.resize(1080, 700);
    QTest::qWait(50);
    QVERIFY(window.width() <= 1080 && window.height() <= 700);
    QVERIFY(capture(QStringLiteral("10-english-narrow")));
    LanguageManager::instance().setLanguage(QStringLiteral("zh_CN"));
    UiSettings::instance().setDensity(UiDensity::Comfortable);
}

void VisionSelectUiTests::parameterFieldSupportsEmptyAndUnitConversion()
{
    ParameterNumberField field("unit-test", QStringLiteral("长度"), QStringLiteral("mm"));
    QVERIFY(!field.value());
    field.setValue(120.25);
    auto *units = field.findChild<QComboBox *>();
    auto *edit = field.findChild<QLineEdit *>("unit-test");
    QVERIFY(units && edit);
    units->setCurrentIndex(1);
    QVERIFY(qAbs(*field.value() - 120.25) < 1e-8);
    QCOMPARE(edit->text(), QStringLiteral("12.025"));
    const auto state = field.state();
    edit->clear();
    QVERIFY(!field.value());
    field.restoreState(state);
    QVERIFY(qAbs(*field.value() - 120.25) < 1e-8);
    field.setValue(0.0);
    QCOMPARE(edit->text(), QStringLiteral("0"));
    QCOMPARE(*field.value(), 0.0);
    for (const auto &unit : {QStringLiteral("μm"), QStringLiteral("μs"), QStringLiteral("mm/s")}) {
        ParameterNumberField alternate("alternate-unit", unit, unit);
        alternate.setValue(50.0);
        auto *unitChoice = alternate.findChild<QComboBox *>();
        QVERIFY2(unitChoice, qPrintable(unit));
        unitChoice->setCurrentIndex(1);
        QVERIFY(qAbs(*alternate.value() - 50.0) < 1e-8);
    }
}

void VisionSelectUiTests::parameterWorkbenchKeepsTasksAndIndependentSnapshots()
{
    LanguageManager::instance().setLanguage("zh_CN");
    PureCalculationPage page;
    page.setAttribute(Qt::WA_DontShowOnScreen);
    page.resize(1200, 850); page.show();
    auto *tasks = page.findChild<QTabBar *>("WorkbenchTasks");
    QVERIFY(tasks);
    tasks->setFocus();
    QTest::keyClick(tasks, Qt::Key_Right);
    QCOMPARE(page.task(), QStringLiteral("sampling"));
    page.setTask("optics");
    auto *status = page.findChild<QLabel *>("WorkbenchResultStatus");
    auto *results = page.findChild<QTableWidget *>("WorkbenchResults");
    QVERIFY(status && results);
    QCOMPARE(status->property("calculationStatus").toInt(), static_cast<int>(CalculationStatus::Unknown));
    page.resetDefaults();
    QCOMPARE(status->property("calculationStatus").toInt(), static_cast<int>(CalculationStatus::Passed));
    QCOMPARE(results->rowCount(), 6);
    auto *width = page.findChild<QLineEdit *>("optics.width");
    auto *distance = page.findChild<QLineEdit *>("optics.distance");
    auto *solve = page.findChild<QComboBox *>("optics.solve");
    QVERIFY(width && distance && solve);
    page.saveSnapshot(0);
    width->setText("140");
    page.saveSnapshot(1);
    auto state = page.workspaceState();
    QCOMPARE(state.snapshots.size(), 2);
    const auto snapshotWidth = [](const QJsonValue &snapshot) {
        return snapshot.toObject().value("state").toObject().value("fields").toObject()
            .value("optics.width").toObject().value("text").toString();
    };
    QCOMPARE(snapshotWidth(state.snapshots.at(0)), QStringLiteral("120"));
    QCOMPARE(snapshotWidth(state.snapshots.at(1)), QStringLiteral("140"));
    page.setTask("motion"); page.resetDefaults();
    page.setTask("optics");
    QCOMPARE(width->text(), QStringLiteral("140"));
    solve->setCurrentIndex(solve->findData("distance"));
    QVERIFY(page.findChild<QLineEdit *>("optics.focal")->isVisible());
    QVERIFY(!distance->isVisible());
    solve->setCurrentIndex(solve->findData("fov"));
    distance->clear();
    QCOMPARE(status->property("calculationStatus").toInt(), static_cast<int>(CalculationStatus::Unknown));
    QCOMPARE(results->rowCount(), 0);
    distance->setText("300");
    QCOMPARE(results->rowCount(), 6);
    state = page.workspaceState();
    PureCalculationPage restored;
    restored.restoreWorkspaceState(state);
    QCOMPARE(restored.workspaceState().toJson(), state.toJson());
    QVERIFY(!ParameterWorkspaceState::fromJson(QJsonObject{{"version", 999}}));
}

void VisionSelectUiTests::parameterWorkbenchImportsAndChecksUnknownSpecifications()
{
    PureCalculationPage page;
    page.setTask("check");
    page.resetDefaults();
    CameraSpec camera;
    camera.manufacturer = QStringLiteral("测试制造商"); camera.model = "CAMERA";
    camera.resolutionX = 2448; camera.resolutionY = 2048; camera.pixelSizeUm = 3.45;
    camera.maxFps = 10.0; camera.bitDepth = 12.0; camera.colorMode = "Mono"; camera.lensMount = "C";
    page.importCamera(camera);
    QCOMPARE(page.findChild<QComboBox *>("check.format")->currentData().toString(), QString());
    QCOMPARE(page.findChild<QLineEdit *>("check.maxFps")->text(), QStringLiteral("10"));
    LensSpec lens;
    lens.model = "LENS"; lens.focalLengthMm = 16; lens.imageCircleMm = 12;
    page.importLens(lens);
    QCOMPARE(page.findChild<QLineEdit *>("check.dof")->text(), QString());
    page.findChild<QLineEdit *>("check.width")->setText("10");
    page.findChild<QLineEdit *>("check.height")->setText("10");
    page.findChild<QLineEdit *>("check.targetPixel")->setText("5");
    page.findChild<QLineEdit *>("check.distance")->setText("110");
    auto *model = page.findChild<QComboBox *>("check.model");
    model->setCurrentIndex(model->findData("thin"));
    auto *table = page.findChild<QTableWidget *>("WorkbenchResults");
    const auto rowFor = [table](const QString &key) {
        for (int r = 0; r < table->rowCount(); ++r)
            if (table->item(r, 0)->text() == Parameters::checkTitle(key)) return r;
        return -1;
    };
    int row = rowFor("samplingX");
    QVERIFY(row >= 0);
    QCOMPARE(table->item(row, 3)->data(Qt::UserRole + 1).toInt(), static_cast<int>(CalculationStatus::Failed));
    row = rowFor("fps");
    QCOMPARE(table->item(row, 3)->data(Qt::UserRole + 1).toInt(), static_cast<int>(CalculationStatus::Failed));
    lens.lensType = LensType::ObjectTelecentric; lens.pmag = 0.2; lens.telecentricityDeg = -1;
    page.importLens(lens);
    page.findChild<QLineEdit *>("check.heightRange")->setText("2");
    row = rowFor("telecentricity");
    QVERIFY(row >= 0);
    QCOMPARE(table->item(row, 2)->text(), QStringLiteral("—"));
    QCOMPARE(table->item(row, 3)->data(Qt::UserRole + 1).toInt(), static_cast<int>(CalculationStatus::Unknown));
    page.findChild<QLineEdit *>("camera.pixel")->setText("4");
    QVERIFY(page.workspaceState().provenance.value("camera").toObject().value("edited").toBool());
    auto *geometry = page.findChild<QComboBox *>("check.geometry");
    geometry->setCurrentIndex(geometry->findData("measured"));
    page.findChild<QLineEdit *>("check.actualWidth")->setText("10");
    page.findChild<QLineEdit *>("check.actualHeight")->setText("10");
    auto *pixelUnit = page.findChild<QComboBox *>("camera.pixel.unit");
    QVERIFY(pixelUnit);
    pixelUnit->setCurrentIndex(1);
    QCOMPARE(page.findChild<QLineEdit *>("check.actualWidth")->text(), QStringLiteral("10"));
    page.findChild<QLineEdit *>("camera.nx")->setText("1024");
    QVERIFY(page.findChild<QLineEdit *>("check.actualWidth")->text().isEmpty());
    QVERIFY(page.findChild<QLineEdit *>("check.actualHeight")->text().isEmpty());
    page.setTask("optics"); page.resetDefaults();
    auto *apply = page.findChild<QPushButton *>("WorkbenchApplyCheck");
    QVERIFY(apply->isEnabled()); apply->click();
    QCOMPARE(page.task(), QStringLiteral("check"));
    QVERIFY(page.findChild<QLineEdit *>("check.imageCircle")->text().isEmpty());
    QVERIFY(page.findChild<QLineEdit *>("check.dof")->text().isEmpty());
}

void VisionSelectUiTests::parameterCatalogPickersSupportSearchAndImport()
{
    QTemporaryDir directory;
    CatalogRepository catalog;
    catalog.setStorageDirectory(directory.path());
    QString error;
    QVERIFY2(catalog.initializeDatabase(&error), qPrintable(error));
    CameraSpec camera;
    camera.model = "WORKBENCH_PICKER_TEST_CAMERA"; camera.manufacturer = "Test";
    camera.resolutionX = 2448; camera.resolutionY = 2048; camera.pixelSizeUm = 3.45;
    camera.maxFps = 60.0; camera.colorMode = "Mono"; camera.lensMount = "C";
    camera.interfaceType = "USB3"; camera.shutterType = "Global";
    QVERIFY2(catalog.addCamera(camera, &error), qPrintable(error));
    QWidget parent;
    parent.setAttribute(Qt::WA_DontShowOnScreen);
    bool selectCamera = true;
    QTimer driver;
    driver.setInterval(30);
    connect(&driver, &QTimer::timeout, &parent, [&]() {
        auto *dialog = parent.findChild<QDialog *>("ParameterCatalogPicker");
        if (!dialog) return;
        auto *search = dialog->findChild<QLineEdit *>("ParameterCatalogSearch");
        auto *table = dialog->findChild<QTableWidget *>("ParameterCatalogResults");
        if (!search || !table) { dialog->reject(); return; }
        if (selectCamera && search->text() != camera.model) { search->setText(camera.model); return; }
        if (table->rowCount() == 0 || !table->item(0, 0)) return;
        if (selectCamera && !table->item(0, 0)->text().contains(camera.model)) return;
        table->setCurrentCell(0, 0);
        const QString capture = qEnvironmentVariable("VISIONSELECT_UI_CAPTURE_DIR");
        if (!capture.isEmpty()) {
            QDir().mkpath(capture);
            dialog->grab().save(QDir(capture).filePath(selectCamera ? "workbench-picker-camera.png" : "workbench-picker-lens.png"));
        }
        dialog->accept();
    });
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(&timeout, &QTimer::timeout, &parent, [&]() {
        if (auto *dialog = parent.findChild<QDialog *>("ParameterCatalogPicker")) dialog->reject();
    });
    driver.start(); timeout.start(10000);
    const auto selectedCamera = ParameterCatalogDialog::camera(&parent, catalog);
    timeout.stop();
    QVERIFY(selectedCamera.has_value());
    QCOMPARE(selectedCamera->model, camera.model);
    Parameters::SystemInput context;
    context.sensor = {2448.0, 2048.0, 3.45}; context.cameraMount = "C";
    context.distanceMm = 300; context.targetFovWidthMm = 120; context.targetFovHeightMm = 80; context.targetObjectPixelUm = 60;
    selectCamera = false; timeout.start(10000);
    const auto selectedLens = ParameterCatalogDialog::lens(&parent, catalog, context, true);
    timeout.stop(); driver.stop();
    QVERIFY(selectedLens.has_value());
    QVERIFY(!selectedLens->isTelecentric());
    QVERIFY(selectedLens->focalLengthMm > 0.0);
}

void VisionSelectUiTests::parameterWorkbenchLayoutLanguageAndPersistence()
{
    QSettings().remove(QStringLiteral("parameterWorkbench/state"));
    LanguageManager::instance().setLanguage("zh_CN");
    UiSettings::instance().setDensity(UiDensity::Comfortable);
    UiSettings::instance().setPreferredSidebarExpanded(true);
    MainWindow window;
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(1440, 900); window.show();
    const auto openWorkbench = [](MainWindow &window) {
        for (auto *button : window.findChildren<QPushButton *>("NavButton"))
            if (button->property("pageIndex").toInt() == 1) button->click();
        return window.findChild<PureCalculationPage *>();
    };
    PureCalculationPage *page = openWorkbench(window);
    QVERIFY(page);
    const QString directory = qEnvironmentVariable("VISIONSELECT_UI_CAPTURE_DIR");
    if (!directory.isEmpty()) QVERIFY(QDir().mkpath(directory));
    const auto capture = [&](const QString &name) {
        QCoreApplication::processEvents();
        return directory.isEmpty() || window.grab().save(QDir(directory).filePath(name + ".png"));
    };
    for (const QString &key : ParameterUi::taskKeys) {
        page->setTask(key); page->resetDefaults();
        QTest::qWait(35);
        QVERIFY(capture("workbench-wide-" + key));
        auto *scroll = page->findChild<QScrollArea *>("WorkbenchScroll");
        QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
    }
    window.resize(1176, 740); QTest::qWait(35);
    window.resize(1024, 740); QTest::qWait(35);
    if (window.width() > 1024) {
        qInfo() << "window" << window.size() << window.minimumSize() << window.minimumSizeHint();
        qInfo() << "workbench" << page->minimumSizeHint();
        for (auto *widget : window.findChildren<QWidget *>()) {
            if (widget->minimumSizeHint().width() > 800)
                qInfo() << widget->metaObject()->className() << widget->objectName() << widget->minimumSizeHint();
        }
    }
    QVERIFY(window.width() <= 1024);
    for (const QString &key : ParameterUi::taskKeys) {
        page->setTask(key); QTest::qWait(35);
        auto *scroll = page->findChild<QScrollArea *>("WorkbenchScroll");
        QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
        QVERIFY(capture("workbench-narrow-" + key));
        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
        QVERIFY(capture("workbench-narrow-result-" + key));
    }
    page->setTask("optics");
    page->findChild<QLineEdit *>("optics.width")->setText("123");
    page->saveSnapshot(0);
    const auto snapshots = page->workspaceState().snapshots;
    LanguageManager::instance().setLanguage("en_US");
    QTest::qWait(35);
    page = window.findChild<PureCalculationPage *>();
    QVERIFY(page);
    QCOMPARE(page->findChild<QLineEdit *>("optics.width")->text(), QStringLiteral("123"));
    QCOMPARE(page->workspaceState().snapshots, snapshots);
    QVERIFY(capture("workbench-english-narrow"));
    UiSettings::instance().setDensity(UiDensity::Compact);
    QCOMPARE(page->findChild<QLineEdit *>("optics.width")->text(), QStringLiteral("123"));
    QVERIFY(capture("workbench-compact"));
    window.close();
    const auto persisted = ParameterWorkspaceState::fromJson(QJsonDocument::fromJson(QSettings().value("parameterWorkbench/state").toByteArray()).object());
    QVERIFY(persisted.has_value());
    QCOMPARE(persisted->snapshots, snapshots);
    MainWindow reopened;
    reopened.setAttribute(Qt::WA_DontShowOnScreen);
    reopened.show();
    auto *loaded = openWorkbench(reopened);
    QVERIFY(loaded);
    QCOMPARE(loaded->findChild<QLineEdit *>("optics.width")->text(), QStringLiteral("123"));
    QCOMPARE(loaded->workspaceState().snapshots, snapshots);
    LanguageManager::instance().setLanguage("zh_CN");
    UiSettings::instance().setDensity(UiDensity::Comfortable);
}

void VisionSelectUiTests::threeDCameraPrioritizesResultsAndKeepsFilters()
{
    LanguageManager::instance().setLanguage("zh_CN");
    UiSettings::instance().setDensity(UiDensity::Comfortable);
    UiSettings::instance().setValue("ui/threeD/advancedExpanded", false);
    MainWindow window;
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(1440, 900);
    window.show();
    const auto navigate = [&](int index) {
        for (auto *button : window.findChildren<QPushButton *>("NavButton"))
            if (button->property("pageIndex").toInt() == index) button->click();
        QCoreApplication::processEvents();
    };
    navigate(3);
    auto *page = window.findChild<ThreeDCameraPage *>();
    QVERIFY(page);
    auto *filterScroll = page->findChild<QScrollArea *>("ThreeDFilterScroll");
    auto *filters = page->findChild<QPushButton *>("ThreeDFiltersToggle");
    auto *details = page->findChild<QPushButton *>("ThreeDDetailsToggle");
    auto *detailsPanel = page->findChild<QFrame *>("ThreeDDetailsPanel");
    auto *table = page->findChild<QTableWidget *>("threeD/table");
    QVERIFY(filterScroll && filters && details && detailsPanel && table);
    QVERIFY(filterScroll->isHidden());
    QVERIFY(detailsPanel->isHidden());
    QVERIFY(table->rowCount() > 5);
    QVERIFY(table->height() > page->height() * 0.65);

    const QString directory = qEnvironmentVariable("VISIONSELECT_UI_CAPTURE_DIR");
    if (!directory.isEmpty()) QVERIFY(QDir().mkpath(directory));
    const auto capture = [&](const QString &name) {
        QCoreApplication::processEvents();
        return directory.isEmpty() || window.grab().save(QDir(directory).filePath(name + ".png"));
    };
    QVERIFY(capture("three-d-results-wide"));
    window.resize(1080, 700);
    QCoreApplication::processEvents();
    QVERIFY(window.width() <= 1080);
    QVERIFY(table->height() > page->height() * 0.60);
    QVERIFY(capture("three-d-results-narrow"));

    filters->click();
    QVERIFY(!filterScroll->isHidden());
    QComboBox *brand = nullptr;
    for (auto *combo : page->findChildren<QComboBox *>())
        if (combo->accessibleName() == QStringLiteral("品牌")) brand = combo;
    QVERIFY(brand && brand->count() > 1);
    brand->setCurrentIndex(1);
    QVERIFY(filters->text().contains(QStringLiteral("未应用")));
    const QString brandValue = brand->currentData().toString();
    QVERIFY(capture("three-d-filters"));
    auto *advanced = page->findChild<QToolButton *>("ThreeDAdvancedFilters");
    QVERIFY(advanced);
    advanced->click();
    QCoreApplication::processEvents();
    filterScroll->verticalScrollBar()->setValue(filterScroll->verticalScrollBar()->maximum());
    auto *integration = page->findChild<QWidget *>("FilterCheckGroup");
    QVERIFY(integration);
    for (auto *check : integration->findChildren<QCheckBox *>())
        QVERIFY(integration->rect().contains(check->geometry()));
    QVERIFY(capture("three-d-advanced-filters"));
    advanced->click();
    page->findChild<QPushButton *>("ThreeDApplyFilters")->click();
    QVERIFY(filterScroll->isHidden());
    QVERIFY(!filters->text().contains(QStringLiteral("未应用")));
    QCOMPARE(brand->currentData().toString(), brandValue);
    navigate(0);
    navigate(3);
    QCOMPARE(brand->currentData().toString(), brandValue);
    filters->click();
    page->findChild<QPushButton *>("ThreeDClearFilters")->click();
    QCOMPARE(brand->currentIndex(), 0);
    filters->click();

    table->setCurrentCell(1, 4);
    table->scrollToItem(table->item(1, 4));
    QCoreApplication::processEvents();
    const QString selected = table->item(1, 4)->text();
    QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                     table->visualItemRect(table->item(1, 4)).center());
    QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                      table->visualItemRect(table->item(1, 4)).center());
    QVERIFY(details->isChecked());
    QVERIFY(!detailsPanel->isHidden());
    auto *text = detailsPanel->findChild<QTextBrowser *>();
    QVERIFY(text && text->toPlainText().contains(selected));
    table->setFocus();
    QTest::keyClick(table, Qt::Key_Down);
    QVERIFY(text->toPlainText().contains(table->item(table->currentRow(), 4)->text()));
    QVERIFY(capture("three-d-details"));
    page->findChild<QPushButton *>("ThreeDDetailsClose")->click();
    QVERIFY(detailsPanel->isHidden());
    QTest::keyClick(table, Qt::Key_Return);
    QVERIFY(details->isChecked());
    details->click();
    const QString selectedBeforeFilter = table->item(table->currentRow(), 4)->text();
    filters->click();
    page->findChild<QPushButton *>("ThreeDApplyFilters")->click();
    QCOMPARE(table->item(table->currentRow(), 4)->text(), selectedBeforeFilter);

    LanguageManager::instance().setLanguage("en_US");
    QTest::qWait(30);
    auto *english = qobject_cast<ThreeDCameraPage *>(window.findChild<QStackedWidget *>("WorkspacePages")->currentWidget());
    QVERIFY(english);
    auto *englishTable = english->findChild<QTableWidget *>("threeD/table");
    QVERIFY(capture("three-d-results-english"));
    QVERIFY(englishTable->height() > english->height() * 0.60);
    auto *tabs = english->findChild<QTabWidget *>("ThreeDTasks");
    tabs->setCurrentIndex(1);
    QCoreApplication::processEvents();
    QVERIFY(english->findChild<QTextEdit *>("CalculationResultText")->isVisible());
    LanguageManager::instance().setLanguage("zh_CN");
}

void VisionSelectUiTests::navigationReusesPagesAndKeepsState()
{
    LanguageManager::instance().setLanguage("zh_CN");
    UiSettings::instance().setDensity(UiDensity::Comfortable);
    MainWindow window;
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(1440, 900);
    window.show();
    QTest::qWait(50);
    auto *pages = window.findChild<QStackedWidget *>("WorkspacePages");
    QVERIFY(pages);
    const auto navigation = window.findChildren<QPushButton *>("NavButton");
    const auto navigate = [&](int index) {
        for (auto *button : navigation)
            if (button->property("pageIndex").toInt() == index) button->click();
        QCoreApplication::processEvents();
    };
    QVector<QWidget *> created(6, nullptr);
    std::unique_ptr<QSignalSpy> candidateChanges;
    std::unique_ptr<QSignalSpy> workbenchResets;
    ParameterWorkspaceState savedState;
    int selectedCamera = -1;
    const QString captures = qEnvironmentVariable("VISIONSELECT_UI_CAPTURE_DIR");
    if (!captures.isEmpty()) QVERIFY(QDir().mkpath(captures));
    for (int round = 0; round < 3; ++round) {
        for (auto *button : navigation) {
            const int index = button->property("pageIndex").toInt();
            QElapsedTimer timer;
            timer.start();
            button->click();
            const qint64 clicked = timer.elapsed();
            QCoreApplication::processEvents();
            qInfo() << "页面切换" << round << index << "点击/含事件处理(ms)" << clicked << timer.elapsed();
            QCOMPARE(pages->currentIndex(), index);
            if (round == 0) created[index] = pages->currentWidget();
            else QCOMPARE(pages->currentWidget(), created[index]);
            if (index == 4) {
                auto *run = window.findChild<QPushButton *>("RunSelectionButton");
                QTRY_VERIFY_WITH_TIMEOUT(run->isEnabled(), 60000);
            }
            if (round == 0 && !captures.isEmpty())
                QVERIFY(window.grab().save(QDir(captures).filePath(QStringLiteral("navigation-%1.png").arg(index))));
        }
        if (round == 0) {
            auto *workbench = window.findChild<PureCalculationPage *>();
            QVERIFY(workbench);
            workbench->setTask("optics");
            workbench->resetDefaults();
            workbench->findChild<QLineEdit *>("optics.width")->setText("123");
            workbench->saveSnapshot(0);
            savedState = workbench->workspaceState();
            auto *table = window.findChild<QTableWidget *>("calculation/cameras");
            QVERIFY(table && table->rowCount() > 2);
            navigate(2);
            window.findChild<QPushButton *>("AssistantChooseCamera")->setChecked(true);
            QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                             table->visualItemRect(table->item(2, 0)).center());
            selectedCamera = table->currentRow();
            QCOMPARE(selectedCamera, 2);
            candidateChanges = std::make_unique<QSignalSpy>(table->model(), &QAbstractItemModel::dataChanged);
            workbenchResets = std::make_unique<QSignalSpy>(
                workbench->findChild<QTableWidget *>("WorkbenchResults")->model(), &QAbstractItemModel::modelReset);
        }
    }
    QCOMPARE(candidateChanges->count(), 0);
    QCOMPARE(workbenchResets->count(), 0);
    QCOMPARE(window.findChild<QTableWidget *>("calculation/cameras")->currentRow(), selectedCamera);
    QCOMPARE(window.findChild<PureCalculationPage *>()->workspaceState().toJson(), savedState.toJson());

    auto *input = window.findChild<InputPage *>();
    SelectionRequest changed = input->request();
    changed.objectWidthMm += 10;
    input->setRequest(changed);
    navigate(2);
    QVERIFY2(candidateChanges->count() > 0, "修改需求后应重新计算候选");
    candidateChanges->clear();
    window.findChild<CalculationPage *>()->recalculateRequested();
    QVERIFY2(candidateChanges->count() > 0, "主动重算应刷新候选");
    candidateChanges->clear();
    navigate(2);
    QCOMPARE(candidateChanges->count(), 0);

    // 参数工作台内部任务切换也保留状态，并输出可比的交互耗时。
    navigate(1);
    auto *workbench = window.findChild<PureCalculationPage *>();
    for (const auto &key : ParameterUi::taskKeys) {
        QElapsedTimer timer;
        timer.start();
        workbench->setTask(key);
        QCoreApplication::processEvents();
        qInfo() << "计算任务切换" << key << timer.elapsed();
    }
    window.resize(1080, 700);
    navigate(3);
    for (auto *step : window.findChildren<QLabel *>("WorkflowStep"))
        QVERIFY(!step->isVisible());
    navigate(2);
    for (auto *step : window.findChildren<QLabel *>("WorkflowStep"))
        QCOMPARE(step->isVisible(), step->property("state").toString() == QStringLiteral("active"));
}

void VisionSelectUiTests::calculationSelectionUsesKeyboardAndLensIdentity()
{
    LanguageManager::instance().setLanguage("zh_CN");
    CalculationPage page;
    page.setAttribute(Qt::WA_DontShowOnScreen);
    page.resize(1200, 710);
    page.show();
    QVector<CameraCalculationEstimate> cameras(2);
    cameras[0].camera.model = "相机 A";
    cameras[1].camera.model = "相机 B";
    page.setCameraEstimates(cameras);
    auto *cameraTable = page.findChild<QTableWidget *>("calculation/cameras");
    QVERIFY(cameraTable->isHidden() || !cameraTable->isVisible());
    page.findChild<QPushButton *>("AssistantChooseCamera")->setChecked(true);
    cameraTable->sortItems(0, Qt::AscendingOrder);
    cameraTable->setCurrentCell(0, 0);
    QSignalSpy selection(&page, &CalculationPage::cameraSelectionChanged);
    QTest::keyClick(cameraTable, Qt::Key_Down);
    QCOMPARE(selection.count(), 1);
    QCOMPARE(selection.first().first().toInt(), 1);
    QCOMPARE(page.selectedCameraEstimateRow(), 1);
    QVERIFY(page.findChild<QLabel *>("AssistantCameraSummary")->text().contains("相机 B"));
    std::reverse(cameras.begin(), cameras.end());
    page.setCameraEstimates(cameras);
    QCOMPARE(page.selectedCameraEstimateRow(), 0);
    QVERIFY(page.findChild<QLabel *>("AssistantCameraSummary")->text().contains("相机 B"));

    QVector<LensCalculationEstimate> lenses(2);
    lenses[0].lens.model = "镜头 A";
    lenses[0].checks[CandidateCheck::ImageCircle] = CandidateCheckState::Failed;
    lenses[1].lens.model = "镜头 B";
    lenses[1].checks[CandidateCheck::WorkingDistance] = CandidateCheckState::Unknown;
    page.setLensEstimates(lenses);
    auto *lensTable = page.findChild<QTableWidget *>("calculation/lenses");
    lensTable->sortItems(2, Qt::AscendingOrder);
    QVERIFY(lensTable->item(0, 9)->text().contains("像圈"));
    lensTable->setCurrentCell(1, 2);
    page.findChild<QPushButton *>("AssistantDetailsToggle")->setChecked(true);
    auto *details = page.findChild<QTextEdit *>("AssistantLensDetails");
    QVERIFY(details->toPlainText().contains("镜头 B"));
    QVERIFY(details->toPlainText().contains("待确认"));
    QVERIFY(!details->toPlainText().contains("镜头 A"));
    page.findChild<QPushButton *>("AssistantChooseCamera")->setChecked(false);
    page.findChild<QPushButton *>("AssistantDetailsToggle")->setChecked(false);
    QCoreApplication::processEvents();
    QVERIFY(lensTable->height() > page.height() * 0.60);
}

void VisionSelectUiTests::resultsSortNumericallyAndShowChecks()
{
    ResultsPage page;
    page.setAttribute(Qt::WA_DontShowOnScreen);
    page.resize(1200, 710);
    page.show();
    QVector<SelectionResult> results(4);
    const double scores[] = {100, 97, 9, 0};
    for (int i = 0; i < results.size(); ++i) {
        results[i].score.score = scores[i];
        results[i].camera.model = QString("相机%1").arg(i);
        results[i].lens.model = QString("镜头%1").arg(i);
    }
    results[0].checks[CandidateCheck::WorkingDistance] = CandidateCheckState::Unknown;
    results[0].interfaceCapacityMBps = 2400;
    results[0].bandwidthRequiredMBps = 675;
    results[3].hardConstraintsPassed = false;
    results[3].checks[CandidateCheck::DepthOfField] = CandidateCheckState::Failed;
    page.setResults(results, SelectionRequest());
    auto *table = page.findChild<QTableWidget *>("results/table");
    table->sortItems(2, Qt::DescendingOrder);
    QCOMPARE(table->item(0, 2)->text(), "100%");
    QCOMPARE(table->item(1, 2)->text(), "97%");
    QCOMPARE(table->item(2, 2)->text(), "9%");
    table->sortItems(2, Qt::AscendingOrder);
    QCOMPARE(table->item(0, 2)->text(), "9%");
    QCOMPARE(table->item(2, 2)->text(), "100%");
    table->sortItems(2, Qt::DescendingOrder);
    table->setCurrentCell(0, 0);
    QCOMPARE(table->item(0, 1)->text(), "待确认");
    page.findChild<QPushButton *>("ResultsDetailsToggle")->setChecked(true);
    const QString text = page.findChild<QTextEdit *>("ResultsDetails")->toPlainText();
    QVERIFY(text.contains("接口容量 2400.0"));
    QVERIFY(text.contains("1725.0 MB/s"));
    QVERIFY(text.contains("WD 工作距离"));
    page.findChild<QPushButton *>("ResultsDetailsToggle")->setChecked(false);
    QCoreApplication::processEvents();
    QVERIFY(table->height() > page.height() * 0.60);
}

void VisionSelectUiTests::resultSnapshotSurvivesInputNavigationAndLanguage()
{
    LanguageManager::instance().setLanguage("zh_CN");
    MainWindow window;
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(1440, 900);
    window.show();
    auto *input = window.findChild<InputPage *>();
    input->setRequest(SelectionRequest());
    input->runSelectionRequested();
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QPushButton *>("RunSelectionButton")->isEnabled(), 60000);
    QVERIFY(!window.selectionSnapshot().results.isEmpty());
    const auto snapshot = window.selectionSnapshot();
    const auto navigate = [&](int index) {
        for (auto *button : window.findChildren<QPushButton *>("NavButton"))
            if (button->property("pageIndex").toInt() == index) button->click();
        QCoreApplication::processEvents();
    };
    const QString captureDir = qEnvironmentVariable("VISIONSELECT_UI_CAPTURE_DIR");
    const auto capture = [&](const QString &name) {
        if (captureDir.isEmpty()) return true;
        QDir().mkpath(captureDir);
        QCoreApplication::processEvents();
        return window.grab().save(QDir(captureDir).filePath(name + ".png"));
    };
    QVERIFY(capture("results-wide"));
    window.resize(1280, 790);
    QVERIFY(capture("results-compact"));
    auto *resultTable = window.findChild<QTableWidget *>("results/table");
    auto *resultPage = qobject_cast<ResultsPage *>(window.findChild<QStackedWidget *>("WorkspacePages")->currentWidget());
    QVERIFY(resultTable->height() > resultPage->height() * 0.60);
    window.findChild<QPushButton *>("ResultsDetailsToggle")->setChecked(true);
    QVERIFY(capture("results-details"));
    window.findChild<QPushButton *>("ResultsDetailsToggle")->setChecked(false);
    navigate(2);
    QVERIFY(capture("assistant-compact"));
    window.findChild<QPushButton *>("AssistantChooseCamera")->setChecked(true);
    QVERIFY(capture("assistant-camera-selection"));
    window.findChild<QPushButton *>("AssistantChooseCamera")->setChecked(false);
    window.findChild<QPushButton *>("AssistantDetailsToggle")->setChecked(true);
    QVERIFY(capture("assistant-details"));
    window.findChild<QPushButton *>("AssistantDetailsToggle")->setChecked(false);
    navigate(0);
    auto changed = input->request();
    changed.objectWidthMm = 60;
    input->setRequest(changed);
    navigate(2);
    navigate(4);
    QCOMPARE(window.selectionSnapshot().request.objectWidthMm, snapshot.request.objectWidthMm);
    QCOMPARE(window.selectionSnapshot().results.first().requiredFovWidthMm, 24.0);
    QVERIFY(window.findChild<QFrame *>("ResultsOutdatedBanner")->isVisible());
    QVERIFY(capture("results-outdated"));
    LanguageManager::instance().setLanguage("en_US");
    QTest::qWait(30);
    QCOMPARE(window.selectionSnapshot().request.objectWidthMm, 20.0);
    auto *english = window.findChild<QStackedWidget *>("WorkspacePages")->currentWidget();
    QVERIFY(english->findChild<QFrame *>("ResultsOutdatedBanner")->isVisible());
    QVERIFY(capture("results-english"));
    navigate(2);
    QVERIFY(capture("assistant-english"));
    LanguageManager::instance().setLanguage("zh_CN");
}

QTEST_MAIN(VisionSelectUiTests)
#include "test_ui.moc"
