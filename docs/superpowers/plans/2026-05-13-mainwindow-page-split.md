# MainWindow Page Split Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split `src/ui/MainWindow` into page-level UI classes while preserving the current UI and behavior.

**Architecture:** `MainWindow` remains the application shell and workflow coordinator. Page classes own their widgets and expose small methods for reading input or refreshing display state; shared helpers and catalog edit dialogs move into focused UI support files.

**Tech Stack:** C++14, Qt 5.12.9 Widgets, qmake, MSVC2015 x64, QtTest.

---

## Execution Prerequisite: Dirty Worktree Handling

The current workspace contains pre-existing uncommitted changes in:

- `src/selection/CalculationAssistant.cpp`
- `src/selection/CalculationAssistant.h`
- `src/ui/MainWindow.cpp`
- `src/ui/MainWindow.h`
- `tests/test_selection.cpp`
- `AGENTS.md`

Do not revert those changes. Before implementation starts, choose one explicit policy:

- **Policy A:** Commit the current feature baseline first, then run this refactor in follow-up commits.
- **Policy B:** Continue in the dirty worktree and skip the commit steps in this plan.
- **Policy C:** Create a separate branch after the user explicitly approves carrying the dirty work into that branch.

If no policy is approved, stop before Task 1.

## File Structure

Create:

- `src/ui/UiHelpers.h`: shared widget/table formatting declarations.
- `src/ui/UiHelpers.cpp`: shared widget/table formatting implementations moved from `MainWindow.cpp`.
- `src/ui/CatalogDialogs.h`: camera, lens, and light edit dialog declarations.
- `src/ui/CatalogDialogs.cpp`: camera, lens, and light edit dialog implementations moved from `MainWindow.cpp`.
- `src/ui/pages/InputPage.h`: demand input page class.
- `src/ui/pages/InputPage.cpp`: demand input controls and `SelectionRequest` reader.
- `src/ui/pages/PureCalculationPage.h`: manual pure calculation page class.
- `src/ui/pages/PureCalculationPage.cpp`: pure calculation controls, defaults, input reader, and result refresh.
- `src/ui/pages/CalculationPage.h`: product calculation assistant page class.
- `src/ui/pages/CalculationPage.cpp`: assistant summary, camera table, lens table, and details text.
- `src/ui/pages/ResultsPage.h`: recommendation results page class.
- `src/ui/pages/ResultsPage.cpp`: results table and details text.
- `src/ui/pages/ComparisonPage.h`: top-result comparison page class.
- `src/ui/pages/ComparisonPage.cpp`: comparison table and details text.
- `src/ui/pages/CatalogPage.h`: catalog tabs, filters, tables, and visible index maps.
- `src/ui/pages/CatalogPage.cpp`: catalog table refresh and catalog action signals.
- `src/ui/pages/ReportPage.h`: report preview page class.
- `src/ui/pages/ReportPage.cpp`: report preview and export action signals.

Modify:

- `src/ui/MainWindow.h`: replace page widget members with page object pointers and coordinator declarations.
- `src/ui/MainWindow.cpp`: construct pages, connect signals, keep workflow and repository actions.
- `app/app.pro`: add new UI source and header files.

Do not modify:

- generated `Makefile` files
- selection formulas
- catalog persistence behavior
- CSV, BOM, or PDF formats
- user-facing labels or page order

## Shared Interface Targets

Use these class interfaces unless implementation proves a compile error requires a minor Qt forward-declaration adjustment.

```cpp
// src/ui/pages/InputPage.h
#ifndef INPUTPAGE_H
#define INPUTPAGE_H

#include "core/SelectionTypes.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;

class InputPage : public QWidget
{
    Q_OBJECT

public:
    explicit InputPage(QWidget *parent = nullptr);
    SelectionRequest request() const;

signals:
    void calculateRequested();
    void resultsRequested();

private:
    QDoubleSpinBox *m_widthSpin = nullptr;
    QDoubleSpinBox *m_heightSpin = nullptr;
    QDoubleSpinBox *m_marginSpin = nullptr;
    QDoubleSpinBox *m_minFeatureSpin = nullptr;
    QDoubleSpinBox *m_toleranceSpin = nullptr;
    QDoubleSpinBox *m_wdSpin = nullptr;
    QDoubleSpinBox *m_heightVariationSpin = nullptr;
    QDoubleSpinBox *m_speedSpin = nullptr;
    QDoubleSpinBox *m_fpsSpin = nullptr;
    QComboBox *m_detectionCombo = nullptr;
    QComboBox *m_surfaceCombo = nullptr;
    QCheckBox *m_reflectiveCheck = nullptr;
    QCheckBox *m_monoCheck = nullptr;
    QCheckBox *m_allowTelecentricCheck = nullptr;
};

#endif
```

