QT += core gui testlib

CONFIG += c++14 console testcase warn_on
TEMPLATE = app
TARGET = VisionSelectTests
DESTDIR = ../bin

INCLUDEPATH += ../src

win32-msvc*: QMAKE_CXXFLAGS += /utf-8 /wd4819

SOURCES += \
    test_selection.cpp \
    ../src/catalog/CatalogRepository.cpp \
    ../src/core/SelectionTypes.cpp \
    ../src/report/PdfReportWriter.cpp \
    ../src/selection/CalculationAssistant.cpp \
    ../src/selection/SelectionEngine.cpp

HEADERS += \
    ../src/catalog/CatalogRepository.h \
    ../src/core/SelectionTypes.h \
    ../src/report/PdfReportWriter.h \
    ../src/selection/CalculationAssistant.h \
    ../src/selection/SelectionEngine.h

RESOURCES += ../resources/resources.qrc
