// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "synccontroller.h"

namespace PicSync {

// Creates an idle controller.
SyncController::SyncController()
{
    qRegisterMetaType<PicSync::SyncResult>();
}

// Stops a running synchronisation and waits for the worker thread.
SyncController::~SyncController()
{
    stop();
    shutdown();
}

// Returns true while a synchronisation is running.
bool SyncController::isRunning() const
{
    return static_cast<bool>(m_session);
}

// Starts a manual synchronisation unless one is already running.
void SyncController::start()
{
    if (isRunning())
        return;
    m_thread = std::make_unique<QThread>();
    m_session = std::make_unique<SyncSession>();
    m_session->moveToThread(m_thread.get());

    connect(m_thread.get(), &QThread::started, m_session.get(), &SyncSession::runManual);
    connect(m_session.get(), &SyncSession::progress, this, &SyncController::progress);
    connect(m_session.get(), &SyncSession::phaseChanged, this, &SyncController::phaseChanged);
    connect(m_session.get(), &SyncSession::finished, this, &SyncController::onSessionFinished);

    Q_EMIT started();
    m_thread->start();
}

// Asks the running synchronisation to stop after the current file.
void SyncController::stop()
{
    if (m_session)
        m_session->requestStop();
}

// Cleans up the worker thread and forwards the result.
void SyncController::onSessionFinished(const PicSync::SyncResult &result)
{
    shutdown();
    Q_EMIT finished(result);
}

// Ends the worker thread and releases the session.
void SyncController::shutdown()
{
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
    }
    m_session.reset();
    m_thread.reset();
}

} // namespace PicSync