```cpp
// src/ui/pages/PureCalculationPage.h
#ifndef PURECALCULATIONPAGE_H
#define PURECALCULATIONPAGE_H

#include "selection/CalculationAssistant.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QTextEdit;

class PureCalculationPage : public QWidget
{
    Q_OBJECT

public:
    explicit PureCalculationPage(QWidget *parent = nullptr);
    PureCalculationInput input() const;

public slots:
    void refresh();
    void resetDefaults();

private:
    QDoubleSpinBox *m_widthSpin = nullptr;
    QDoubleSpinBox *m_heightSpin = nullptr;
    QDoubleSpinBox *m_marginSpin = nullptr;
    QDoubleSpinBox *m_minFeatureSpin = nullptr;
    QDoubleSpinBox *m_toleranceSpin = nullptr;
    QDoubleSpinBox *m_wdSpin = nullptr;
    QDoubleSpinBox *m_heightVariationSpin = nullptr;
    QDoubleSpinBox *m_speedSpin = nullptr;
    QDoubleSpinBox *m_fpsSpin = nullptr;
    QComboBox *m_detectionCombo = nullptr;
    QComboBox *m_surfaceCombo = nullptr;
    QCheckBox *m_reflectiveCheck = nullptr;
    QSpinBox *m_resolutionXSpin = nullptr;
    QSpinBox *m_resolutionYSpin = nullptr;
    QDoubleSpinBox *m_pixelSizeSpin = nullptr;
    QDoubleSpinBox *m_bitDepthSpin = nullptr;
    QDoubleSpinBox *m_interfaceBandwidthSpin = nullptr;
    QComboBox *m_shutterCombo = nullptr;
    QComboBox *m_lensModeCombo = nullptr;
    QDoubleSpinBox *m_focalSpin = nullptr;
    QDoubleSpinBox *m_fNumberSpin = nullptr;
    QDoubleSpinBox *m_minWdSpin = nullptr;
    QDoubleSpinBox *m_distortionSpin = nullptr;
    QDoubleSpinBox *m_imageCircleSpin = nullptr;
    QDoubleSpinBox *m_lensMpSpin = nullptr;
    QDoubleSpinBox *m_pmagSpin = nullptr;
    QDoubleSpinBox *m_nominalWdSpin = nullptr;
    QDoubleSpinBox *m_wdToleranceSpin = nullptr;
    QDoubleSpinBox *m_dofSpin = nullptr;
    QDoubleSpinBox *m_telecentricitySpin = nullptr;
    QComboBox *m_lightTypeCombo = nullptr;
    QComboBox *m_lightModeCombo = nullptr;
    QDoubleSpinBox *m_lightWidthSpin = nullptr;
    QDoubleSpinBox *m_lightHeightSpin = nullptr;
    QTextEdit *m_output = nullptr;
};

#endif
```

```cpp
// src/ui/pages/CalculationPage.h
#ifndef CALCULATIONPAGE_H
#define CALCULATIONPAGE_H

#include "selection/CalculationAssistant.h"

#include <QWidget>
#include <QVector>

class QLabel;
class QTableWidget;
class QTextEdit;

class CalculationPage : public QWidget
{
    Q_OBJECT

public:
    explicit CalculationPage(QWidget *parent = nullptr);
    void setSummary(const RequirementEstimate &requirement,
                    int cameraCount,
                    int lensCount);
    void setCameraEstimates(const QVector<CameraCalculationEstimate> &estimates,
                            const RequirementEstimate &requirement);
    void setLensEstimates(const QVector<LensCalculationEstimate> &estimates);
    int selectedCameraEstimateRow() const;

signals:
    void inputRequested();
    void recalculateRequested();
    void cameraSelectionChanged(int row);

private:
    QLabel *m_summaryLabel = nullptr;
    QTableWidget *m_cameraTable = nullptr;
    QTableWidget *m_lensTable = nullptr;
    QTextEdit *m_details = nullptr;
};

#endif
```

```cpp
// src/ui/pages/ResultsPage.h
#ifndef RESULTSPAGE_H
#define RESULTSPAGE_H

#include "core/SelectionTypes.h"

#include <QWidget>
#include <QVector>

class QLabel;
class QTableWidget;
class QTextEdit;

class ResultsPage : public QWidget
{
    Q_OBJECT

public:
    explicit ResultsPage(QWidget *parent = nullptr);
    void setResults(const QVector<SelectionResult> &results,
                    const SelectionRequest &request);

signals:
    void comparisonRequested();

private:
    QVector<SelectionResult> m_results;
    QLabel *m_summaryLabel = nullptr;
    QTableWidget *m_table = nullptr;
    QTextEdit *m_details = nullptr;
    void refreshTable(const SelectionRequest &request);
    void refreshDetails(int row);
};

#endif
```

```cpp
// src/ui/pages/ComparisonPage.h
#ifndef COMPARISONPAGE_H
#define COMPARISONPAGE_H

#include "core/SelectionTypes.h"

#include <QWidget>
#include <QVector>

class QTableWidget;
class QTextEdit;

class ComparisonPage : public QWidget
{
    Q_OBJECT

public:
    explicit ComparisonPage(QWidget *parent = nullptr);
    void setResults(const QVector<SelectionResult> &results);

signals:
    void recalculateRequested();
    void exportBomRequested();

private:
    QVector<SelectionResult> m_results;
    QTableWidget *m_table = nullptr;
    QTextEdit *m_details = nullptr;
    void refreshDetails(int row);
};

#endif
```

```cpp
// src/ui/pages/CatalogPage.h
#ifndef CATALOGPAGE_H
#define CATALOGPAGE_H

#include "catalog/CatalogRepository.h"

#include <QWidget>
#include <QVector>

class QComboBox;
class QLineEdit;
class QTableWidget;

class CatalogPage : public QWidget
{
    Q_OBJECT

public:
    explicit CatalogPage(QWidget *parent = nullptr);
    void setCatalog(const CatalogRepository &catalog);
    int selectedCameraIndex() const;
    int selectedLensIndex() const;
    int selectedLightIndex() const;
    QVector<int> visibleCameraIndexes() const;
    QVector<int> visibleLensIndexes() const;
    QVector<int> visibleLightIndexes() const;

signals:
    void addCameraRequested();
    void editCameraRequested();
    void removeCameraRequested();
    void addLensRequested();
    void editLensRequested();
    void removeLensRequested();
    void addLightRequested();
    void editLightRequested();
    void removeLightRequested();
    void importCamerasRequested();
    void importLensesRequested();
    void importLightsRequested();
    void exportCamerasRequested();
    void exportLensesRequested();
    void exportLightsRequested();
    void exportFilteredCamerasRequested();
    void exportFilteredLensesRequested();
    void exportFilteredLightsRequested();
    void resetCamerasRequested();
    void resetLensesRequested();
    void resetLightsRequested();

private:
    const CatalogRepository *m_catalog = nullptr;
    QLineEdit *m_cameraSearchEdit = nullptr;
    QComboBox *m_cameraManufacturerFilter = nullptr;
    QComboBox *m_cameraInterfaceFilter = nullptr;
    QLineEdit *m_lensSearchEdit = nullptr;
    QComboBox *m_lensManufacturerFilter = nullptr;
    QComboBox *m_lensTypeFilter = nullptr;
    QComboBox *m_lensMountFilter = nullptr;
    QLineEdit *m_lightSearchEdit = nullptr;
    QComboBox *m_lightManufacturerFilter = nullptr;
    QComboBox *m_lightTypeFilter = nullptr;
    QComboBox *m_lightModeFilter = nullptr;
    QTableWidget *m_cameraTable = nullptr;
    QTableWidget *m_lensTable = nullptr;
    QTableWidget *m_lightTable = nullptr;
    QVector<int> m_cameraRowMap;
    QVector<int> m_lensRowMap;
    QVector<int> m_lightRowMap;

    void refreshFilterOptions();
    void refreshCameraTable();
    void refreshLensTable();
    void refreshLightTable();
};

#endif
```

