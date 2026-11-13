# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
#
# Settings shared by every qmake project of PicSync.

CONFIG += c++17 warn_on
DEFINES += QT_DISABLE_DEPRECATED_UP_TO=0x050F00 QT_NO_CAST_FROM_BYTEARRAY
PICSYNC_VERSION = 1.0.0
# Android versionCode of this release (F-Droid reads it from here).
# Raise it together with PICSYNC_VERSION for every release.
PICSYNC_VERSION_CODE = 1
DEFINES += PICSYNC_VERSION=\\\"$$PICSYNC_VERSION\\\"
