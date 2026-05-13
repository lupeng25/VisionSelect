QT += core gui widgets printsupport

CONFIG += c++14 warn_on
TEMPLATE = app
TARGET = VisionSelect
DESTDIR = ../bin

INCLUDEPATH += ../src

win32-msvc*: QMAKE_CXXFLAGS += /utf-8 /wd4819

SOURCES += \
    ../src/main.cpp \
    ../src/catalog/CatalogRepository.cpp \
    ../src/core/SelectionTypes.cpp \
    ../src/report/PdfReportWriter.cpp \
    ../src/selection/CalculationAssistant.cpp \
    ../src/selection/SelectionEngine.cpp \
    ../src/ui/CatalogDialogs.cpp \
    ../src/ui/UiHelpers.cpp \
    ../src/ui/MainWindow.cpp \
    ../src/ui/pages/CalculationPage.cpp \
    ../src/ui/pages/CatalogPage.cpp \
    ../src/ui/pages/ComparisonPage.cpp \
    ../src/ui/pages/InputPage.cpp \
    ../src/ui/pages/PureCalculationPage.cpp \
    ../src/ui/pages/ReportPage.cpp \
    ../src/ui/pages/ResultsPage.cpp

HEADERS += \
    ../src/catalog/CatalogRepository.h \
    ../src/core/SelectionTypes.h \
    ../src/report/PdfReportWriter.h \
    ../src/selection/CalculationAssistant.h \
    ../src/selection/SelectionEngine.h \
    ../src/ui/CatalogDialogs.h \
    ../src/ui/UiHelpers.h \
    ../src/ui/MainWindow.h \
    ../src/ui/pages/CalculationPage.h \
    ../src/ui/pages/CatalogPage.h \
    ../src/ui/pages/ComparisonPage.h \
    ../src/ui/pages/InputPage.h \
    ../src/ui/pages/PureCalculationPage.h \
    ../src/ui/pages/ReportPage.h \
    ../src/ui/pages/ResultsPage.h

RESOURCES += ../resources/resources.qrc
