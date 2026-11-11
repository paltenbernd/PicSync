// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include "syncmodel.h"

#include <QString>

namespace PicSync {

/// Persistent record of the last synchronised state and of pending deletions.
///
/// The data is kept as JSON in the app's private data directory and written
/// atomically, so an interrupted sync never leaves a corrupt state behind.
class StateStore
{
public:
    // Creates a store backed by the given JSON file (nothing is read yet).
    explicit StateStore(const QString &filePath);

    // Loads the file; a missing file yields an empty state. Returns false on corruption.
    bool load();

    // Writes the current content atomically. Returns false on I/O errors.
    bool save() const;

    // Removes all entries (used when the server or folder configuration changes).
    void clear();

    // Returns the path of the backing file.
    QString filePath() const;

    // Returns all recorded file states.
    const StateMap &entries() const;

    // Returns all pending (delayed) deletions.
    const PendingMap &pending() const;

    // Inserts or replaces the recorded state of one file.
    void setEntry(const StateEntry &entry);

    // Removes the recorded state of one file.
    void removeEntry(const QString &relativePath);

    // Records a delayed deletion, which locks the file on the other side.
    void setPending(const PendingDeletion &deletion);

    // Removes a delayed deletion.
    void removePending(const QString &relativePath);

    // Returns the configuration fingerprint the state belongs to.
    QString fingerprint() const;

    // Sets the configuration fingerprint (server, user and folder).
    void setFingerprint(const QString &fingerprint);

    // Serialises the content to JSON bytes.
    QByteArray toJson() const;

    // Replaces the content from JSON bytes. Returns false if the data is invalid.
    bool fromJson(const QByteArray &json);

private:
    QString m_filePath;
    QString m_fingerprint;
    StateMap m_entries;
    PendingMap m_pending;
};

} // namespace PicSync
