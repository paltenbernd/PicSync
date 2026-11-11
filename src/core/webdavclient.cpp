// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "webdavclient.h"
#include "pathutils.h"
#include "webdavparser.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QQueue>

namespace PicSync {

namespace {

// Removes trailing slashes, so folder paths can be compared.
QString withoutTrailingSlash(QString path)
{
    while (path.size() > 1 && path.endsWith(QLatin1Char('/')))
        path.chop(1);
    return path;
}

// Returns true for HTTP status codes that mean success.
bool isSuccess(int status)
{
    return status >= 200 && status < 300;
}

} // namespace

// Connects to the reply; data is written to the (already open) file.
ReplyFileWriter::ReplyFileWriter(QNetworkReply &reply, QFile &file)
    : m_reply(reply)
    , m_file(file)
{
    connect(&m_reply, &QNetworkReply::readyRead, this, &ReplyFileWriter::drain);
}

// Returns true if writing to the file failed at some point.
bool ReplyFileWriter::failed() const
{
    return m_failed;
}

// Writes all data currently buffered in the reply to the file.
void ReplyFileWriter::drain()
{
    const QByteArray data = m_reply.readAll();
    if (!data.isEmpty() && m_file.write(data) != data.size())
        m_failed = true;
}

// Creates a client for the given server, login and device folder.
WebDavClient::WebDavClient(const QString &serverUrl, const QString &user,
                           const QString &password, const QString &remoteFolder)
    : m_nam(std::make_unique<QNetworkAccessManager>())
    , m_filesRoot(PathUtils::davFilesRoot(serverUrl, user))
    , m_remoteFolder(PathUtils::normalizeRelative(remoteFolder))
{
    const QByteArray credentials = (user + QLatin1Char(':') + password).toUtf8();
    m_authorization = QByteArrayLiteral("Basic ") + credentials.toBase64();
    m_nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

// Destroys the client and its network manager.
WebDavClient::~WebDavClient() = default;

// Sets the inactivity timeout for network transfers in milliseconds.
void WebDavClient::setTimeout(int milliseconds)
{
    m_timeoutMs = milliseconds;
}

// Builds a request with authentication for a path below the files root.
QNetworkRequest WebDavClient::makeRequest(const QString &pathBelowFilesRoot) const
{
    QNetworkRequest request(PathUtils::davUrl(m_filesRoot, pathBelowFilesRoot));
    request.setRawHeader("Authorization", m_authorization);
    request.setRawHeader("User-Agent", "PicSync/" PICSYNC_VERSION " (Android)");
    request.setTransferTimeout(m_timeoutMs);
    return request;
}

// Blocks until the reply has finished.
void WebDavClient::waitFor(QNetworkReply &reply)
{
    if (reply.isFinished())
        return;
    QEventLoop loop;
    QObject::connect(&reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
}

// Copies status, headers and error information from a finished reply.
void WebDavClient::collect(QNetworkReply &reply, HttpResult &result)
{
    result.status = reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.error = reply.error();
    result.errorString = reply.errorString();
    result.etag = reply.rawHeader("OC-ETag");
    if (result.etag.isEmpty())
        result.etag = reply.rawHeader("ETag");
    result.lastModified = reply.rawHeader("Last-Modified");
}

// Sends a request with an in-memory body and waits for the answer.
WebDavClient::HttpResult WebDavClient::send(QNetworkRequest request, const QByteArray &verb,
                                            const QByteArray &body)
{
    if (!body.isEmpty())
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QByteArrayLiteral("application/xml; charset=utf-8"));
    std::unique_ptr<QNetworkReply> reply(m_nam->sendCustomRequest(request, verb, body));
    waitFor(*reply);
    HttpResult result;
    collect(*reply, result);
    result.body = reply->readAll();
    return result;
}

// Turns a failed request into a readable error message.
QString WebDavClient::describe(const HttpResult &result)
{
    switch (result.status) {
    case 401:
        return QCoreApplication::translate("WebDavClient", "Login failed. Check user name and app password.");
    case 403:
        return QCoreApplication::translate("WebDavClient", "Access denied by the server.");
    case 404:
        return QCoreApplication::translate("WebDavClient", "Folder or file not found on the server.");
    case 507:
        return QCoreApplication::translate("WebDavClient", "Not enough storage space on the server.");
    default:
        break;
    }
    if (result.status == 0 || result.error != QNetworkReply::NoError) {
        if (result.error == QNetworkReply::HostNotFoundError)
            return QCoreApplication::translate("WebDavClient", "Server not found. Check the address and your connection.");
        if (result.error == QNetworkReply::OperationCanceledError
                || result.error == QNetworkReply::TimeoutError)
            return QCoreApplication::translate("WebDavClient", "The server did not answer in time.");
        if (result.error == QNetworkReply::SslHandshakeFailedError)
            return QCoreApplication::translate("WebDavClient", "Secure connection failed (certificate problem).");
        if (result.status == 0)
            return result.errorString;
    }
    return QCoreApplication::translate("WebDavClient", "Server error %1").arg(result.status);
}

// Lists all media files below the device folder (recursively).
RemoteStore::ListStatus WebDavClient::list(RemoteFileMap &files, QString &error)
{
    files.clear();
    const QString rootPath = withoutTrailingSlash(QUrl::fromPercentEncoding(
            PathUtils::davUrl(m_filesRoot, m_remoteFolder).path(QUrl::FullyEncoded).toUtf8()));

    QQueue<QString> folders;
    folders.enqueue(QString());
    while (!folders.isEmpty()) {
        const QString folder = folders.dequeue();
        QNetworkRequest request = makeRequest(PathUtils::join(m_remoteFolder, folder)
                                              + QLatin1Char('/'));
        request.setRawHeader("Depth", "1");
        const HttpResult result = send(request, "PROPFIND", WebDavParser::propfindBody());

        if (result.status == 404 && folder.isEmpty())
            return ListStatus::RootMissing;
        if (result.status != 207) {
            error = describe(result);
            return ListStatus::Error;
        }

        QList<DavEntry> entries;
        if (!WebDavParser::parseMultiStatus(result.body, entries, error))
            return ListStatus::Error;

        if (folder.isEmpty())
            m_knownFolders.insert(QString());

        for (const DavEntry &entry : std::as_const(entries)) {
            const QString path = withoutTrailingSlash(entry.path);
            if (!path.startsWith(rootPath))
                continue;
            const QString rel = PathUtils::normalizeRelative(path.mid(rootPath.size()));
            if (rel.isEmpty() || rel == folder)
                continue;   // the folder itself
            if (PathUtils::isIgnored(rel))
                continue;
            if (entry.isCollection) {
                m_knownFolders.insert(rel);
                folders.enqueue(rel);
            } else if (PathUtils::isMediaFile(rel) && entry.found) {
                RemoteFile file;
                file.relativePath = rel;
                file.size = entry.size;
                file.mtime = entry.mtime;
                file.etag = entry.etag;
                files.insert(rel, file);
            }
        }
    }
    return ListStatus::Ok;
}

// Creates the device folder (and all parents) on the server.
bool WebDavClient::createRoot(QString &error)
{
    const QStringList parts = m_remoteFolder.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QString current;
    for (const QString &part : parts) {
        current = PathUtils::join(current, part);
        const HttpResult result = send(makeRequest(current + QLatin1Char('/')), "MKCOL");
        // 201 = created, 405 = already exists.
        if (result.status != 201 && result.status != 405) {
            error = describe(result);
            return false;
        }
    }
    m_knownFolders.insert(QString());
    return true;
}

// Creates a folder below the device folder and all of its parents.
bool WebDavClient::ensureFolder(const QString &relativeFolder, QString &error)
{
    const QString folder = PathUtils::normalizeRelative(relativeFolder);
    if (folder.isEmpty() || m_knownFolders.contains(folder))
        return true;
    if (!ensureFolder(PathUtils::parentPath(folder), error))
        return false;
    const HttpResult result = send(makeRequest(PathUtils::join(m_remoteFolder, folder)
                                               + QLatin1Char('/')), "MKCOL");
    if (result.status != 201 && result.status != 405) {
        error = describe(result);
        return false;
    }
    m_knownFolders.insert(folder);
    return true;
}

// Asks the server for the properties of one file below the device folder.
bool WebDavClient::statRemote(const QString &relativePath, RemoteFile &file, QString &error)
{
    QNetworkRequest request = makeRequest(PathUtils::join(m_remoteFolder, relativePath));
    request.setRawHeader("Depth", "0");
    const HttpResult result = send(request, "PROPFIND", WebDavParser::propfindBody());
    if (result.status != 207) {
        error = describe(result);
        return false;
    }
    QList<DavEntry> entries;
    if (!WebDavParser::parseMultiStatus(result.body, entries, error) || entries.isEmpty())
        return false;
    file.relativePath = PathUtils::normalizeRelative(relativePath);
    file.size = entries.first().size;
    file.mtime = entries.first().mtime;
    file.etag = entries.first().etag;
    return true;
}

// Uploads a local file; on success "result" describes the new remote file.
bool WebDavClient::upload(const QString &relativePath, const QString &localPath,
                          qint64 mtime, RemoteFile &result, QString &error)
{
    if (!ensureFolder(PathUtils::parentPath(relativePath), error))
        return false;

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QCoreApplication::translate("WebDavClient", "Cannot read %1: %2").arg(localPath, file.errorString());
        return false;
    }

    QNetworkRequest request = makeRequest(PathUtils::join(m_remoteFolder, relativePath));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QByteArrayLiteral("application/octet-stream"));
    request.setHeader(QNetworkRequest::ContentLengthHeader, file.size());
    request.setRawHeader("X-OC-MTime", QByteArray::number(mtime));

