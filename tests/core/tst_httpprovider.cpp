// End-to-end tests of the shared HTTP plumbing (retries, Retry-After, timeout,
// cancel, error mapping, fallbacks) against a local mock server. No real
// network access.

#include "MockHttpServer.h"

#include "core/ClaudeProvider.h"
#include "core/OpenAIProvider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QtTest>

using namespace sct;

namespace {

// Built with QJsonObject: moc (Qt 6.4) mis-parses raw strings containing \".
QByteArray claudeOk()
{
    QJsonObject text;
    text.insert(QStringLiteral("type"), QStringLiteral("text"));
    text.insert(QStringLiteral("text"),
                QStringLiteral(R"({"translation":"佢喺邊度呀？","jyutping":"keoi5 hai2 bin1 dou6 aa3?","literal":"","alternatives":[],"notes":[]})"));
    QJsonObject root;
    root.insert(QStringLiteral("type"), QStringLiteral("message"));
    root.insert(QStringLiteral("role"), QStringLiteral("assistant"));
    root.insert(QStringLiteral("model"), QStringLiteral("claude-opus-5-5"));
    root.insert(QStringLiteral("content"), QJsonArray{text});
    root.insert(QStringLiteral("stop_reason"), QStringLiteral("end_turn"));
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray openAiOk()
{
    QJsonObject message;
    message.insert(QStringLiteral("role"), QStringLiteral("assistant"));
    message.insert(QStringLiteral("content"),
                   QStringLiteral(R"({"translation":"唔該晒","jyutping":"m4 goi1 saai3","literal":"","alternatives":[],"notes":[]})"));
    message.insert(QStringLiteral("refusal"), QJsonValue::Null);
    QJsonObject choice;
    choice.insert(QStringLiteral("index"), 0);
    choice.insert(QStringLiteral("message"), message);
    choice.insert(QStringLiteral("finish_reason"), QStringLiteral("stop"));
    QJsonObject root;
    root.insert(QStringLiteral("object"), QStringLiteral("chat.completion"));
    root.insert(QStringLiteral("model"), QStringLiteral("gpt-6.1-sol"));
    root.insert(QStringLiteral("choices"), QJsonArray{choice});
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

const QByteArray kClaudeOk = claudeOk();
const QByteArray kOpenAiOk = openAiOk();

TranslationRequest sampleRequest(const QString &text = QStringLiteral("Where is he?"))
{
    TranslationRequest r;
    r.text = text;
    return r;
}

} // namespace

class TstHttpProvider : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<MockHttpServer> m_server;
    std::unique_ptr<QNetworkAccessManager> m_nam;

    template <typename P>
    std::unique_ptr<P> makeProvider()
    {
        auto p = std::make_unique<P>(m_nam.get());
        p->setBaseUrl(m_server->url());
        p->setApiKey(QStringLiteral("test-key"));
        p->setRetryDelays({10, 30});
        p->setTransferTimeout(5000);
        return p;
    }

private slots:
    void initTestCase()
    {
        registerMetaTypes();
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    }

    void init()
    {
        m_server = std::make_unique<MockHttpServer>();
        QVERIFY(m_server->isListening());
        m_nam = std::make_unique<QNetworkAccessManager>();
    }

    void cleanup()
    {
        m_nam.reset();
        m_server.reset();
    }

    void claudeSuccess()
    {
        m_server->enqueue(MockResponse::json(200, kClaudeOk));
        auto p = makeProvider<ClaudeProvider>();
        p->setQuality(QStringLiteral("best"));
        QSignalSpy finished(p.get(), &TranslationProvider::finished);
        QSignalSpy failed(p.get(), &TranslationProvider::failed);

        p->translate(sampleRequest());
        QVERIFY(p->isBusy());
        QCOMPARE(finished.count(), 0);  // never synchronous
        QVERIFY(finished.wait(5000));
        QCOMPARE(failed.count(), 0);
        QVERIFY(!p->isBusy());

        const TranslationResult r = finished.first().first().value<TranslationResult>();
        QCOMPARE(r.translation, QStringLiteral("佢喺邊度呀？"));
        QCOMPARE(r.providerId, QStringLiteral("claude"));
        QCOMPARE(r.model, QStringLiteral("claude-opus-5-5"));
        QCOMPARE(r.request.text, QStringLiteral("Where is he?"));
        QVERIFY(r.timestamp.isValid());
        QVERIFY(!r.fromCache);

        QCOMPARE(m_server->requests().size(), 1);
        const RecordedRequest &req = m_server->requests().first();
        QCOMPARE(req.method, QByteArray("POST"));
        QCOMPARE(req.target, QByteArray("/v1/messages"));
        QCOMPARE(req.headers.value("x-api-key"), QByteArray("test-key"));
        QCOMPARE(req.headers.value("anthropic-version"), QByteArray("2023-06-01"));
        QCOMPARE(req.headers.value("anthropic-beta"), QByteArray("server-side-fallback-2026-07-01"));
        QVERIFY(req.headers.value("content-type").startsWith("application/json"));
        const QJsonObject body = QJsonDocument::fromJson(req.body).object();
        QCOMPARE(body.value(QStringLiteral("output_config")).toObject().value(QStringLiteral("effort")).toString(),
                 QStringLiteral("high"));
    }

    void retriesTransientErrors()
    {
        m_server->enqueue(MockResponse::json(529, R"({"type":"error","error":{"type":"overloaded_error","message":"Overloaded"}})"));
        m_server->enqueue(MockResponse::json(503, R"({"type":"error","error":{"type":"api_error","message":"busy"}})"));
        m_server->enqueue(MockResponse::json(200, kClaudeOk));
        auto p = makeProvider<ClaudeProvider>();
        QSignalSpy finished(p.get(), &TranslationProvider::finished);
        QSignalSpy failed(p.get(), &TranslationProvider::failed);
        p->translate(sampleRequest());
        QVERIFY(finished.wait(5000));
        QCOMPARE(failed.count(), 0);
        QCOMPARE(m_server->requests().size(), 3);
    }

    void givesUpAfterMaxRetries()
    {
        for (int i = 0; i < 3; ++i)
            m_server->enqueue(MockResponse::json(529, R"({"type":"error","error":{"type":"overloaded_error","message":"Overloaded"}})"));
        auto p = makeProvider<ClaudeProvider>();
        QSignalSpy failed(p.get(), &TranslationProvider::failed);
        p->translate(sampleRequest());
        QVERIFY(failed.wait(5000));
        const TranslationError e = failed.first().first().value<TranslationError>();
        QCOMPARE(e.kind, ErrorKind::Server);
        QCOMPARE(e.httpStatus, 529);
        QCOMPARE(m_server->requests().size(), 3);  // 1 + 2 retries
        QVERIFY(!p->isBusy());
    }

    void honoursRetryAfter()
    {
        m_server->enqueue(MockResponse::json(429, R"({"type":"error","error":{"type":"rate_limit_error","message":"slow"}})",
                                             {{"retry-after", "0"}}));
        m_server->enqueue(MockResponse::json(200, kClaudeOk));
        auto p = makeProvider<ClaudeProvider>();
        p->setRetryDelays({60000});  // would time out the test if retry-after were ignored
        QSignalSpy finished(p.get(), &TranslationProvider::finished);
        p->translate(sampleRequest());
        QVERIFY(finished.wait(5000));
        QCOMPARE(m_server->requests().size(), 2);
    }

    void longRetryAfterIsReportedNotAwaited()
    {
        m_server->enqueue(MockResponse::json(429, R"({"type":"error","error":{"type":"rate_limit_error","message":"slow"}})",
                                             {{"retry-after", "60"}}));
        auto p = makeProvider<ClaudeProvider>();
        QSignalSpy failed(p.get(), &TranslationProvider::failed);
        p->translate(sampleRequest());
        QVERIFY(failed.wait(5000));
        const TranslationError e = failed.first().first().value<TranslationError>();
        QCOMPARE(e.kind, ErrorKind::RateLimited);
        QVERIFY(e.message.contains(QStringLiteral("60 s")));
        QCOMPARE(m_server->requests().size(), 1);
    }

    void authErrorIsNotRetried()
    {
        m_server->enqueue(MockResponse::json(401, R"({"type":"error","error":{"type":"authentication_error","message":"invalid x-api-key"}})"));
        auto p = makeProvider<ClaudeProvider>();
        QSignalSpy failed(p.get(), &TranslationProvider::failed);
        p->translate(sampleRequest());
        QVERIFY(failed.wait(5000));
        const TranslationError e = failed.first().first().value<TranslationError>();
        QCOMPARE(e.kind, ErrorKind::Auth);
        QCOMPARE(e.httpStatus, 401);
        QCOMPARE(m_server->requests().size(), 1);
    }

    void unknownModel()
    {
        m_server->enqueue(MockResponse::json(404, R"({"type":"error","error":{"type":"not_found_error","message":"model: claude-nope"}})"));
        auto p = makeProvider<ClaudeProvider>();
        p->setModel(QStringLiteral("claude-nope"));
        QSignalSpy failed(p.get(), &TranslationProvider::failed);
        p->translate(sampleRequest());
        QVERIFY(failed.wait(5000));
        const TranslationError e = failed.first().first().value<TranslationError>();
        QCOMPARE(e.kind, ErrorKind::InvalidRequest);
        QVERIFY(e.message.contains(QStringLiteral("claude-nope")));
        const RecordedRequest &req = m_server->requests().first();
        QVERIFY(!req.headers.contains("anthropic-beta"));  // not a fallback-capable model
    }

    void claudeOlderModelRetriesWithoutEffort()
    {
        // e.g. a model picked from /v1/models that predates the effort parameter.
        m_server->enqueue(MockResponse::json(400, R"({"type":"error","error":{"type":"invalid_request_error","message":"output_config.effort: not supported for this model"}})"));
        m_server->enqueue(MockResponse::json(200, kClaudeOk));
        auto p = makeProvider<ClaudeProvider>();
        p->setModel(QStringLiteral("claude-sonnet-4-5"));
        QSignalSpy finished(p.get(), &TranslationProvider::finished);
        QSignalSpy failed(p.get(), &TranslationProvider::failed);
        p->translate(sampleRequest());
        QVERIFY(finished.wait(5000));
        QCOMPARE(failed.count(), 0);
        QCOMPARE(m_server->requests().size(), 2);
        const QJsonObject first = QJsonDocument::fromJson(m_server->requests().at(0).body).object();
        const QJsonObject second = QJsonDocument::fromJson(m_server->requests().at(1).body).object();
        QVERIFY(first.value(QStringLiteral("output_config")).toObject().contains(QStringLiteral("effort")));
        QVERIFY(!second.value(QStringLiteral("output_config")).toObject().contains(QStringLiteral("effort")));
        QVERIFY(second.value(QStringLiteral("output_config")).toObject().contains(QStringLiteral("format")));
    }

    void refusalFromServer()
    {
        m_server->enqueue(MockResponse::json(200, R"({"type":"message","content":[],"stop_reason":"refusal"})"));
        auto p = makeProvider<ClaudeProvider>();
        QSignalSpy failed(p.get(), &TranslationProvider::failed);
        p->translate(sampleRequest());
        QVERIFY(failed.wait(5000));
        QCOMPARE(failed.first().first().value<TranslationError>().kind, ErrorKind::Refused);
    }

    void cancelInFlight()
    {
        m_server->enqueue(MockResponse::hanging());
        auto p = makeProvider<ClaudeProvider>();
        QSignalSpy finished(p.get(), &TranslationProvider::finished);
        QSignalSpy failed(p.get(), &TranslationProvider::failed);
        p->translate(sampleRequest());
        QTRY_COMPARE(m_server->requests().size(), 1);
        p->cancel();
        QCOMPARE(failed.count(), 1);
        QCOMPARE(failed.first().first().value<TranslationError>().kind, ErrorKind::Cancelled);
        QVERIFY(!p->isBusy());
        QTest::qWait(100);
        QCOMPARE(finished.count(), 0);
        QCOMPARE(failed.count(), 1);
        p->cancel();  // idempotent: nothing in flight
        QCOMPARE(failed.count(), 1);
    }

    void cancelDuringRetryWait()
    {
        m_server->enqueue(MockResponse::json(503, R"({"type":"error","error":{"type":"api_error","message":"busy"}})"));
        m_server->enqueue(MockResponse::json(200, kClaudeOk));
        auto p = makeProvider<ClaudeProvider>();
        p->setRetryDelays({300});
        QSignalSpy finished(p.get(), &TranslationProvider::finished);
        QSignalSpy failed(p.get(), &TranslationProvider::failed);
        p->translate(sampleRequest());
        QTRY_COMPARE(m_server->requests().size(), 1);
        QTest::qWait(50);  // first reply handled, retry timer pending
        p->cancel();
        QCOMPARE(failed.count(), 1);
        QCOMPARE(failed.first().first().value<TranslationError>().kind, ErrorKind::Cancelled);
        QTest::qWait(500);
        QCOMPARE(m_server->requests().size(), 1);  // the retry never happened
        QCOMPARE(finished.count(), 0);
    }

    void translateWhileBusyCancelsPrevious()
    {
        m_server->enqueue(MockResponse::hanging());
        m_server->enqueue(MockResponse::json(200, kClaudeOk));
        auto p = makeProvider<ClaudeProvider>();
        QSignalSpy finished(p.get(), &TranslationProvider::finished);
        QSignalSpy failed(p.get(), &TranslationProvider::failed);
        p->translate(sampleRequest(QStringLiteral("first")));
        QTRY_COMPARE(m_server->requests().size(), 1);
        p->translate(sampleRequest(QStringLiteral("second")));
        QCOMPARE(failed.count(), 1);
        QCOMPARE(failed.first().first().value<TranslationError>().kind, ErrorKind::Cancelled);
        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.first().first().value<TranslationResult>().request.text, QStringLiteral("second"));
        QCOMPARE(failed.count(), 1);
    }