```cpp
// src/ui/pages/ReportPage.h
#ifndef REPORTPAGE_H
#define REPORTPAGE_H

#include "core/SelectionTypes.h"

#include <QWidget>
#include <QVector>

class QTextEdit;

class ReportPage : public QWidget
{
    Q_OBJECT

public:
    explicit ReportPage(QWidget *parent = nullptr);
    void setReportData(const SelectionRequest &request,
                       const QVector<SelectionResult> &results);

signals:
    void exportPdfRequested();
    void exportBomRequested();
    void recalculateRequested();

private:
    QTextEdit *m_preview = nullptr;
};

#endif
```

## Task 1: Extract Shared UI Helpers

**Files:**
- Create: `src/ui/UiHelpers.h`
- Create: `src/ui/UiHelpers.cpp`
- Modify: `src/ui/MainWindow.cpp`
- Modify: `app/app.pro`

- [ ] **Step 1: Create `UiHelpers.h` with shared declarations**

Use this content:

```cpp
#ifndef UIHELPERS_H
#define UIHELPERS_H

#include "core/SelectionTypes.h"

#include <QString>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;
class QTableWidget;
class QTableWidgetItem;

namespace UiHelpers {

QLabel *pageTitle(const QString &text, const QString &subtitle = QString());
QTableWidgetItem *item(const QString &text);
QTableWidgetItem *indexedItem(const QString &text, int sourceIndex);
int rowSourceIndex(const QTableWidget *table, int row);
void copySelectionToClipboard(QTableWidget *table);
void installTableCopyShortcut(QTableWidget *table);
void setupTable(QTableWidget *table);
QString number(double value, int decimals = 2);
QString productLabel(const QString &manufacturer, const QString &model);
QString riskSummary(const SelectionResult &result);
QString exposureText(double exposureUs);
QDoubleSpinBox *makeSpin(double min, double max, double value, const QString &suffix, int decimals = 2);
QDoubleSpinBox *dialogSpin(double min, double max, double value, const QString &suffix = QString(), int decimals = 3);
QSpinBox *dialogIntSpin(int min, int max, int value, const QString &suffix = QString());
void setComboText(QComboBox *combo, const QString &text);
QComboBox *editableCombo(const QStringList &items, const QString &value);

}

#endif
```

- [ ] **Step 2: Create `UiHelpers.cpp` by moving existing helper bodies**

Move these anonymous-namespace helper implementations from `src/ui/MainWindow.cpp` into `UiHelpers.cpp`:

- `pageTitle`
- `item`
- `indexedItem`
- `rowSourceIndex`
- `copySelectionToClipboard`
- `installTableCopyShortcut`
- `number`
- `productLabel`
- `riskSummary`
- `exposureText`
- `setupTable`
- `dialogSpin`
- `dialogIntSpin`
- `setComboText`
- `editableCombo`

Rename the existing `MainWindow::makeSpin` body into `UiHelpers::makeSpin`. Preserve range, decimals, suffix, value, and `setKeyboardTracking(false)`.

- [ ] **Step 3: Replace helper calls in `MainWindow.cpp`**

Add:

```cpp
#include "ui/UiHelpers.h"

using namespace UiHelpers;
```

Remove the moved anonymous-namespace helper definitions and remove `MainWindow::makeSpin`.

- [ ] **Step 4: Remove `makeSpin` from `MainWindow.h`**

Delete:

```cpp
QDoubleSpinBox *makeSpin(double min, double max, double value, const QString &suffix, int decimals = 2);
```

- [ ] **Step 5: Add helper files to `app/app.pro`**

Add to `SOURCES`:

```qmake
    ../src/ui/UiHelpers.cpp \
```

Add to `HEADERS`:

```qmake
    ../src/ui/UiHelpers.h \
```

- [ ] **Step 6: Build**

Run:

```cmd
build_msvc2015.bat
```

Expected: command exits 0 with no compiler errors.

- [ ] **Step 7: Commit checkpoint if dirty baseline policy allows commits**

Run:

```bash
git add src/ui/UiHelpers.h src/ui/UiHelpers.cpp src/ui/MainWindow.h src/ui/MainWindow.cpp app/app.pro
git commit -m "refactor: extract shared UI helpers"
```

Expected: commit succeeds. If dirty baseline policy is Policy B, skip this step.

## Task 2: Extract Catalog Edit Dialogs

**Files:**
- Create: `src/ui/CatalogDialogs.h`
- Create: `src/ui/CatalogDialogs.cpp`
- Modify: `src/ui/MainWindow.cpp`
- Modify: `app/app.pro`

- [ ] **Step 1: Create `CatalogDialogs.h`**

Use this content:

