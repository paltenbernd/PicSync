// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include "syncmodel.h"

#include <QList>
#include <QString>

namespace PicSync {

/// Log of the most recent synchronisation runs, shown on the history page.
class SyncHistory
{
public:
    // Creates a history backed by the given JSON file.
    explicit SyncHistory(const QString &filePath, int maxEntries = 100);

    // Loads the stored runs (newest first). A missing file yields an empty list.
    bool load();

    // Prepends a run and writes the file, dropping the oldest entries.
    bool append(const SyncResult &result);

    // Returns the stored runs, newest first.
    const QList<SyncResult> &entries() const;

    // Removes all stored runs.
    bool clear();

    // Converts a result to JSON bytes (one entry).
    static QByteArray toJson(const SyncResult &result);

    // Reads a result from JSON bytes (one entry).
    static SyncResult fromJson(const QByteArray &json);

private:
    // Writes all entries atomically.
    bool save() const;

    QString m_filePath;
    int m_maxEntries;
    QList<SyncResult> m_entries;
};

} // namespace PicSync
