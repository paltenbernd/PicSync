TARGET = tst_servicerunner
include(../tests.pri)
APP = $$PWD/../../src/app
INCLUDEPATH += $$APP
HEADERS += $$APP/servicerunner.h $$APP/synccontroller.h $$APP/syncsession.h \
    $$APP/syncprogress.h $$APP/platform/platform.h
SOURCES += tst_servicerunner.cpp \
    $$APP/servicerunner.cpp $$APP/synccontroller.cpp $$APP/syncsession.cpp \
    $$APP/syncprogress.cpp $$APP/platform/platform_desktop.cpp
