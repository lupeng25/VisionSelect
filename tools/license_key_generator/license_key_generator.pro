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
    ../../src/core/Localization.cpp \
    ../../src/i18n/LanguageManager.cpp \
    ../../src/license/LicenseIssuer.cpp

HEADERS += \
    LicenseGeneratorWindow.h \
    ../../src/core/Localization.h \
    ../../src/i18n/LanguageManager.h \
    ../../src/license/LicenseIssuer.h
