# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
#
# Unit tests of the sync core. Every test binary is executed right after it
# is linked (see tests.pri), so "make" fails as soon as a test fails.
# "make check" runs them again on demand.

TEMPLATE = subdirs
SUBDIRS = \
    tst_pathutils \
    tst_webdavparser \
    tst_statestore \
    tst_syncplanner \
    tst_synchistory \
    tst_syncengine
