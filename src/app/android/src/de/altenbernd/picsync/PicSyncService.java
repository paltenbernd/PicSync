// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

package de.altenbernd.picsync;

import org.qtproject.qt.android.bindings.QtService;

/**
 * Hosts the C++ sync engine in the separate ":sync" process.
 *
 * Qt starts the native main() of PicSync with the argument "-service"
 * (see AndroidManifest.xml); main.cpp then runs one synchronisation and
 * reports back via a broadcast. Android requires services to be Java
 * classes, so this class only exists to give the Qt service a name.
 */
public class PicSyncService extends QtService
{
}
