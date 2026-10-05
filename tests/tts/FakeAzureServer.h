#pragma once

// Minimal local HTTP server standing in for the Azure TTS endpoint in tests.
// Records each request and answers with a configurable status and body.

#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QMap>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>

#include <memory>

struct FakeRequest
{
    QByteArray method;
    QByteArray path;
    QMap<QByteArray, QByteArray> headers;  // lower-case names
    QByteArray body;
};

class FakeAzureServer : public QTcpServer
{
public:
    int status = 200;
    QByteArray contentType = "audio/mpeg";
    QByteArray body = silentMp3();
    bool hang = false;  // accept the request but never answer
    QList<FakeRequest> requests;

    FakeAzureServer()
    {
        listen(QHostAddress::LocalHost, 0);
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = nextPendingConnection())
                handle(socket);
        });
    }

    // ~0.25 s of valid, silent MPEG-1 Layer III audio (128 kbit/s, 44.1 kHz,
    // mono): frame header FF FB 90 C4 followed by zeroed side info / data.
    static QByteArray silentMp3()
    {
        QByteArray frame(417, '\0');
        frame[0] = '\xFF';
        frame[1] = '\xFB';
        frame[2] = '\x90';
        frame[3] = '\xC4';
        return frame.repeated(10);
    }

    QUrl url() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/cognitiveservices/v1").arg(serverPort()));
    }

    // A URL on which nothing listens (connection refused).
    static QUrl deadUrl()
    {
        QTcpServer probe;
        probe.listen(QHostAddress::LocalHost, 0);
        const quint16 port = probe.serverPort();
        probe.close();
        return QUrl(QStringLiteral("http://127.0.0.1:%1/cognitiveservices/v1").arg(port));
    }

private:
    void handle(QTcpSocket *socket)
    {
        auto buffer = std::make_shared<QByteArray>();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer] {
            buffer->append(socket->readAll());
            const qsizetype headerEnd = buffer->indexOf("\r\n\r\n");
            if (headerEnd < 0)
                return;
            FakeRequest request;
            const QList<QByteArray> lines = buffer->left(headerEnd).split('\n');
            const QList<QByteArray> requestLine = lines.value(0).trimmed().split(' ');
            request.method = requestLine.value(0);
            request.path = requestLine.value(1);
            for (qsizetype i = 1; i < lines.size(); ++i) {
                const QByteArray line = lines.at(i).trimmed();
                const qsizetype colon = line.indexOf(':');
                if (colon > 0)
                    request.headers.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
            }
            const qsizetype length = request.headers.value("content-length", "0").toLongLong();
            if (buffer->size() < headerEnd + 4 + length)
                return;  // wait for the rest of the body
            request.body = buffer->mid(headerEnd + 4, length);
            buffer->clear();
            requests.append(request);
            if (hang)
                return;
            QByteArray response = "HTTP/1.1 " + QByteArray::number(status) + " Status\r\n";
            response += "Content-Type: " + contentType + "\r\n";
            response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
            response += "Connection: close\r\n\r\n";
            response += body;
            socket->write(response);
            socket->disconnectFromHost();
        });
    }
};
