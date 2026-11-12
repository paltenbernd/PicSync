// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include "interfaces.h"

#include <QByteArray>
#include <QNetworkReply>
#include <QObject>
#include <QSet>
#include <QUrl>

#include <memory>

class QFile;
class QNetworkAccessManager;
class QNetworkRequest;

namespace PicSync {

/// Copies the data of a running download into a file as it arrives.
class ReplyFileWriter : public QObject
{
    Q_OBJECT
public:
    // Connects to the reply; data is written to the (already open) file.
    ReplyFileWriter(QNetworkReply &reply, QFile &file);

    // Returns true if writing to the file failed at some point.
    bool failed() const;

public Q_SLOTS:
    // Writes all data currently buffered in the reply to the file.
    void drain();

private:
    QNetworkReply &m_reply;
    QFile &m_file;
    bool m_failed = false;
};

/// RemoteStore talking to Nextcloud through its WebDAV interface.
///
/// All calls are blocking (they run a local event loop) and must be made from
/// the thread that created the client. The sync engine runs in a worker thread,
/// so the user interface stays responsive.
class WebDavClient : public RemoteStore
{
public:
    /// Raw result of one HTTP request.
    struct HttpResult
    {
        int status = 0;
        QNetworkReply::NetworkError error = QNetworkReply::NoError;
        QString errorString;
        QByteArray body;
        QByteArray etag;
        QByteArray lastModified;
    };

    // Creates a client for the given server, login and device folder.
    WebDavClient(const QString &serverUrl, const QString &user, const QString &password,
                 const QString &remoteFolder);

    // Destroys the client and its network manager.
    ~WebDavClient() override;

    // Lists all media files below the device folder (recursively).
    ListStatus list(RemoteFileMap &files, QString &error) override;

    // Creates the device folder (and all parents) on the server.
    bool createRoot(QString &error) override;

    // Uploads a local file; on success "result" describes the new remote file.
    bool upload(const QString &relativePath, const QString &localPath, qint64 mtime,
                RemoteFile &result, QString &error) override;

    // Downloads a remote file to the given local path.
    bool download(const QString &relativePath, const QString &localPath,
                  RemoteFile &file, QString &error) override;

    // Deletes a remote file (it goes to the Nextcloud trash bin).
    bool remove(const QString &relativePath, QString &error) override;

    // Queries the storage quota of the account.
    bool quota(QuotaInfo &info, QString &error) override;

    // Checks that server address and credentials are valid.
    bool testConnection(QString &error) override;

    // Finds out which WebDAV address the server accepts (done once, automatically).
    bool selectEndpoint(QString &error);

    // Returns the WebDAV root that is used, e.g. for display.
    QUrl filesRoot() const;

    // Sets the inactivity timeout for network transfers in milliseconds.
    void setTimeout(int milliseconds);

private:
    // Builds a request with authentication for a path below the files root.
    QNetworkRequest makeRequest(const QString &pathBelowFilesRoot) const;

    // Sends a request with an in-memory body and waits for the answer.
    HttpResult send(QNetworkRequest request, const QByteArray &verb,
                    const QByteArray &body = QByteArray());

    // Copies status, headers and error information from a finished reply.
    static void collect(QNetworkReply &reply, HttpResult &result);

    // Blocks until the reply has finished.
    static void waitFor(QNetworkReply &reply);

    // Turns a failed request into a readable error message.
    static QString describe(const HttpResult &result);

    // Creates a folder below the device folder and all of its parents.
    bool ensureFolder(const QString &relativeFolder, QString &error);

    // Sends a depth-0 PROPFIND to a WebDAV root and returns the result.
    HttpResult probe(const QUrl &root);

    // Asks the server for the properties of one file below the device folder.
    bool statRemote(const QString &relativePath, RemoteFile &file, QString &error);

    std::unique_ptr<QNetworkAccessManager> m_nam;
    QUrl m_filesRoot;
    QUrl m_legacyRoot;
    bool m_endpointSelected = false;
    QString m_remoteFolder;
    QByteArray m_authorization;
    int m_timeoutMs = 60000;
    QSet<QString> m_knownFolders;
};

} // namespace PicSync
