// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "syncengine.h"
#include "pathutils.h"
#include "syncplanner.h"

#include <QCoreApplication>
#include <QDateTime>

namespace PicSync {

namespace {
const int kMaxConsecutiveFailures = 5;
}

// Creates an engine working on the given stores (which must outlive it).
SyncEngine::SyncEngine(LocalMediaStore &local, RemoteStore &remote, StateStore &state)
    : m_local(local)
    , m_remote(remote)
    , m_state(state)
{
}

// Asks a running synchronisation to stop after the current file.
void SyncEngine::requestStop()
{
    m_stop = true;
}

// Returns true if the action moves or deletes data (used for progress).
bool SyncEngine::isWork(ActionType type)
{
    switch (type) {
    case ActionType::Upload:
    case ActionType::Download:
    case ActionType::DeleteLocal:
    case ActionType::DeleteRemote:
    case ActionType::Conflict:
        return true;
    default:
        return false;
    }
}

// Adds a line to the list of changed files (respecting the cap).
void SyncEngine::note(SyncResult &result, const QString &line)
{
    if (result.changedFiles.size() < kMaxChangedFiles)
        result.changedFiles.append(line);
}

// Performs one synchronisation run and returns its summary.
SyncResult SyncEngine::run(bool manual, const QString &configFingerprint)
{
    m_stop = false;
    SyncResult result;
    result.manual = manual;
    result.startedAt = QDateTime::currentSecsSinceEpoch();

    // A different server, user or folder means the old state is meaningless.
    if (m_state.fingerprint() != configFingerprint) {
        m_state.clear();
        m_state.setFingerprint(configFingerprint);
    }

    Q_EMIT phaseChanged(QCoreApplication::translate("SyncEngine", "Scanning gallery"));
    LocalFileMap local;
    QString error;
    if (!m_local.scan(local, error)) {
        result.errorMessage = error;
        result.finishedAt = QDateTime::currentSecsSinceEpoch();
        return result;
    }

    // Files the media index does not (yet) know, e.g. a download the media
    // scanner has not processed, must not be mistaken for deletions.
    for (auto it = m_state.entries().constBegin(); it != m_state.entries().constEnd(); ++it) {
        if (local.contains(it.key()))
            continue;
        LocalFile file;
        if (m_local.stat(it.key(), file))
            local.insert(it.key(), file);
    }

    // Losing the storage permission makes the gallery look empty. Never treat
    // that as "the user deleted everything".
    if (local.isEmpty() && !m_state.entries().isEmpty()) {
        result.errorMessage = QCoreApplication::translate("SyncEngine",
                "No gallery files are visible. Check the storage permission.");
        result.finishedAt = QDateTime::currentSecsSinceEpoch();
        return result;
    }

    Q_EMIT phaseChanged(QCoreApplication::translate("SyncEngine", "Reading Nextcloud folder"));
    RemoteFileMap remote;
    const RemoteStore::ListStatus status = m_remote.list(remote, error);
    if (status == RemoteStore::ListStatus::Error) {
        result.errorMessage = error;
        result.finishedAt = QDateTime::currentSecsSinceEpoch();
        return result;
    }
    if (status == RemoteStore::ListStatus::RootMissing) {
        if (!m_state.entries().isEmpty()) {
            result.errorMessage = QCoreApplication::translate("SyncEngine",
                    "The Nextcloud folder has disappeared. Nothing was deleted; "
                    "reset the synchronisation in the settings to start over.");
            result.finishedAt = QDateTime::currentSecsSinceEpoch();
            return result;
        }
        if (!m_remote.createRoot(error)) {
            result.errorMessage = error;
            result.finishedAt = QDateTime::currentSecsSinceEpoch();
            return result;
        }
        remote.clear();
    }

    // Nextcloud accepts names Android cannot store (e.g. "12:30.jpg"). Such
    // files are renamed on the server ("12-30.jpg"); if that fails they stay
    // on the server only. Either way this is no error.
    QStringList unstorable;
    for (auto it = remote.constBegin(); it != remote.constEnd(); ++it) {
        if (!local.contains(it.key()) && !m_state.entries().contains(it.key())
            && !PathUtils::isStorableOnPhone(it.key()))
            unstorable << it.key();
    }
    for (const QString &oldPath : unstorable) {
        const QString newPath = freePath(PathUtils::phoneSafePath(oldPath), local, remote);
        RemoteFile moved;
        QString moveError;
        if (!m_stop && PathUtils::isStorableOnPhone(newPath)
            && m_remote.move(oldPath, newPath, moved, moveError)) {
            ++result.renamedFiles;
            note(result, QStringLiteral("~ ") + oldPath + QStringLiteral(" → ")
                 + newPath.section(QLatin1Char('/'), -1));
            moved.relativePath = newPath;
            remote.insert(newPath, moved);
        } else {
            ++result.skippedFiles;
            note(result, QStringLiteral("– ") + oldPath);
        }
        remote.remove(oldPath);
    }

    const SyncPlan plan = SyncPlanner::plan(local, remote, m_state.entries(), m_state.pending());

    int total = 0;
    for (const SyncAction &action : plan) {
        if (isWork(action.type))
            ++total;
    }

    Q_EMIT phaseChanged(QCoreApplication::translate("SyncEngine", "Synchronising"));
    int done = 0;
    int consecutiveFailures = 0;
    bool aborted = false;
    for (const SyncAction &action : plan) {
        if (m_stop) {
            result.errorMessage = QCoreApplication::translate("SyncEngine", "Stopped by user");
            aborted = true;
            break;
        }
        const bool work = isWork(action.type);
        if (work)
            Q_EMIT progress(done, total, action.relativePath);

        QString actionError;
        if (execute(action, local, remote, result, actionError)) {
            consecutiveFailures = 0;
        } else {
            ++result.failedFiles;
            ++consecutiveFailures;
            result.errorMessage = actionError;
            note(result, QStringLiteral("! ") + action.relativePath + QStringLiteral(": ")
                 + actionError);
            if (consecutiveFailures >= kMaxConsecutiveFailures) {
                aborted = true;
                break;
            }
        }
        if (work) {
            ++done;
            m_state.save();
        }
    }
    m_state.save();
    if (total > 0)
        Q_EMIT progress(done, total, QString());

    QString quotaError;
    m_remote.quota(result.quota, quotaError);

    result.pendingDeletions = m_state.pending().size();
    result.success = !aborted && result.failedFiles == 0;
    result.finishedAt = QDateTime::currentSecsSinceEpoch();
    return result;
}

// Returns "path", or "name (2).ext", "name (3).ext", ... if it is already taken.
QString SyncEngine::freePath(const QString &path, const LocalFileMap &local,
                             const RemoteFileMap &remote) const
{
    const QString folder = PathUtils::parentPath(path);
    const QString name = path.section(QLatin1Char('/'), -1);
    const int dot = name.lastIndexOf(QLatin1Char('.'));
    const QString base = dot > 0 ? name.left(dot) : name;
    const QString suffix = dot > 0 ? name.mid(dot) : QString();
    QString candidate = path;
    for (int n = 2; local.contains(candidate) || remote.contains(candidate)
                    || m_state.entries().contains(candidate); ++n) {
        candidate = PathUtils::join(folder, base + QStringLiteral(" (%1)").arg(n) + suffix);
    }
    return candidate;
}

// Executes a single planned action; returns false if the file failed.
bool SyncEngine::execute(const SyncAction &action, const LocalFileMap &local,
                         const RemoteFileMap &remote, SyncResult &result, QString &error)
{
    const QString &path = action.relativePath;
    switch (action.type) {
    case ActionType::Upload:
        if (!doUpload(path, error))
            return false;
        ++result.uploaded;
        note(result, QStringLiteral("↑ ") + path);
        return true;

    case ActionType::Download:
        if (!doDownload(path, error))
            return false;
        ++result.downloaded;
        note(result, QStringLiteral("↓ ") + path);
        return true;

    case ActionType::DeleteLocal:
        if (!m_local.remove(path, error))
            return false;
        m_state.removeEntry(path);
        m_state.removePending(path);
        ++result.deletedLocal;
        note(result, QStringLiteral("× ") + path);
        return true;

    case ActionType::DeleteRemote:
        if (!m_remote.remove(path, error))
            return false;
        m_state.removeEntry(path);
        m_state.removePending(path);
        ++result.deletedRemote;
        note(result, QStringLiteral("× ") + path);
        return true;

    case ActionType::MarkLocalDeleted:
    case ActionType::MarkRemoteDeleted: {
        PendingDeletion deletion;
        deletion.relativePath = path;
        deletion.deletedOn = action.type == ActionType::MarkLocalDeleted ? Side::Local
                                                                         : Side::Remote;
        deletion.detectedAt = QDateTime::currentSecsSinceEpoch();
        m_state.setPending(deletion);
        note(result, QStringLiteral("… ") + path);
        return true;
    }

    case ActionType::CancelPending:
        m_state.removePending(path);
        return true;

    case ActionType::Conflict:
        if (!doConflict(path, error))
            return false;
        ++result.conflicts;
        note(result, QStringLiteral("! ") + path);
        return true;

    case ActionType::Adopt:
        recordState(local.value(path), remote.value(path));
        return true;

    case ActionType::Forget:
        m_state.removeEntry(path);
        m_state.removePending(path);
        return true;
    }
    return true;
}

// Uploads one local file and records the new state.
bool SyncEngine::doUpload(const QString &path, QString &error)
{
    LocalFile file;
    if (!m_local.stat(path, file)) {
        error = QCoreApplication::translate("SyncEngine", "File vanished before upload");
        return false;
    }
    RemoteFile uploaded;
    if (!m_remote.upload(path, m_local.absolutePath(path), file.mtime, uploaded, error))
        return false;
    recordState(file, uploaded);
    return true;
}

// Downloads one remote file and records the new state.
bool SyncEngine::doDownload(const QString &path, QString &error)
{
    if (!m_local.prepareWrite(path, error))
        return false;
    RemoteFile downloaded;
    if (!m_remote.download(path, m_local.absolutePath(path), downloaded, error))
        return false;
    m_local.fileWritten(path);
    LocalFile file;
    if (!m_local.stat(path, file)) {
        error = QCoreApplication::translate("SyncEngine", "Downloaded file is missing");
        return false;
    }
    recordState(file, downloaded);
    return true;
}

// Keeps both versions of a file that changed on both sides.
bool SyncEngine::doConflict(const QString &path, QString &error)
{
    // The phone's version moves aside under a conflict name and is uploaded;
    // the server's version takes the original name on both sides.
    const QString aside = PathUtils::conflictName(path, QDateTime::currentDateTime());
    if (!m_local.rename(path, aside, error))
        return false;
    m_local.fileWritten(path);
    m_local.fileWritten(aside);
    if (!doUpload(aside, error))
        return false;
    return doDownload(path, error);
}

// Records the state of a file that is identical on both sides.
void SyncEngine::recordState(const LocalFile &local, const RemoteFile &remote)
{
    StateEntry entry;
    entry.relativePath = local.relativePath.isEmpty() ? remote.relativePath : local.relativePath;
    entry.localSize = local.size;
    entry.localMtime = local.mtime;
    entry.remoteSize = remote.size;
    entry.remoteEtag = remote.etag;
    m_state.setEntry(entry);
}

} // namespace PicSync
