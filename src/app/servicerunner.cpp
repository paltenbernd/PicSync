// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "servicerunner.h"
#include "platform/platform.h"
#include "syncsession.h"

namespace PicSync {

// Synchronises once and tells the JobService that the work is done.
void ServiceRunner::run()
{
    SyncSession session;
    session.run(false);
    // The JobService unbinds when it receives this; Android then ends the process.
    Platform::signalServiceFinished();
}

} // namespace PicSync
