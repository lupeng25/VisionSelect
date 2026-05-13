# MainWindow Page Split Design

## Purpose

Split `MainWindow` into page-level UI components while preserving existing behavior. The goal is to reduce `src/ui/MainWindow.cpp` size and isolate page responsibilities without changing navigation, calculations, table columns, labels, CSV/PDF behavior, or catalog storage.

## Current Problem

`MainWindow` currently owns the application shell and most page implementations:

- demand input page
- pure calculation page
- product calculation assistant page
- results page
- comparison page
- catalog maintenance page
- report page
- camera, lens, and light edit dialogs
- CSV import/export, BOM export, PDF export, and catalog reset actions

This makes `MainWindow.cpp` difficult to review and increases risk when adding UI features. The current file is especially sensitive because there are active changes around pure calculation, calculation assistant behavior, and tests.

## Design Choice

Use a behavior-preserving page split. `MainWindow` remains the application shell and workflow coordinator. New page classes own their widgets, expose focused read/refresh methods, and emit signals for actions that still need `MainWindow` or repository access.

This keeps the first refactor small enough to verify while creating clear boundaries for future work.

## New Files

Add page components under `src/ui/pages/`:

- `InputPage.h/.cpp`
- `PureCalculationPage.h/.cpp`
- `CalculationPage.h/.cpp`
- `ResultsPage.h/.cpp`
- `ComparisonPage.h/.cpp`
- `CatalogPage.h/.cpp`
- `ReportPage.h/.cpp`

Add shared UI support:

- `src/ui/UiHelpers.h/.cpp`
- `src/ui/CatalogDialogs.h/.cpp`

Update `app/app.pro` to include the new headers and sources. Do not edit generated `Makefile` files.

## MainWindow Responsibilities

`MainWindow` keeps:

- `CatalogRepository m_catalog`
- `QVector<SelectionResult> m_results`
- `SelectionRequest m_request`
- sidebar navigation and `QStackedWidget`
- `calculate()`
- catalog import/export/reset actions
- BOM CSV export
- PDF export
- cross-page coordination
- error and completion message boxes

`MainWindow` should stop storing direct widget members for page internals. It should hold page pointers instead, for example `InputPage *m_inputPage`, `ResultsPage *m_resultsPage`, and similar.

## Page Responsibilities

### InputPage

Own the demand input controls currently created by `createInputPage()`.

Expose:

- `SelectionRequest request() const`

Emit:

- `calculateRequested()`
- `resultsRequested()`

### PureCalculationPage

Own the manual camera, lens, light, and request controls currently created by `createPureCalculationPage()`.

Expose:

- `PureCalculationInput input() const`
- `void refresh()`
- `void resetDefaults()`

It may call `CalculationAssistant::estimatePure()` directly because this page is a self-contained calculator and already does not depend on repository state.

### CalculationPage

Own assistant summary, camera table, lens table, and details text.

Expose:

- `void setRequest(const SelectionRequest &request)`
- `void setCameraEstimates(const QVector<CameraCalculationEstimate> &estimates, const RequirementEstimate &requirement)`
- `void setLensEstimates(const QVector<LensCalculationEstimate> &estimates)`
- `int selectedCameraEstimateRow() const`

Emit:

- `inputRequested()`
- `recalculateRequested()`

`MainWindow` will continue calling `CalculationAssistant` during this refactor. Moving assistant calculation orchestration fully into the page is explicitly out of scope for this design.

### ResultsPage

Own result table, result summary, and result details.

Expose:

- `void setResults(const QVector<SelectionResult> &results, const SelectionRequest &request)`

Emit:

- `comparisonRequested()`

### ComparisonPage

Own top-result comparison table and details.

Expose:

- `void setResults(const QVector<SelectionResult> &results)`

Emit:

- `recalculateRequested()`
- `exportBomRequested()`

### CatalogPage

Own catalog tabs, filters, tables, and visible-row maps.