    std::unique_ptr<QNetworkReply> reply(m_nam->put(request, &file));
    waitFor(*reply);
    HttpResult http;
    collect(*reply, http);
    reply->readAll();

    if (!isSuccess(http.status)) {
        error = describe(http);
        return false;
    }

    result.relativePath = PathUtils::normalizeRelative(relativePath);
    result.size = file.size();
    result.mtime = mtime;
    result.etag = PathUtils::normalizeEtag(QString::fromLatin1(http.etag));
    if (result.etag.isEmpty()) {
        RemoteFile fresh;
        if (statRemote(relativePath, fresh, error))
            result = fresh;
    }
    return true;
}

// Downloads a remote file to the given local path.
bool WebDavClient::download(const QString &relativePath, const QString &localPath,
                            RemoteFile &fileInfo, QString &error)
{
    const QString partPath = localPath + QStringLiteral(".picsync-part");
    QFile part(partPath);
    if (!part.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        error = QCoreApplication::translate("WebDavClient", "Cannot write %1: %2").arg(partPath, part.errorString());
        return false;
    }

    const QNetworkRequest request = makeRequest(PathUtils::join(m_remoteFolder, relativePath));
    std::unique_ptr<QNetworkReply> reply(m_nam->get(request));
    ReplyFileWriter writer(*reply, part);
    waitFor(*reply);
    writer.drain();

    HttpResult http;
    collect(*reply, http);
    if (http.status != 200 || writer.failed()) {
        error = writer.failed() ? QCoreApplication::translate("WebDavClient", "Cannot write %1: %2").arg(partPath, part.errorString())
                                : describe(http);
        part.close();
        part.remove();
        return false;
    }

    fileInfo.relativePath = PathUtils::normalizeRelative(relativePath);
    fileInfo.size = part.size();
    fileInfo.etag = PathUtils::normalizeEtag(QString::fromLatin1(http.etag));
    fileInfo.mtime = WebDavParser::parseHttpDate(QString::fromLatin1(http.lastModified));
    if (fileInfo.mtime > 0)
        part.setFileTime(QDateTime::fromSecsSinceEpoch(fileInfo.mtime),
                         QFileDevice::FileModificationTime);
    part.close();

    if (QFileInfo::exists(localPath) && !QFile::remove(localPath)) {
        error = QCoreApplication::translate("WebDavClient", "Cannot replace %1").arg(localPath);
        QFile::remove(partPath);
        return false;
    }
    if (!QFile::rename(partPath, localPath)) {
        error = QCoreApplication::translate("WebDavClient", "Cannot rename %1").arg(partPath);
        QFile::remove(partPath);
        return false;
    }

    if (fileInfo.etag.isEmpty()) {
        RemoteFile fresh;
        QString ignored;
        if (statRemote(relativePath, fresh, ignored))
            fileInfo.etag = fresh.etag;
    }
    return true;
}

