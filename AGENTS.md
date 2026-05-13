# VisionSelect Agent Guide

## Project Overview
- VisionSelect is a C++14 Qt Widgets application for industrial machine-vision component selection.
- The root qmake project is `VisionSelect.pro`, with app and test subprojects under `app/` and `tests/`.
- The application binary is `bin/VisionSelect.exe`; the QtTest binary is `bin/VisionSelectTests.exe`.

## Build And Test
- Build: run `build_msvc2015.bat` from the repository root.
- Test: run `set PATH=C:\Qt\Qt5.12.9\5.12.9\msvc2015_64\bin;%PATH%&& bin\VisionSelectTests.exe` from the repository root.
- The expected local toolchain is Qt 5.12.9 MSVC2015 x64 plus the Visual Studio 2015 C++ tools.

## Module Map
- `src/catalog/`: loads, stores, and persists camera, lens, and light catalog data.
- `src/core/`: shared selection data types, labels, parsers, and compatibility helpers.
- `src/selection/`: selection engine, scoring, requirement estimates, and calculation helpers.
- `src/ui/`: Qt Widgets UI, navigation, forms, tables, reports, and user-facing workflows.
- `src/report/`: PDF report generation.

## Working Rules
- Preserve user edits. Inspect `git status --short` before making changes, and do not revert unrelated work.
- Do not edit generated `Makefile` files unless explicitly asked.
- Prefer tests in `tests/test_selection.cpp` for selection logic and calculation behavior.
- UI changes need manual verification notes in the final response, especially navigation, layout, and text-fit checks.
- Keep changes scoped to the requested module and follow the existing Qt/qmake style.