Expose:

- `void setCatalog(const CatalogRepository &catalog)`
- `int selectedCameraIndex() const`
- `int selectedLensIndex() const`
- `int selectedLightIndex() const`
- `QVector<int> visibleCameraIndexes() const`
- `QVector<int> visibleLensIndexes() const`
- `QVector<int> visibleLightIndexes() const`

Emit action signals:

- `addCameraRequested()`, `editCameraRequested()`, `removeCameraRequested()`
- `addLensRequested()`, `editLensRequested()`, `removeLensRequested()`
- `addLightRequested()`, `editLightRequested()`, `removeLightRequested()`
- `importCamerasRequested()`, `importLensesRequested()`, `importLightsRequested()`
- `exportCamerasRequested()`, `exportLensesRequested()`, `exportLightsRequested()`
- `exportFilteredCamerasRequested()`, `exportFilteredLensesRequested()`, `exportFilteredLightsRequested()`
- `resetCamerasRequested()`, `resetLensesRequested()`, `resetLightsRequested()`

`MainWindow` remains responsible for repository mutation and error handling.

### ReportPage

Own report preview and export buttons.

Expose:

- `void setReportData(const SelectionRequest &request, const QVector<SelectionResult> &results)`

Emit:

- `exportPdfRequested()`
- `exportBomRequested()`
- `recalculateRequested()`

## Shared Helpers

Move generic UI helpers from `MainWindow.cpp` into `UiHelpers` where they are shared by multiple pages:

- table item creation
- source-index item helpers
- row source index lookup
- table copy shortcut setup
- table setup
- numeric formatting
- product label formatting
- risk summary formatting
- exposure text formatting

Keep helpers small and free of repository mutation.

Move `editCameraDialog`, `editLensDialog`, and `editLightDialog` into `CatalogDialogs`. Preserve dialog fields, defaults, labels, and accepted values.

## Behavior Preservation Rules

Do not intentionally change:

- page order
- sidebar labels
- button labels
- table columns or column order
- calculation formulas
- scoring behavior
- CSV import/export behavior
- BOM export format
- PDF export behavior
- catalog reset behavior
- user-facing validation messages

Any necessary signal/slot or include changes are allowed only to preserve current behavior after moving code.

## Implementation Order

1. Extract `UiHelpers` and `CatalogDialogs`.
2. Extract `InputPage` and keep `MainWindow::calculate()` as the coordinator.
3. Extract `PureCalculationPage` because it has the largest new widget state.
4. Extract `ResultsPage` and `ComparisonPage`.
5. Extract `CalculationPage`.
6. Extract `CatalogPage`.
7. Extract `ReportPage`.
8. Shrink `MainWindow.h/.cpp` to shell and coordinator responsibilities.
9. Update `app/app.pro`.

Each step should compile before moving to the next when practical.

## Verification

Run:

```cmd
build_msvc2015.bat
set PATH=C:\Qt\Qt5.12.9\5.12.9\msvc2015_64\bin;%PATH%&& bin\VisionSelectTests.exe
```

Manual UI verification should cover:

- sidebar navigation through every page
- demand input calculation
- pure calculation compute and reset defaults
- product calculation assistant refresh and lens table behavior
- results table and details
- comparison page and details
- catalog filters, add/edit/remove, import/export, filtered export, and reset actions
- BOM CSV export
- PDF export

## Out Of Scope

- Changing selection formulas
- Redesigning the UI
- Renaming user-facing Chinese labels
- Reworking catalog persistence
- Introducing new dependencies
- Changing generated `Makefile` files

## Risks

- Signal/slot rewiring can silently miss an action if not manually verified.
- Splitting catalog tables before stabilizing visible index behavior could break filtered exports.
- Moving shared helpers can create include cycles if page headers expose too many Qt details.
- Current uncommitted pure calculation changes increase merge risk, so implementation should be small-step and behavior-preserving.
