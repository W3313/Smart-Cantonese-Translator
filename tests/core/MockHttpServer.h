#pragma once

// Minimal HTTP/1.1 server on 127.0.0.1 for provider tests. Responses are
// served from a FIFO queue; every request is recorded.

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QList>
#include <QPair>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

#include <memory>

struct MockResponse
{
    int status = 200;
    QByteArray body;
    QList<QPair<QByteArray, QByteArray>> headers;
    bool hang = false;  // never answer (for cancel/timeout tests)

    static MockResponse json(int status, const QByteArray &body,
                             const QList<QPair<QByteArray, QByteArray>> &headers = {})
    {
        MockResponse r;
        r.status = status;
        r.body = body;
        r.headers = headers;
        return r;
    }
    static MockResponse hanging()
    {
        MockResponse r;
        r.hang = true;
        return r;
    }
};

struct RecordedRequest
{
    QByteArray method;
    QByteArray target;  // path + query
    QHash<QByteArray, QByteArray> headers;  // lower-case names
    QByteArray body;
};

class MockHttpServer
{
public:
    MockHttpServer()
    {
        m_server.listen(QHostAddress::LocalHost, 0);
        QObject::connect(&m_server, &QTcpServer::newConnection, &m_server, [this] {
            while (QTcpSocket *socket = m_server.nextPendingConnection())
                accept(socket);
        });
    }

    ~MockHttpServer()
    {
        for (const QPointer<QTcpSocket> &s : std::as_const(m_open)) {
            if (s)
                s->abort();
        }
    }

    bool isListening() const { return m_server.isListening(); }
    QUrl url() const { return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort())); }
    void enqueue(const MockResponse &r) { m_queue.append(r); }
    const QList<RecordedRequest> &requests() const { return m_requests; }

private:
    void accept(QTcpSocket *socket)
    {
        m_open.append(socket);
        auto buffer = std::make_shared<QByteArray>();
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer] {
            buffer->append(socket->readAll());
            const qsizetype headerEnd = buffer->indexOf("\r\n\r\n");
            if (headerEnd < 0)
                return;
            RecordedRequest req;
            const QList<QByteArray> lines = buffer->left(headerEnd).split('\n');
            const QList<QByteArray> requestLine = lines.value(0).trimmed().split(' ');
            req.method = requestLine.value(0);
            req.target = requestLine.value(1);
            qsizetype contentLength = 0;
            for (qsizetype i = 1; i < lines.size(); ++i) {
                const QByteArray line = lines.at(i).trimmed();
                const qsizetype colon = line.indexOf(':');
                if (colon <= 0)
                    continue;
                const QByteArray name = line.left(colon).trimmed().toLower();
                const QByteArray value = line.mid(colon + 1).trimmed();
                req.headers.insert(name, value);
                if (name == "content-length")
                    contentLength = value.toLongLong();
            }
            const qsizetype bodyStart = headerEnd + 4;
            if (buffer->size() - bodyStart < contentLength)
                return;  // wait for the rest of the body
            req.body = buffer->mid(bodyStart, contentLength);
            buffer->clear();
            m_requests.append(req);
            respond(socket);
        });
    }

    void respond(QTcpSocket *socket)
    {
        MockResponse r = m_queue.isEmpty()
                             ? MockResponse::json(500, R"({"error":{"message":"mock queue empty"}})")
                             : m_queue.takeFirst();
        if (r.hang)
            return;
        QByteArray out = "HTTP/1.1 " + QByteArray::number(r.status) + " Mock\r\n";
        out += "Content-Type: application/json\r\n";
        out += "Content-Length: " + QByteArray::number(r.body.size()) + "\r\n";
        out += "Connection: close\r\n";
        for (const auto &h : std::as_const(r.headers))
            out += h.first + ": " + h.second + "\r\n";
        out += "\r\n";
        out += r.body;
        socket->write(out);
        socket->flush();
        socket->disconnectFromHost();
    }

    QTcpServer m_server;
    QList<MockResponse> m_queue;
    QList<RecordedRequest> m_requests;
    QList<QPointer<QTcpSocket>> m_open;
};
