# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
#
# Common configuration of all unit test projects.

include(../common.pri)
include(../src/core/core.pri)

TEMPLATE = app
QT += testlib
!contains(QT, widgets): QT -= gui
CONFIG += testcase console
CONFIG -= app_bundle

INCLUDEPATH += $$PWD

# Run the test as part of the build: a failing test aborts "make".
QMAKE_POST_LINK += $$shell_quote($$OUT_PWD/$$TARGET)