```cpp
#ifndef CATALOGDIALOGS_H
#define CATALOGDIALOGS_H

#include "core/SelectionTypes.h"

class QWidget;

bool editCameraDialog(QWidget *parent, CameraSpec *camera, const QString &title);
bool editLensDialog(QWidget *parent, LensSpec *lens, const QString &title);
bool editLightDialog(QWidget *parent, LightSpec *light, const QString &title);

#endif
```

- [ ] **Step 2: Create `CatalogDialogs.cpp`**

Move these existing function bodies from `MainWindow.cpp`:

- `editCameraDialog`
- `editLensDialog`
- `editLightDialog`

Add:

```cpp
#include "ui/CatalogDialogs.h"
#include "ui/UiHelpers.h"
```

Use `using namespace UiHelpers;` inside the file. Preserve every dialog field, label, range, default, accepted value assignment, and return condition.

- [ ] **Step 3: Update `MainWindow.cpp`**

Add:

```cpp
#include "ui/CatalogDialogs.h"
```

Remove the three moved dialog functions from `MainWindow.cpp`.

- [ ] **Step 4: Update `app/app.pro`**

Add to `SOURCES`:

```qmake
    ../src/ui/CatalogDialogs.cpp \
```

Add to `HEADERS`:

```qmake
    ../src/ui/CatalogDialogs.h \
```

- [ ] **Step 5: Build and run tests**

Run:

```cmd
build_msvc2015.bat
set PATH=C:\Qt\Qt5.12.9\5.12.9\msvc2015_64\bin;%PATH%&& bin\VisionSelectTests.exe
```

Expected: both commands exit 0.

- [ ] **Step 6: Commit checkpoint if dirty baseline policy allows commits**

Run:

```bash
git add src/ui/CatalogDialogs.h src/ui/CatalogDialogs.cpp src/ui/MainWindow.cpp app/app.pro
git commit -m "refactor: extract catalog edit dialogs"
```

Expected: commit succeeds. If dirty baseline policy is Policy B, skip this step.

## Task 3: Extract InputPage

**Files:**
- Create: `src/ui/pages/InputPage.h`
- Create: `src/ui/pages/InputPage.cpp`
- Modify: `src/ui/MainWindow.h`
- Modify: `src/ui/MainWindow.cpp`
- Modify: `app/app.pro`

- [ ] **Step 1: Create `InputPage.h`**

Use the `InputPage.h` interface from Shared Interface Targets.

- [ ] **Step 2: Create `InputPage.cpp`**

Move the body of `MainWindow::createInputPage()` into `InputPage::InputPage(QWidget *parent)`. Replace member names by dropping the `m_` prefixes shown in `InputPage.h` only where they are different. Keep the same labels, ranges, defaults, group boxes, layout margins, and button connections.

Connect the calculate button:

```cpp
connect(calculateButton, &QPushButton::clicked, this, &InputPage::calculateRequested);
```

Connect the result button:

```cpp
connect(resultButton, &QPushButton::clicked, this, &InputPage::resultsRequested);
```

- [ ] **Step 3: Move `readRequest()` into `InputPage::request()`**

Move the existing `MainWindow::readRequest()` body into `InputPage::request() const`. Preserve all field assignments:

- object width and height
- placement margin
- min feature
- measurement tolerance
- detection type
- surface type
- working distance
- height variation
- motion speed
- required fps
- reflective
- prefer mono
- allow telecentric

- [ ] **Step 4: Update `MainWindow.h`**

Forward declare:

```cpp
class InputPage;
```

Add:

```cpp
InputPage *m_inputPage = nullptr;
```

Remove these `MainWindow` members:

- `m_widthSpin`
- `m_heightSpin`
- `m_marginSpin`
- `m_minFeatureSpin`
- `m_toleranceSpin`
- `m_wdSpin`
- `m_heightVariationSpin`
- `m_speedSpin`
- `m_fpsSpin`
- `m_detectionCombo`
- `m_surfaceCombo`
- `m_reflectiveCheck`
- `m_monoCheck`
- `m_allowTelecentricCheck`

Remove:

```cpp
QWidget *createInputPage();
SelectionRequest readRequest() const;
```

- [ ] **Step 5: Update `MainWindow.cpp` construction and request reading**

Replace:

```cpp
m_pages->addWidget(createInputPage());
```

with:

```cpp
m_inputPage = new InputPage;
connect(m_inputPage, &InputPage::calculateRequested, this, &MainWindow::calculate);
connect(m_inputPage, &InputPage::resultsRequested, this, [this]() { setActivePage(3); });
m_pages->addWidget(m_inputPage);
```

Replace:

```cpp
m_request = readRequest();
```

with:

```cpp
m_request = m_inputPage->request();
```

Remove `MainWindow::createInputPage()` and `MainWindow::readRequest()`.

- [ ] **Step 6: Update `app/app.pro`**

Add to `SOURCES`:

```qmake
    ../src/ui/pages/InputPage.cpp \
```

Add to `HEADERS`:

```qmake
    ../src/ui/pages/InputPage.h \
```

- [ ] **Step 7: Build and run tests**

Run:

```cmd
build_msvc2015.bat
set PATH=C:\Qt\Qt5.12.9\5.12.9\msvc2015_64\bin;%PATH%&& bin\VisionSelectTests.exe
```

Expected: both commands exit 0.

- [ ] **Step 8: Commit checkpoint if dirty baseline policy allows commits**

Run:

```bash
git add src/ui/pages/InputPage.h src/ui/pages/InputPage.cpp src/ui/MainWindow.h src/ui/MainWindow.cpp app/app.pro
git commit -m "refactor: extract input page"
```

Expected: commit succeeds. If dirty baseline policy is Policy B, skip this step.

## Task 4: Extract PureCalculationPage

**Files:**
- Create: `src/ui/pages/PureCalculationPage.h`
- Create: `src/ui/pages/PureCalculationPage.cpp`
- Modify: `src/ui/MainWindow.h`
- Modify: `src/ui/MainWindow.cpp`
- Modify: `app/app.pro`