// Deletes a remote file (it goes to the Nextcloud trash bin).
bool WebDavClient::remove(const QString &relativePath, QString &error)
{
    const HttpResult result = send(makeRequest(PathUtils::join(m_remoteFolder, relativePath)),
                                   "DELETE");
    if (isSuccess(result.status) || result.status == 404)
        return true;
    error = describe(result);
    return false;
}

// Queries the storage quota of the account.
bool WebDavClient::quota(QuotaInfo &info, QString &error)
{
    QNetworkRequest request = makeRequest(QString());
    request.setRawHeader("Depth", "0");
    const HttpResult result = send(request, "PROPFIND", WebDavParser::quotaBody());
    if (result.status != 207) {
        error = describe(result);
        return false;
    }
    QList<DavEntry> entries;
    if (!WebDavParser::parseMultiStatus(result.body, entries, error) || entries.isEmpty())
        return false;
    info.valid = true;
    info.usedBytes = qMax<qint64>(0, entries.first().quotaUsed);
    info.availableBytes = entries.first().quotaAvailable < 0 ? -1
                                                              : entries.first().quotaAvailable;
    return true;
}

// Checks that server address and credentials are valid.
bool WebDavClient::testConnection(QString &error)
{
    QNetworkRequest request = makeRequest(QString());
    request.setRawHeader("Depth", "0");
    const HttpResult result = send(request, "PROPFIND", WebDavParser::propfindBody());
    if (result.status == 207)
        return true;
    error = describe(result);
    return false;
}

} // namespace PicSync
