# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
#
# Top-level project of PicSync: the Android app and (on desktop hosts)
# the unit tests. The tests run automatically after they are linked, so
# a failing test breaks the build.

TEMPLATE = subdirs

SUBDIRS = app
app.subdir = src/app

!android {
    SUBDIRS += tests
    tests.subdir = tests
}

OTHER_FILES += README.md LICENSE .github/workflows/build.yml