    void timeout()
    {
        m_server->enqueue(MockResponse::hanging());
        auto p = makeProvider<ClaudeProvider>();
        p->setTransferTimeout(300);
        QSignalSpy failed(p.get(), &TranslationProvider::failed);
        p->translate(sampleRequest());
        QVERIFY(failed.wait(5000));
        QCOMPARE(failed.first().first().value<TranslationError>().kind, ErrorKind::Timeout);
        QCOMPARE(m_server->requests().size(), 1);  // timeouts are not retried
    }

    void defaultTimeoutAllowsLongGenerations()
    {
        // The transfer timeout is an inactivity timeout and a non-streaming
        // reply only arrives once generation is complete, so the default must
        // cover a full max_tokens answer at high effort (SDKs use 10 minutes).
        ClaudeProvider claude(m_nam.get());
        OpenAIProvider openAi(m_nam.get());
        QCOMPARE(claude.transferTimeout(), 10 * 60 * 1000);
        QCOMPARE(openAi.transferTimeout(), 10 * 60 * 1000);
    }

    void networkError()
    {
        // A port nobody listens on.
        QTcpServer probe;
        QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
        const quint16 port = probe.serverPort();
        probe.close();

        auto p = makeProvider<ClaudeProvider>();
        p->setBaseUrl(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(port)));
        QSignalSpy failed(p.get(), &TranslationProvider::failed);
        p->translate(sampleRequest());
        QVERIFY(failed.wait(5000));
        QCOMPARE(failed.first().first().value<TranslationError>().kind, ErrorKind::Network);
    }

    void notConfiguredIsAsync()
    {
        auto p = makeProvider<ClaudeProvider>();
        p->setApiKey(QString());
        QSignalSpy failed(p.get(), &TranslationProvider::failed);
        p->translate(sampleRequest());
        QCOMPARE(failed.count(), 0);
        QVERIFY(failed.wait(2000));
        const TranslationError e = failed.first().first().value<TranslationError>();
        QCOMPARE(e.kind, ErrorKind::NotConfigured);
        QVERIFY(e.message.contains(QStringLiteral("Claude")));
        QCOMPARE(m_server->requests().size(), 0);
    }

    void claudeListModels()
    {
        m_server->enqueue(MockResponse::json(200, R"({"data":[{"id":"claude-opus-5-5"},{"id":"claude-haiku-4-5"}],"has_more":false})"));
        auto p = makeProvider<ClaudeProvider>();
        QSignalSpy listed(p.get(), &TranslationProvider::modelsListed);
        p->listModels(QStringLiteral("override-key"));
        QVERIFY(listed.wait(5000));
        QCOMPARE(listed.first().first().toStringList(),
                 (QStringList{QStringLiteral("claude-opus-5-5"), QStringLiteral("claude-haiku-4-5")}));
        const RecordedRequest &req = m_server->requests().first();
        QCOMPARE(req.method, QByteArray("GET"));
        QCOMPARE(req.target, QByteArray("/v1/models?limit=100"));
        QCOMPARE(req.headers.value("x-api-key"), QByteArray("override-key"));
        QCOMPARE(req.headers.value("anthropic-version"), QByteArray("2023-06-01"));
    }

    void listModelsFailures()
    {
        m_server->enqueue(MockResponse::json(401, R"({"type":"error","error":{"type":"authentication_error","message":"bad key"}})"));
        auto p = makeProvider<ClaudeProvider>();
        QSignalSpy failed(p.get(), &TranslationProvider::modelsListFailed);
        p->listModels();
        QVERIFY(failed.wait(5000));
        QCOMPARE(failed.first().first().value<TranslationError>().kind, ErrorKind::Auth);

        p->setApiKey(QString());
        failed.clear();
        p->listModels();
        QCOMPARE(failed.count(), 0);
        QVERIFY(failed.wait(2000));
        QCOMPARE(failed.first().first().value<TranslationError>().kind, ErrorKind::NotConfigured);
    }

    void openAiSuccess()
    {
        m_server->enqueue(MockResponse::json(200, kOpenAiOk));
        auto p = makeProvider<OpenAIProvider>();
        p->setModel(QStringLiteral("gpt-6.1-sol"));
        QSignalSpy finished(p.get(), &TranslationProvider::finished);
        p->translate(sampleRequest(QStringLiteral("Thank you")));
        QVERIFY(finished.wait(5000));
        const TranslationResult r = finished.first().first().value<TranslationResult>();
        QCOMPARE(r.translation, QStringLiteral("唔該晒"));
        QCOMPARE(r.providerId, QStringLiteral("openai"));
        const RecordedRequest &req = m_server->requests().first();
        QCOMPARE(req.target, QByteArray("/v1/chat/completions"));
        QCOMPARE(req.headers.value("authorization"), QByteArray("Bearer test-key"));
        const QJsonObject body = QJsonDocument::fromJson(req.body).object();
        QCOMPARE(body.value(QStringLiteral("reasoning_effort")).toString(), QStringLiteral("medium"));
    }

    void openAiRetriesWithoutReasoningEffort()
    {
        m_server->enqueue(MockResponse::json(400, R"({"error":{"message":"Unsupported parameter: 'reasoning_effort' is not supported with this model.","type":"invalid_request_error","param":"reasoning_effort","code":"unsupported_parameter"}})"));
        m_server->enqueue(MockResponse::json(200, kOpenAiOk));
        auto p = makeProvider<OpenAIProvider>();
        p->setModel(QStringLiteral("gpt-5-future-variant"));
        QSignalSpy finished(p.get(), &TranslationProvider::finished);
        QSignalSpy failed(p.get(), &TranslationProvider::failed);
        p->translate(sampleRequest(QStringLiteral("Thank you")));
        QVERIFY(finished.wait(5000));
        QCOMPARE(failed.count(), 0);
        QCOMPARE(m_server->requests().size(), 2);
        const QJsonObject first = QJsonDocument::fromJson(m_server->requests().at(0).body).object();
        const QJsonObject second = QJsonDocument::fromJson(m_server->requests().at(1).body).object();
        QVERIFY(first.contains(QStringLiteral("reasoning_effort")));
        QVERIFY(!second.contains(QStringLiteral("reasoning_effort")));
    }

    void openAiQuotaIsNotRetried()
    {
        m_server->enqueue(MockResponse::json(429, R"({"error":{"message":"You exceeded your current quota.","type":"insufficient_quota","param":null,"code":"insufficient_quota"}})"));
        auto p = makeProvider<OpenAIProvider>();
        QSignalSpy failed(p.get(), &TranslationProvider::failed);
        p->translate(sampleRequest());
        QVERIFY(failed.wait(5000));
        QCOMPARE(failed.first().first().value<TranslationError>().kind, ErrorKind::RateLimited);
        QCOMPARE(m_server->requests().size(), 1);
    }

    void openAiListModelsFilters()
    {
        m_server->enqueue(MockResponse::json(200, R"({"object":"list","data":[{"id":"gpt-6.1-sol"},{"id":"tts-1"},{"id":"gpt-6-luna"},{"id":"gpt-4o-realtime-preview"}]})"));
        auto p = makeProvider<OpenAIProvider>();
        QSignalSpy listed(p.get(), &TranslationProvider::modelsListed);
        p->listModels();
        QVERIFY(listed.wait(5000));
        QCOMPARE(listed.first().first().toStringList(),
                 (QStringList{QStringLiteral("gpt-6-luna"), QStringLiteral("gpt-6.1-sol")}));
        QCOMPARE(m_server->requests().first().target, QByteArray("/v1/models"));
    }
};

QTEST_GUILESS_MAIN(TstHttpProvider)
#include "tst_httpprovider.moc"
