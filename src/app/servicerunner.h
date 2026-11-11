// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include <QObject>

namespace PicSync {

/// Runs one background synchronisation inside the Android sync service process.
class ServiceRunner : public QObject
{
    Q_OBJECT
public Q_SLOTS:
    // Synchronises once and tells the JobService that the work is done.
    void run();
};

} // namespace PicSync
