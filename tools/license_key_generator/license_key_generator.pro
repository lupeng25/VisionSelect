QT += core gui widgets

CONFIG += c++14 warn_on
TEMPLATE = app
TARGET = LicenseKeyGenerator
DESTDIR = $$PWD/../../bin

INCLUDEPATH += $$PWD \
    $$PWD/../../src

win32-msvc*: QMAKE_CXXFLAGS += /utf-8 /wd4819
win32: LIBS += -lbcrypt

SOURCES += \
    main.cpp \
    LicenseGeneratorWindow.cpp \
    ../../src/license/LicenseIssuer.cpp

HEADERS += \
    LicenseGeneratorWindow.h \
    ../../src/license/LicenseIssuer.h
