QT += core gui widgets printsupport

CONFIG += c++14 warn_on
TEMPLATE = app
TARGET = VisionSelect
DESTDIR = ../bin

INCLUDEPATH += ../src

win32-msvc*: QMAKE_CXXFLAGS += /utf-8 /wd4819
win32: LIBS += -lbcrypt
win32: RC_FILE = ../resources/windows/VisionSelect.rc
TRANSLATIONS += \
    ../resources/i18n/visionselect_zh_CN.ts \
    ../resources/i18n/visionselect_en_US.ts

SOURCES += \
    ../src/main.cpp \
    ../src/catalog/CatalogRepository.cpp \
    ../src/core/SelectionTypes.cpp \
    ../src/i18n/LanguageManager.cpp \
    ../src/license/LicenseManager.cpp \
    ../src/report/PdfReportWriter.cpp \
    ../src/selection/CalculationAssistant.cpp \
    ../src/selection/SelectionEngine.cpp \
    ../src/three_d/ThreeDCameraMatcher.cpp \
    ../src/three_d/ThreeDCameraRepository.cpp \
    ../src/three_d/ThreeDCameraTypes.cpp \
    ../src/ui/CatalogDialogs.cpp \
    ../src/ui/LicenseDialog.cpp \
    ../src/ui/UiHelpers.cpp \
    ../src/ui/MainWindow.cpp \
    ../src/ui/pages/CalculationPage.cpp \
    ../src/ui/pages/CatalogPage.cpp \
    ../src/ui/pages/ComparisonPage.cpp \
    ../src/ui/pages/InputPage.cpp \
    ../src/ui/pages/PureCalculationPage.cpp \
    ../src/ui/pages/ReportPage.cpp \
    ../src/ui/pages/ResultsPage.cpp \
    ../src/ui/pages/ThreeDCameraPage.cpp

HEADERS += \
    ../src/catalog/CatalogRepository.h \
    ../src/core/SelectionTypes.h \
    ../src/i18n/LanguageManager.h \
    ../src/license/LicenseManager.h \
    ../src/report/PdfReportWriter.h \
    ../src/selection/CalculationAssistant.h \
    ../src/selection/SelectionEngine.h \
    ../src/three_d/ThreeDCameraMatcher.h \
    ../src/three_d/ThreeDCameraRepository.h \
    ../src/three_d/ThreeDCameraTypes.h \
    ../src/ui/CatalogDialogs.h \
    ../src/ui/LicenseDialog.h \
    ../src/ui/UiHelpers.h \
    ../src/ui/MainWindow.h \
    ../src/ui/pages/CalculationPage.h \
    ../src/ui/pages/CatalogPage.h \
    ../src/ui/pages/ComparisonPage.h \
    ../src/ui/pages/InputPage.h \
    ../src/ui/pages/PureCalculationPage.h \
    ../src/ui/pages/ReportPage.h \
    ../src/ui/pages/ResultsPage.h \
    ../src/ui/pages/ThreeDCameraPage.h

RESOURCES += ../resources/resources.qrc
