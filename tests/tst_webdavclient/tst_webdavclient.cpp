// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "webdavclient.h"

#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

using namespace PicSync;

/// Minimal HTTP server imitating MagentaCLOUD: the modern Nextcloud address
/// /remote.php/dav/files/<user>/ is refused, only /remote.php/webdav/ works.
class MockDavServer : public QObject
{
    Q_OBJECT
public:
    // Starts listening on a free local port.
    MockDavServer()
    {
        connect(&m_server, &QTcpServer::newConnection, this, &MockDavServer::onConnection);
        m_server.listen(QHostAddress::LocalHost);
    }

    // Returns the base address of the server.
    QString url() const
    {
        return QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort());
    }

    // Requests received so far ("METHOD path").
    QStringList requests;

    // Number of requests that carried a cookie.
    int requestsWithCookie = 0;

private Q_SLOTS:
    // Accepts a new client connection.
    void onConnection()
    {
        const QPointer<QTcpSocket> socket(m_server.nextPendingConnection());
        if (socket)
            connect(socket.data(), &QTcpSocket::readyRead, this, &MockDavServer::onReadyRead);
    }

    // Answers complete requests.
    void onReadyRead()
    {
        const QPointer<QTcpSocket> socket(qobject_cast<QTcpSocket *>(sender()));
        if (!socket)
            return;
        m_buffer += socket->readAll();
        const int headerEnd = m_buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return;
        const QByteArray header = m_buffer.left(headerEnd);
        int length = 0;
        for (const QByteArray &line : header.split('\n')) {
            if (line.toLower().startsWith("content-length:"))
                length = line.mid(15).trimmed().toInt();
        }
        if (m_buffer.size() < headerEnd + 4 + length)
            return;
        m_buffer.remove(0, headerEnd + 4 + length);

        const QList<QByteArray> requestLine = header.left(header.indexOf('\r')).split(' ');
        const QByteArray method = requestLine.value(0);
        const QByteArray path = requestLine.value(1);
        requests << QString::fromLatin1(method + ' ' + path);

        QByteArray status = "404 Not Found";
        QByteArray body;
        // Like an expired Nextcloud session: a request that sends the session
        // cookie back is refused, even with a valid password.
        if (header.toLower().contains("\r\ncookie:")) {
            requestsWithCookie += 1;
            socket->write("HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\n\r\n");
            socket->flush();
            return;
        }
        if (path.startsWith("/remote.php/dav/")) {
            status = "401 Unauthorized";
        } else if (method == "PROPFIND" && path == "/remote.php/webdav/") {
            status = "207 Multi-Status";
            body = multistatus("/remote.php/webdav/", true);
        } else if (method == "PROPFIND" && path == "/remote.php/webdav/PicSync/FP6/") {
            status = "207 Multi-Status";
            body = multistatus("/remote.php/webdav/PicSync/FP6/", true)
                       .replace("</d:multistatus>", "")
                   + multistatus("/remote.php/webdav/PicSync/FP6/a.jpg", false)
                         .replace("<d:multistatus xmlns:d=\"DAV:\">", "");
        }
        socket->write("HTTP/1.1 " + status + "\r\nContent-Type: application/xml\r\n"
                      "Set-Cookie: nc_session_id=abc123; path=/; HttpOnly\r\n"
                      "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
        socket->flush();
    }

private:
    // Builds a multistatus answer for one folder or file.
    static QByteArray multistatus(const QByteArray &href, bool folder)
    {
        return "<d:multistatus xmlns:d=\"DAV:\"><d:response><d:href>" + href
               + "</d:href><d:propstat><d:prop>"
               + (folder ? "<d:resourcetype><d:collection/></d:resourcetype>"
                         : "<d:resourcetype/><d:getcontentlength>5</d:getcontentlength>"
                           "<d:getetag>\"e1\"</d:getetag>")
               + "</d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>"
                 "</d:multistatus>";
    }

    QTcpServer m_server;
    QByteArray m_buffer;
};

/// Tests of the WebDAV client against a mock server.
class TestWebDavClient : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // Falls back to /remote.php/webdav/ when the modern address is refused.
    void fallsBackToLegacyAddress();
    // Reports a readable error when the server cannot be reached.
    void unreachableServer();
};

// Falls back to /remote.php/webdav/ when the modern address is refused.
void TestWebDavClient::fallsBackToLegacyAddress()
{
    MockDavServer server;
    WebDavClient client(server.url() + QStringLiteral("/remote.php/webdav"),
                        QStringLiteral("120049"), QStringLiteral("secret"),
                        QStringLiteral("PicSync/FP6"));
    QString error;
    QVERIFY2(client.testConnection(error), qPrintable(error));
    QVERIFY(client.filesRoot().toString().endsWith(QLatin1String("/remote.php/webdav/")));

    RemoteFileMap files;
    const RemoteStore::ListStatus status = client.list(files, error);
    QVERIFY2(status == RemoteStore::ListStatus::Ok, qPrintable(server.requests.join(QStringLiteral(", "))));
    QCOMPARE(files.size(), 1);
    QCOMPARE(files.value(QStringLiteral("a.jpg")).etag, QStringLiteral("e1"));
    QVERIFY(server.requests.contains(QStringLiteral("PROPFIND /remote.php/dav/files/120049/")));

    // The server set a session cookie on every answer; it must never come back.
    QCOMPARE(client.list(files, error), RemoteStore::ListStatus::Ok);
    QCOMPARE(server.requestsWithCookie, 0);
}

// Reports a readable error when the server cannot be reached.
void TestWebDavClient::unreachableServer()
{
    WebDavClient client(QStringLiteral("http://127.0.0.1:1"), QStringLiteral("u"),
                        QStringLiteral("p"), QStringLiteral("PicSync/FP6"));
    client.setTimeout(3000);
    QString error;
    QVERIFY(!client.testConnection(error));
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(TestWebDavClient)
#include "tst_webdavclient.moc"
