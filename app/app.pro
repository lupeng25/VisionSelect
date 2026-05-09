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
    ../src/selection/SelectionEngine.cpp \
    ../src/ui/MainWindow.cpp

HEADERS += \
    ../src/catalog/CatalogRepository.h \
    ../src/core/SelectionTypes.h \
    ../src/report/PdfReportWriter.h \
    ../src/selection/SelectionEngine.h \
    ../src/ui/MainWindow.h

RESOURCES += ../resources/resources.qrc