- [ ] **Step 1: Create `PureCalculationPage.h`**

Use the `PureCalculationPage.h` interface from Shared Interface Targets.

- [ ] **Step 2: Create `PureCalculationPage.cpp`**

Move these methods from `MainWindow.cpp` into the page:

- `createPureCalculationPage()` body into `PureCalculationPage::PureCalculationPage(QWidget *parent)`
- `readPureCalculationInput()` body into `PureCalculationPage::input() const`
- `refreshPureCalculation()` body into `PureCalculationPage::refresh()`
- reset button lambda body into `PureCalculationPage::resetDefaults()`

Replace `m_pure` member prefixes with the shorter member names from `PureCalculationPage.h`. Preserve all labels, default values, ranges, combo contents, HTML headings, and risk/reason text.

- [ ] **Step 3: Update `MainWindow.h`**

Forward declare:

```cpp
class PureCalculationPage;
```

Add:

```cpp
PureCalculationPage *m_pureCalculationPage = nullptr;
```

Remove all `m_pure` widget members, `m_pureCalculationOutput`, `createPureCalculationPage()`, `readPureCalculationInput()`, and `refreshPureCalculation()`.

- [ ] **Step 4: Update `MainWindow.cpp` construction**

Replace:

```cpp
m_pages->addWidget(createPureCalculationPage());
```

with:

```cpp
m_pureCalculationPage = new PureCalculationPage;
m_pages->addWidget(m_pureCalculationPage);
```

Replace the constructor call:

```cpp
refreshPureCalculation();
```

with:

```cpp
m_pureCalculationPage->refresh();
```

Remove the moved methods from `MainWindow.cpp`.

- [ ] **Step 5: Update `app/app.pro`**

Add to `SOURCES`:

```qmake
    ../src/ui/pages/PureCalculationPage.cpp \
```

Add to `HEADERS`:

```qmake
    ../src/ui/pages/PureCalculationPage.h \
```

- [ ] **Step 6: Build and run tests**

Run:

```cmd
build_msvc2015.bat
set PATH=C:\Qt\Qt5.12.9\5.12.9\msvc2015_64\bin;%PATH%&& bin\VisionSelectTests.exe
```

Expected: both commands exit 0.

- [ ] **Step 7: Commit checkpoint if dirty baseline policy allows commits**

Run:

```bash
git add src/ui/pages/PureCalculationPage.h src/ui/pages/PureCalculationPage.cpp src/ui/MainWindow.h src/ui/MainWindow.cpp app/app.pro
git commit -m "refactor: extract pure calculation page"
```

Expected: commit succeeds. If dirty baseline policy is Policy B, skip this step.

## Task 5: Extract ResultsPage and ComparisonPage

**Files:**
- Create: `src/ui/pages/ResultsPage.h`
- Create: `src/ui/pages/ResultsPage.cpp`
- Create: `src/ui/pages/ComparisonPage.h`
- Create: `src/ui/pages/ComparisonPage.cpp`
- Modify: `src/ui/MainWindow.h`
- Modify: `src/ui/MainWindow.cpp`
- Modify: `app/app.pro`

- [ ] **Step 1: Create headers**

Use the `ResultsPage.h` and `ComparisonPage.h` interfaces from Shared Interface Targets.

- [ ] **Step 2: Create `ResultsPage.cpp`**

Move these existing behaviors:

- `createResultsPage()` body into `ResultsPage::ResultsPage(QWidget *parent)`
- `refreshResultTable()` body into `ResultsPage::refreshTable(const SelectionRequest &request)`
- `refreshResultDetails(int row)` body into `ResultsPage::refreshDetails(int row)`

Replace direct `m_results` access with the page member `m_results`. Emit `comparisonRequested()` from the compare button instead of calling `setActivePage()`.

- [ ] **Step 3: Create `ComparisonPage.cpp`**

Move these existing behaviors:

- `createComparisonPage()` body into `ComparisonPage::ComparisonPage(QWidget *parent)`
- `refreshComparisonPage()` body into `ComparisonPage::setResults(const QVector<SelectionResult> &results)`
- `refreshComparisonDetails(int row)` body into `ComparisonPage::refreshDetails(int row)`

Emit `recalculateRequested()` and `exportBomRequested()` from the existing buttons.

- [ ] **Step 4: Update `MainWindow.h`**

Forward declare:

```cpp
class ResultsPage;
class ComparisonPage;
```

Add:

```cpp
ResultsPage *m_resultsPage = nullptr;
ComparisonPage *m_comparisonPage = nullptr;
```

Remove `m_resultSummaryLabel`, `m_resultTable`, `m_resultDetails`, `m_compareTable`, and `m_compareDetails`. Remove declarations for `createResultsPage()`, `createComparisonPage()`, `refreshResultTable()`, `refreshResultDetails(int)`, `refreshComparisonPage()`, and `refreshComparisonDetails(int)`.

- [ ] **Step 5: Update `MainWindow.cpp` construction and refresh**

Construct pages:

```cpp
m_resultsPage = new ResultsPage;
connect(m_resultsPage, &ResultsPage::comparisonRequested, this, [this]() { setActivePage(4); });
m_pages->addWidget(m_resultsPage);

m_comparisonPage = new ComparisonPage;
connect(m_comparisonPage, &ComparisonPage::recalculateRequested, this, &MainWindow::calculate);
connect(m_comparisonPage, &ComparisonPage::exportBomRequested, this, &MainWindow::exportBomCsv);
m_pages->addWidget(m_comparisonPage);
```

After `m_results = engine.select(m_request, m_catalog.cameras(), m_catalog.lenses(), m_catalog.lights(), 12)`, call:

```cpp
m_resultsPage->setResults(m_results, m_request);
m_comparisonPage->setResults(m_results);
```

