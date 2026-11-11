# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
#
# Platform independent sync core, shared by the app and the unit tests.

QT *= core network
INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

HEADERS += \
    $$PWD/appsettings.h \
    $$PWD/directorymediastore.h \
    $$PWD/interfaces.h \
    $$PWD/pathutils.h \
    $$PWD/statestore.h \
    $$PWD/syncengine.h \
    $$PWD/synchistory.h \
    $$PWD/syncmodel.h \
    $$PWD/syncplanner.h \
    $$PWD/webdavclient.h \
    $$PWD/webdavparser.h

SOURCES += \
    $$PWD/appsettings.cpp \
    $$PWD/directorymediastore.cpp \
    $$PWD/pathutils.cpp \
    $$PWD/statestore.cpp \
    $$PWD/syncengine.cpp \
    $$PWD/synchistory.cpp \
    $$PWD/syncplanner.cpp \
    $$PWD/webdavclient.cpp \
    $$PWD/webdavparser.cpp
