QT += core gui widgets testlib

CONFIG += c++14 console testcase warn_on
TEMPLATE = app
TARGET = VisionSelectTests
DESTDIR = ../bin

INCLUDEPATH += ../src

win32-msvc*: QMAKE_CXXFLAGS += /utf-8 /wd4819
win32: LIBS += -lbcrypt

SOURCES += \
    test_selection.cpp \
    ../src/catalog/CatalogRepository.cpp \
    ../src/core/SelectionTypes.cpp \
    ../src/i18n/LanguageManager.cpp \
    ../src/license/LicenseIssuer.cpp \
    ../src/license/LicenseManager.cpp \
    ../src/report/PdfReportWriter.cpp \
    ../src/selection/CalculationAssistant.cpp \
    ../src/selection/SelectionEngine.cpp \
    ../src/three_d/ThreeDCalculation.cpp \
    ../src/three_d/ThreeDCameraMatcher.cpp \
    ../src/three_d/ThreeDCameraRepository.cpp \
    ../src/three_d/ThreeDCameraTypes.cpp \
    ../src/ui/UiHelpers.cpp

HEADERS += \
    ../src/catalog/CatalogRepository.h \
    ../src/core/SelectionTypes.h \
    ../src/i18n/LanguageManager.h \
    ../src/license/LicenseIssuer.h \
    ../src/license/LicenseManager.h \
    ../src/report/PdfReportWriter.h \
    ../src/selection/CalculationAssistant.h \
    ../src/selection/SelectionEngine.h \
    ../src/three_d/ThreeDCalculation.h \
    ../src/three_d/ThreeDCameraMatcher.h \
    ../src/three_d/ThreeDCameraRepository.h \
    ../src/three_d/ThreeDCameraTypes.h \
    ../src/ui/UiHelpers.h

RESOURCES += ../resources/resources.qrc
