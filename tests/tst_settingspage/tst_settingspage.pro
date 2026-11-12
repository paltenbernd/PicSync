TARGET = tst_settingspage
include(../tests.pri)
QT += widgets
APP = $$PWD/../../src/app
INCLUDEPATH += $$APP
HEADERS += $$APP/settingspage.h $$APP/syncsession.h $$APP/uihelpers.h $$APP/platform/platform.h
SOURCES += tst_settingspage.cpp \
    $$APP/settingspage.cpp $$APP/syncsession.cpp $$APP/uihelpers.cpp \
    $$APP/platform/platform_desktop.cpp
RESOURCES += $$APP/picsync.qrc