Remove the moved create and refresh methods from `MainWindow.cpp`.

- [ ] **Step 6: Update `app/app.pro`**

Add to `SOURCES`:

```qmake
    ../src/ui/pages/ResultsPage.cpp \
    ../src/ui/pages/ComparisonPage.cpp \
```

Add to `HEADERS`:

```qmake
    ../src/ui/pages/ResultsPage.h \
    ../src/ui/pages/ComparisonPage.h \
```

- [ ] **Step 7: Build and run tests**

Run:

```cmd
build_msvc2015.bat
set PATH=C:\Qt\Qt5.12.9\5.12.9\msvc2015_64\bin;%PATH%&& bin\VisionSelectTests.exe
```

Expected: both commands exit 0.

- [ ] **Step 8: Commit checkpoint if dirty baseline policy allows commits**

Run:

```bash
git add src/ui/pages/ResultsPage.h src/ui/pages/ResultsPage.cpp src/ui/pages/ComparisonPage.h src/ui/pages/ComparisonPage.cpp src/ui/MainWindow.h src/ui/MainWindow.cpp app/app.pro
git commit -m "refactor: extract result pages"
```

Expected: commit succeeds. If dirty baseline policy is Policy B, skip this step.

## Task 6: Extract ReportPage

**Files:**
- Create: `src/ui/pages/ReportPage.h`
- Create: `src/ui/pages/ReportPage.cpp`
- Modify: `src/ui/MainWindow.h`
- Modify: `src/ui/MainWindow.cpp`
- Modify: `app/app.pro`

- [ ] **Step 1: Create `ReportPage.h`**

Use the `ReportPage.h` interface from Shared Interface Targets.

- [ ] **Step 2: Create `ReportPage.cpp`**

Move these existing behaviors:

- `createReportPage()` body into `ReportPage::ReportPage(QWidget *parent)`
- `refreshReportPreview()` body into `ReportPage::setReportData(const SelectionRequest &request, const QVector<SelectionResult> &results)`

Emit `exportPdfRequested()`, `exportBomRequested()`, and `recalculateRequested()` from the existing buttons.

- [ ] **Step 3: Update `MainWindow.h`**

Forward declare:

```cpp
class ReportPage;
```

Add:

```cpp
ReportPage *m_reportPage = nullptr;
```

Remove `m_reportPreview`, `createReportPage()`, and `refreshReportPreview()`.

- [ ] **Step 4: Update `MainWindow.cpp`**

Construct the page:

```cpp
m_reportPage = new ReportPage;
connect(m_reportPage, &ReportPage::exportPdfRequested, this, &MainWindow::exportPdf);
connect(m_reportPage, &ReportPage::exportBomRequested, this, &MainWindow::exportBomCsv);
connect(m_reportPage, &ReportPage::recalculateRequested, this, &MainWindow::calculate);
m_pages->addWidget(m_reportPage);
```

After every calculation result refresh, call:

```cpp
m_reportPage->setReportData(m_request, m_results);
```

Remove moved methods from `MainWindow.cpp`.

- [ ] **Step 5: Update `app/app.pro`**

Add to `SOURCES`:

```qmake
    ../src/ui/pages/ReportPage.cpp \
```

Add to `HEADERS`:

```qmake
    ../src/ui/pages/ReportPage.h \
```

- [ ] **Step 6: Build and run tests**

Run:

```cmd
build_msvc2015.bat
set PATH=C:\Qt\Qt5.12.9\5.12.9\msvc2015_64\bin;%PATH%&& bin\VisionSelectTests.exe
```

Expected: both commands exit 0.

- [ ] **Step 7: Commit checkpoint if dirty baseline policy allows commits**

Run:

```bash
git add src/ui/pages/ReportPage.h src/ui/pages/ReportPage.cpp src/ui/MainWindow.h src/ui/MainWindow.cpp app/app.pro
git commit -m "refactor: extract report page"
```

Expected: commit succeeds. If dirty baseline policy is Policy B, skip this step.

## Task 7: Extract CalculationPage

**Files:**
- Create: `src/ui/pages/CalculationPage.h`
- Create: `src/ui/pages/CalculationPage.cpp`
- Modify: `src/ui/MainWindow.h`
- Modify: `src/ui/MainWindow.cpp`
- Modify: `app/app.pro`

- [ ] **Step 1: Create `CalculationPage.h`**

Use the `CalculationPage.h` interface from Shared Interface Targets.

- [ ] **Step 2: Create `CalculationPage.cpp`**

Move these existing behaviors:

- `createCalculationPage()` body into `CalculationPage::CalculationPage(QWidget *parent)`
- camera table population from `refreshCalculationAssistant()` into `setCameraEstimates(const QVector<CameraCalculationEstimate> &estimates, const RequirementEstimate &requirement)`
- lens table population from `refreshAssistantLensTable()` into `setLensEstimates(const QVector<LensCalculationEstimate> &estimates)`

Keep `CalculationAssistant::estimateRequirement`, `estimateCameras`, and `estimateLenses` calls in `MainWindow`.

- [ ] **Step 3: Update `MainWindow.h`**

Forward declare:

```cpp
class CalculationPage;
```

Add:

```cpp
CalculationPage *m_calculationPage = nullptr;
```

Remove `m_assistantSummaryLabel`, `m_assistantCameraTable`, `m_assistantLensTable`, and `m_assistantDetails`. Keep `m_assistantCameraEstimates`, `m_assistantLensEstimates`, and `m_assistantSelectedCameraRow` in `MainWindow` for this refactor.

- [ ] **Step 4: Update `MainWindow.cpp`**

Construct the page:

