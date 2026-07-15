#include "ui/MainWindow.h"
#include "ui/ResultPresentation.h"
#include "ui/UiSettings.h"
#include "ui/pages/CatalogPage.h"
#include "ui/pages/InputPage.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSplitter>
#include <QTableWidget>
#include <QTextEdit>
#include <QToolButton>
#include <QtTest>

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
};

void VisionSelectUiTests::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("VisionSelectUiTests"));
    QCoreApplication::setApplicationName(QStringLiteral("VisionSelectUiTests"));
    QSettings().clear();
    UiSettings::instance().initialize();
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

QTEST_MAIN(VisionSelectUiTests)
#include "test_ui.moc"
