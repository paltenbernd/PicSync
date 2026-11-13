TARGET = tst_dashboardpage
include(../tests.pri)
QT += widgets
APP = $$PWD/../../src/app
INCLUDEPATH += $$APP
HEADERS += $$APP/dashboardpage.h $$APP/syncsession.h $$APP/uihelpers.h $$APP/platform/platform.h
SOURCES += tst_dashboardpage.cpp \
    $$APP/dashboardpage.cpp $$APP/syncsession.cpp $$APP/uihelpers.cpp \
    $$APP/platform/platform_desktop.cpp
RESOURCES += $$APP/picsync.qrc
DEFINES += PICSYNC_BUILD=\\\"test\\\"