```cpp
m_calculationPage = new CalculationPage;
connect(m_calculationPage, &CalculationPage::inputRequested, this, [this]() { setActivePage(0); });
connect(m_calculationPage, &CalculationPage::recalculateRequested, this, [this]() {
    calculate();
    setActivePage(3);
});
connect(m_calculationPage, &CalculationPage::cameraSelectionChanged, this, [this](int row) {
    m_assistantSelectedCameraRow = row;
    refreshAssistantLensTable();
});
m_pages->addWidget(m_calculationPage);
```

In `refreshCalculationAssistant()`, replace direct widget updates with:

```cpp
const RequirementEstimate requirement = CalculationAssistant::estimateRequirement(m_request);
m_assistantCameraEstimates = CalculationAssistant::estimateCameras(m_request, m_catalog.cameras(), 12);
m_calculationPage->setSummary(requirement, m_catalog.cameras().size(), m_catalog.lenses().size());
m_calculationPage->setCameraEstimates(m_assistantCameraEstimates, requirement);
m_assistantSelectedCameraRow = m_calculationPage->selectedCameraEstimateRow();
refreshAssistantLensTable();
```

In `refreshAssistantLensTable()`, compute `m_assistantLensEstimates` as today and call:

```cpp
m_calculationPage->setLensEstimates(m_assistantLensEstimates);
```

- [ ] **Step 5: Update `app/app.pro`**

Add to `SOURCES`:

```qmake
    ../src/ui/pages/CalculationPage.cpp \
```

Add to `HEADERS`:

```qmake
    ../src/ui/pages/CalculationPage.h \
```

- [ ] **Step 6: Build and run tests**

Run:

```cmd
build_msvc2015.bat
set PATH=C:\Qt\Qt5.12.9\5.12.9\msvc2015_64\bin;%PATH%&& bin\VisionSelectTests.exe
```

Expected: both commands exit 0.

- [ ] **Step 7: Commit checkpoint if dirty baseline policy allows commits**

Run:

```bash
git add src/ui/pages/CalculationPage.h src/ui/pages/CalculationPage.cpp src/ui/MainWindow.h src/ui/MainWindow.cpp app/app.pro
git commit -m "refactor: extract calculation page"
```

Expected: commit succeeds. If dirty baseline policy is Policy B, skip this step.

## Task 8: Extract CatalogPage

**Files:**
- Create: `src/ui/pages/CatalogPage.h`
- Create: `src/ui/pages/CatalogPage.cpp`
- Modify: `src/ui/MainWindow.h`
- Modify: `src/ui/MainWindow.cpp`
- Modify: `app/app.pro`

- [ ] **Step 1: Create `CatalogPage.h`**

Use the `CatalogPage.h` interface from Shared Interface Targets.

- [ ] **Step 2: Create `CatalogPage.cpp`**

Move these existing behaviors into `CatalogPage`:

- `createCatalogPage()` body into `CatalogPage::CatalogPage(QWidget *parent)`
- `refreshCatalogFilterOptions()` into `CatalogPage::refreshFilterOptions()`
- `refreshCameraTable()` into `CatalogPage::refreshCameraTable()`
- `refreshLensTable()` into `CatalogPage::refreshLensTable()`
- `refreshLightTable()` into `CatalogPage::refreshLightTable()`
- `selectedCameraCatalogIndex()` into `CatalogPage::selectedCameraIndex() const`
- `selectedLensCatalogIndex()` into `CatalogPage::selectedLensIndex() const`
- `selectedLightCatalogIndex()` into `CatalogPage::selectedLightIndex() const`
- `visibleCameraCatalogIndexes()` into `CatalogPage::visibleCameraIndexes() const`
- `visibleLensCatalogIndexes()` into `CatalogPage::visibleLensIndexes() const`
- `visibleLightCatalogIndexes()` into `CatalogPage::visibleLightIndexes() const`
- `clearCameraFilters()`, `clearLensFilters()`, and `clearLightFilters()` as private slots or lambdas inside the page

Store a non-owning `const CatalogRepository *m_catalog` and refresh tables from it.

- [ ] **Step 3: Update `MainWindow.h`**

Forward declare:

```cpp
class CatalogPage;
```

Add:

```cpp
CatalogPage *m_catalogPage = nullptr;
```

Remove catalog table, filter, and row-map members from `MainWindow.h`.

- [ ] **Step 4: Update `MainWindow.cpp` construction**

Construct the page:

```cpp
m_catalogPage = new CatalogPage;
m_catalogPage->setCatalog(m_catalog);
connect(m_catalogPage, &CatalogPage::addCameraRequested, this, &MainWindow::addCamera);
connect(m_catalogPage, &CatalogPage::editCameraRequested, this, &MainWindow::editCamera);
connect(m_catalogPage, &CatalogPage::removeCameraRequested, this, &MainWindow::removeCamera);
connect(m_catalogPage, &CatalogPage::addLensRequested, this, &MainWindow::addLens);
connect(m_catalogPage, &CatalogPage::editLensRequested, this, &MainWindow::editLens);
connect(m_catalogPage, &CatalogPage::removeLensRequested, this, &MainWindow::removeLens);
connect(m_catalogPage, &CatalogPage::addLightRequested, this, &MainWindow::addLight);
connect(m_catalogPage, &CatalogPage::editLightRequested, this, &MainWindow::editLight);
connect(m_catalogPage, &CatalogPage::removeLightRequested, this, &MainWindow::removeLight);
connect(m_catalogPage, &CatalogPage::importCamerasRequested, this, &MainWindow::importCameras);
connect(m_catalogPage, &CatalogPage::importLensesRequested, this, &MainWindow::importLenses);
connect(m_catalogPage, &CatalogPage::importLightsRequested, this, &MainWindow::importLights);
connect(m_catalogPage, &CatalogPage::exportCamerasRequested, this, &MainWindow::exportCameras);
connect(m_catalogPage, &CatalogPage::exportLensesRequested, this, &MainWindow::exportLenses);
connect(m_catalogPage, &CatalogPage::exportLightsRequested, this, &MainWindow::exportLights);
connect(m_catalogPage, &CatalogPage::exportFilteredCamerasRequested, this, &MainWindow::exportFilteredCameras);
connect(m_catalogPage, &CatalogPage::exportFilteredLensesRequested, this, &MainWindow::exportFilteredLenses);
connect(m_catalogPage, &CatalogPage::exportFilteredLightsRequested, this, &MainWindow::exportFilteredLights);
connect(m_catalogPage, &CatalogPage::resetCamerasRequested, this, &MainWindow::resetCameras);
connect(m_catalogPage, &CatalogPage::resetLensesRequested, this, &MainWindow::resetLenses);
connect(m_catalogPage, &CatalogPage::resetLightsRequested, this, &MainWindow::resetLights);
m_pages->addWidget(m_catalogPage);
```

