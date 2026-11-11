// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "syncsession.h"
#include "appsettings.h"
#include "pathutils.h"
#include "platform/platform.h"
#include "statestore.h"
#include "synchistory.h"
#include "webdavclient.h"

#include <QDateTime>
#include <QLockFile>
#include <QMutexLocker>
#include <QSettings>

namespace PicSync {

namespace {
const int kStaleLockMs = 30 * 60 * 1000;
}

// Creates a session; nothing happens until run() is called.
SyncSession::SyncSession() = default;

// Destroys the session.
SyncSession::~SyncSession() = default;

// Returns the file that holds the sync state.
QString SyncSession::stateFilePath()
{
    return Platform::dataDirectory() + QStringLiteral("/syncstate.json");
}

// Returns the file that holds the sync history.
QString SyncSession::historyFilePath()
{
    return Platform::dataDirectory() + QStringLiteral("/history.json");
}

// Asks a running synchronisation to stop after the current file.
void SyncSession::requestStop()
{
    m_stopRequested = true;
    QMutexLocker locker(&m_mutex);
    if (m_engine)
        m_engine->requestStop();
}

// Runs a synchronisation started by the user and emits finished().
void SyncSession::runManual()
{
    Q_EMIT finished(run(true));
}

// Runs a background synchronisation and emits finished().
void SyncSession::runScheduled()
{
    Q_EMIT finished(run(false));
}

// Forwards engine progress to the notification and to listeners.
void SyncSession::onProgress(int done, int total, const QString &currentFile)
{
    if (total > 0 && done < total)
        Platform::showProgress(done, total, currentFile);
    Q_EMIT progress(done, total, currentFile);
}

// Runs the synchronisation in the calling thread and returns its result.
SyncResult SyncSession::run(bool manual)
{
    SyncResult result;
    result.manual = manual;
    result.startedAt = QDateTime::currentSecsSinceEpoch();

    // The app and the background service are separate processes: never run twice.
    QLockFile lock(Platform::dataDirectory() + QStringLiteral("/sync.lock"));
    lock.setStaleLockTime(kStaleLockMs);
    if (!lock.tryLock(0)) {
        result.errorMessage = tr("Another synchronisation is already running.");
        result.finishedAt = QDateTime::currentSecsSinceEpoch();
        return result;
    }

    QSettings qsettings;
    const AppSettings settings = AppSettings::load(qsettings, Platform::deviceName());
    SyncHistory history(historyFilePath());
    history.load();

    QString password;
    const std::unique_ptr<CredentialStore> credentials = Platform::createCredentialStore();
    if (!settings.isComplete()) {
        result.errorMessage = tr("PicSync is not set up yet. Enter server, user and folder.");
    } else if (!credentials->load(settings.user, password)) {
        result.errorMessage = tr("No app password stored. Enter it in the settings.");
    } else if (!Platform::hasStorageAccess()) {
        result.errorMessage = tr("PicSync needs \"All files access\" to synchronise the gallery.");
    }
    if (!result.errorMessage.isEmpty()) {
        result.finishedAt = QDateTime::currentSecsSinceEpoch();
        history.append(result);
        Platform::showResult(result);
        return result;
    }

    const std::unique_ptr<LocalMediaStore> media = Platform::createMediaStore();
    WebDavClient remote(settings.serverUrl, settings.user, password, settings.remoteFolder);
    StateStore state(stateFilePath());
    if (!state.load())
        state.clear();   // corrupt state: start over (the first run never deletes anything)

    {
        QMutexLocker locker(&m_mutex);
        m_engine = std::make_unique<SyncEngine>(*media, remote, state);
        connect(m_engine.get(), &SyncEngine::progress, this, &SyncSession::onProgress);
        connect(m_engine.get(), &SyncEngine::phaseChanged, this, &SyncSession::phaseChanged);
    }

    const QString fingerprint = PathUtils::configFingerprint(settings.serverUrl, settings.user,
                                                            settings.remoteFolder);
    result = m_engine->run(manual, fingerprint);
    if (m_stopRequested && result.errorMessage.isEmpty())
        result.errorMessage = tr("Stopped by user");

    {
        QMutexLocker locker(&m_mutex);
        m_engine.reset();
    }

    history.append(result);
    Platform::showResult(result);
    return result;
}

} // namespace PicSync