- [ ] **Step 5: Update repository action methods**

Replace selected index calls:

```cpp
const int index = m_catalogPage->selectedCameraIndex();
const int index = m_catalogPage->selectedLensIndex();
const int index = m_catalogPage->selectedLightIndex();
```

Replace filtered export calls:

```cpp
m_catalog.exportCameraCsv(path, m_catalogPage->visibleCameraIndexes(), &error)
m_catalog.exportLensCsv(path, m_catalogPage->visibleLensIndexes(), &error)
m_catalog.exportLightCsv(path, m_catalogPage->visibleLightIndexes(), &error)
```

After every catalog mutation or import/reset, call:

```cpp
m_catalogPage->setCatalog(m_catalog);
```

- [ ] **Step 6: Update `app/app.pro`**

Add to `SOURCES`:

```qmake
    ../src/ui/pages/CatalogPage.cpp \
```

Add to `HEADERS`:

```qmake
    ../src/ui/pages/CatalogPage.h \
```

- [ ] **Step 7: Build and run tests**

Run:

```cmd
build_msvc2015.bat
set PATH=C:\Qt\Qt5.12.9\5.12.9\msvc2015_64\bin;%PATH%&& bin\VisionSelectTests.exe
```

Expected: both commands exit 0.

- [ ] **Step 8: Commit checkpoint if dirty baseline policy allows commits**

Run:

```bash
git add src/ui/pages/CatalogPage.h src/ui/pages/CatalogPage.cpp src/ui/MainWindow.h src/ui/MainWindow.cpp app/app.pro
git commit -m "refactor: extract catalog page"
```

Expected: commit succeeds. If dirty baseline policy is Policy B, skip this step.

## Task 9: Final MainWindow Cleanup and Verification

**Files:**
- Modify: `src/ui/MainWindow.h`
- Modify: `src/ui/MainWindow.cpp`
- Modify: `app/app.pro`

- [ ] **Step 1: Remove obsolete declarations**

Remove any leftover page creation declarations from `MainWindow.h`:

- `createInputPage`
- `createPureCalculationPage`
- `createCalculationPage`
- `createResultsPage`
- `createComparisonPage`
- `createCatalogPage`
- `createReportPage`

Remove obsolete page-internal refresh declarations that were moved to page classes.

- [ ] **Step 2: Remove unused includes from `MainWindow.cpp`**

Keep includes required for:

- `CatalogDialogs`
- page classes
- `PdfReportWriter`
- `SelectionEngine`
- Qt file dialogs and message boxes used by coordinator actions

Remove includes only when the compiler confirms they are unused by `MainWindow.cpp`.

- [ ] **Step 3: Verify qmake source lists**

Confirm `app/app.pro` contains every new `.cpp` and `.h` file from this plan. Confirm `tests/tests.pro` remains unchanged because the tests do not compile UI pages.

- [ ] **Step 4: Full build and tests**

Run:

```cmd
build_msvc2015.bat
set PATH=C:\Qt\Qt5.12.9\5.12.9\msvc2015_64\bin;%PATH%&& bin\VisionSelectTests.exe
```

Expected: both commands exit 0.

- [ ] **Step 5: Manual UI verification**

Launch:

```cmd
set PATH=C:\Qt\Qt5.12.9\5.12.9\msvc2015_64\bin;%PATH%&& bin\VisionSelect.exe
```

Verify:

- sidebar navigation reaches every page in the same order
- demand input calculation still fills results
- pure calculation compute button updates output
- pure calculation reset button restores defaults
- product calculation assistant refreshes camera and lens tables
- results table row selection updates details
- comparison table row selection updates details
- catalog filters still filter each table
- catalog add, edit, remove actions still open the same dialogs
- catalog import, export, filtered export, and reset actions still work
- BOM CSV export opens the same save flow
- PDF export opens the same save flow

- [ ] **Step 6: Final diff review**

Run:

```bash
git diff --stat
git diff -- src/ui/MainWindow.h src/ui/MainWindow.cpp app/app.pro
```

Expected: `MainWindow` is smaller and page internals live in page classes. No generated `Makefile` changes are present.

- [ ] **Step 7: Final commit if dirty baseline policy allows commits**

Run:

```bash
git add src/ui app/app.pro
git commit -m "refactor: split MainWindow pages"
```

Expected: commit succeeds. If dirty baseline policy is Policy B, skip this step and report the final diff state.

## Self-Review

Spec coverage:

- Page-level split is covered by Tasks 3 through 8.
- Shared helpers are covered by Task 1.
- Catalog dialogs are covered by Task 2.
- `MainWindow` shell cleanup is covered by Task 9.
- `app/app.pro` updates are included in every task that adds files.
- Generated `Makefile` files are explicitly out of scope.
- Build, tests, and manual UI verification are included.

Placeholder scan:

- The plan contains no unresolved markers, incomplete sections, or unspecified file paths.

Type consistency:

- Page class names match the target filenames.
- Signal names in construction snippets match the header declarations.
- `CatalogPage` selected and visible index methods match the repository action replacements.
