#include "core/ClaudeProvider.h"
#include "core/PromptBuilder.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

using namespace sct;

namespace {

TranslationRequest sampleRequest()
{
    TranslationRequest r;
    r.text = QStringLiteral("Where is he?");
    r.tone = Tone::Casual;
    return r;
}

QJsonObject bodyFor(const QString &model, const QString &quality = QStringLiteral("balanced"))
{
    const QByteArray body = ClaudeApi::buildMessagesBody(sampleRequest(), model, quality);
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError)
        qWarning() << "invalid body JSON:" << err.errorString();
    return doc.object();
}

QByteArray headerValue(const HttpHeaders &headers, const QByteArray &name)
{
    for (const auto &h : headers) {
        if (h.first.toLower() == name)
            return h.second;
    }
    return {};
}

bool hasHeader(const HttpHeaders &headers, const QByteArray &name)
{
    for (const auto &h : headers) {
        if (h.first.toLower() == name)
            return true;
    }
    return false;
}

const char kModelJson[] =
    R"({\"translation\":\"佢喺邊度呀？\",\"jyutping\":\"keoi5 hai2 bin1 dou6 aa3?\",\"literal\":\"\",\"alternatives\":[],\"notes\":[]})";

QByteArray messageResponse(const QString &contentArrayJson, const QString &stopReason = QStringLiteral("end_turn"))
{
    return QStringLiteral(R"({"id":"msg_01","type":"message","role":"assistant","model":"claude-opus-5-5","content":%1,"stop_reason":"%2","usage":{"input_tokens":10,"output_tokens":20}})")
        .arg(contentArrayJson, stopReason)
        .toUtf8();
}

HttpResult httpResult(int status, const QByteArray &body = QByteArray())
{
    HttpResult r;
    r.status = status;
    r.body = body;
    r.networkError = status >= 400 ? QNetworkReply::UnknownContentError : QNetworkReply::NoError;
    return r;
}

} // namespace

class TstClaudeApi : public QObject
{
    Q_OBJECT

private slots:
    void defaults()
    {
        ClaudeProvider p(nullptr);
        QCOMPARE(p.id(), QStringLiteral("claude"));
        QCOMPARE(p.defaultModel(), QStringLiteral("claude-opus-5-5"));
        QCOMPARE(p.model(), QStringLiteral("claude-opus-5-5"));
        QCOMPARE(p.suggestedModels(), (QStringList{QStringLiteral("claude-opus-5-5"), QStringLiteral("claude-sonnet-5-5"),
                                                   QStringLiteral("claude-haiku-4-5")}));
        QVERIFY(!p.isConfigured());
        p.setApiKey(QStringLiteral("  sk-ant-x  "));
        QVERIFY(p.isConfigured());
        p.setModel(QString());
        QCOMPARE(p.model(), QStringLiteral("claude-opus-5-5"));
        p.setModel(QStringLiteral("claude-some-future-model"));
        QCOMPARE(p.model(), QStringLiteral("claude-some-future-model"));
        QCOMPARE(p.baseUrl(), QUrl(QStringLiteral("https://api.anthropic.com")));
    }

    void requestBodyBasics()
    {
        const QJsonObject body = bodyFor(QStringLiteral("claude-opus-5-5"));
        QCOMPARE(body.value(QStringLiteral("model")).toString(), QStringLiteral("claude-opus-5-5"));
        QCOMPARE(body.value(QStringLiteral("max_tokens")).toInt(), 16000);
        QCOMPARE(body.value(QStringLiteral("system")).toString(), PromptBuilder::systemPrompt());

        const QJsonArray messages = body.value(QStringLiteral("messages")).toArray();
        QCOMPARE(messages.size(), 1);
        QCOMPARE(messages.first().toObject().value(QStringLiteral("role")).toString(), QStringLiteral("user"));
        const QString content = messages.first().toObject().value(QStringLiteral("content")).toString();
        QCOMPARE(content, PromptBuilder::userMessage(sampleRequest()));
        QVERIFY(content.contains(QStringLiteral("<source_text>\nWhere is he?\n</source_text>")));

        const QJsonObject outputConfig = body.value(QStringLiteral("output_config")).toObject();
        const QJsonObject format = outputConfig.value(QStringLiteral("format")).toObject();
        QCOMPARE(format.value(QStringLiteral("type")).toString(), QStringLiteral("json_schema"));
        QCOMPARE(format.value(QStringLiteral("schema")).toObject(), PromptBuilder::responseSchema());

        // Parameters current models reject must never be sent.
        for (const char *key : {"temperature", "top_p", "top_k", "thinking", "stop_sequences"})
            QVERIFY2(!body.contains(QLatin1String(key)), key);
        QCOMPARE(messages.size(), 1);  // no assistant prefill
    }

    void schemaOrderPreservedInBody()
    {
        const QByteArray raw = ClaudeApi::buildMessagesBody(sampleRequest(), QStringLiteral("claude-opus-5-5"),
                                                            QStringLiteral("balanced"));
        QVERIFY(raw.contains(PromptBuilder::responseSchemaJson()));
    }

    void effortMapping_data()
    {
        QTest::addColumn<QString>("quality");
        QTest::addColumn<QString>("effort");
        QTest::newRow("fast") << QStringLiteral("fast") << QStringLiteral("low");
        QTest::newRow("balanced") << QStringLiteral("balanced") << QStringLiteral("medium");
        QTest::newRow("best") << QStringLiteral("best") << QStringLiteral("high");
        QTest::newRow("unknown") << QStringLiteral("whatever") << QStringLiteral("medium");
    }

    void effortMapping()
    {
        QFETCH(QString, quality);
        QFETCH(QString, effort);
        QCOMPARE(ClaudeApi::effortForQuality(quality), effort);
        const QJsonObject body = bodyFor(QStringLiteral("claude-sonnet-5-5"), quality);
        QCOMPARE(body.value(QStringLiteral("output_config")).toObject().value(QStringLiteral("effort")).toString(),
                 effort);
    }

    void haikuOmitsEffort()
    {
        QVERIFY(!ClaudeApi::supportsEffort(QStringLiteral("claude-haiku-4-5")));
        QVERIFY(ClaudeApi::supportsEffort(QStringLiteral("claude-opus-5-5")));
        const QJsonObject outputConfig =
            bodyFor(QStringLiteral("claude-haiku-4-5"), QStringLiteral("best")).value(QStringLiteral("output_config")).toObject();
        QVERIFY(!outputConfig.contains(QStringLiteral("effort")));
        QVERIFY(outputConfig.contains(QStringLiteral("format")));
    }

    void fallbacks_data()
    {
        QTest::addColumn<QString>("model");
        QTest::addColumn<bool>("expected");
        QTest::newRow("opus 5.5") << QStringLiteral("claude-opus-5-5") << true;
        QTest::newRow("opus 5") << QStringLiteral("claude-opus-5") << true;
        QTest::newRow("sonnet 5.5") << QStringLiteral("claude-sonnet-5-5") << true;
        QTest::newRow("fable 5.1") << QStringLiteral("claude-fable-5-1") << true;
        QTest::newRow("haiku 4.5") << QStringLiteral("claude-haiku-4-5") << false;
        QTest::newRow("sonnet 4.5") << QStringLiteral("claude-sonnet-4-5") << false;
        QTest::newRow("dated variant") << QStringLiteral("claude-opus-5-5-20270101") << false;
        QTest::newRow("unknown") << QStringLiteral("my-custom-model") << false;
    }

    void fallbacks()
    {
        QFETCH(QString, model);
        QFETCH(bool, expected);
        QCOMPARE(ClaudeApi::supportsServerSideFallback(model), expected);

        const QJsonObject body = bodyFor(model);
        QCOMPARE(body.contains(QStringLiteral("fallbacks")), expected);
        if (expected)
            QCOMPARE(body.value(QStringLiteral("fallbacks")).toString(), QStringLiteral("default"));

        const HttpHeaders headers = ClaudeApi::messagesHeaders(QStringLiteral("sk-ant-key"), model);
        QCOMPARE(hasHeader(headers, "anthropic-beta"), expected);
        if (expected)
            QCOMPARE(headerValue(headers, "anthropic-beta"), QByteArray("server-side-fallback-2026-07-01"));
    }

    void headers()
    {
        const HttpHeaders h = ClaudeApi::messagesHeaders(QStringLiteral("sk-ant-key"), QStringLiteral("claude-opus-5-5"));
        QCOMPARE(headerValue(h, "x-api-key"), QByteArray("sk-ant-key"));
        QCOMPARE(headerValue(h, "anthropic-version"), QByteArray("2023-06-01"));
        QCOMPARE(headerValue(h, "content-type"), QByteArray("application/json"));
        QVERIFY(!hasHeader(h, "authorization"));

        const HttpHeaders m = ClaudeApi::modelsHeaders(QStringLiteral("other-key"));
        QCOMPARE(headerValue(m, "x-api-key"), QByteArray("other-key"));
        QCOMPARE(headerValue(m, "anthropic-version"), QByteArray("2023-06-01"));
        QVERIFY(!hasHeader(m, "anthropic-beta"));
        QCOMPARE(ClaudeApi::modelsPath(), QStringLiteral("/v1/models?limit=100"));
        QCOMPARE(ClaudeApi::messagesPath(), QStringLiteral("/v1/messages"));
    }

    void parseTextBlocksOnly()
    {
        // The JSON answer is split over two text blocks; a thinking block with
        // a decoy JSON object and a fallback block must be ignored.
        const QString content = QStringLiteral(
            R"([{"type":"thinking","thinking":"{\"translation\":\"WRONG\"}","signature":"abc"},)"
            R"({"type":"fallback","from_model":"claude-opus-5-5","to_model":"claude-opus-5"},)"
            R"({"type":"text","text":"{\"translation\":\"佢喺邊度呀？\",\"jyutping\":\"keoi5 hai2 "},)"
            R"({"type":"text","text":"bin1 dou6 aa3?\",\"literal\":\"\",\"alternatives\":[],\"notes\":[]}"}])");
        const ParsedTranslation p = ClaudeApi::parseMessagesResponse(messageResponse(content), sampleRequest());
        QVERIFY2(p.ok, qPrintable(p.error.detail));
        QCOMPARE(p.result.translation, QStringLiteral("佢喺邊度呀？"));
        QCOMPARE(p.result.jyutping, QStringLiteral("keoi5 hai2 bin1 dou6 aa3?"));
        QCOMPARE(p.result.model, QStringLiteral("claude-opus-5-5"));
    }

    void parseSimple()
    {
        const QString content = QStringLiteral(R"([{"type":"text","text":"%1"}])").arg(QString::fromUtf8(kModelJson));
        const ParsedTranslation p = ClaudeApi::parseMessagesResponse(messageResponse(content), sampleRequest());
        QVERIFY2(p.ok, qPrintable(p.error.detail));
        QCOMPARE(p.result.translation, QStringLiteral("佢喺邊度呀？"));
    }

    void parseRefusal()
    {
        const ParsedTranslation p = ClaudeApi::parseMessagesResponse(
            messageResponse(QStringLiteral(R"([{"type":"text","text":"partial"}])"), QStringLiteral("refusal")),
            sampleRequest());
        QVERIFY(!p.ok);
        QCOMPARE(p.error.kind, ErrorKind::Refused);
    }

    void parseMaxTokens()
    {
        const ParsedTranslation p = ClaudeApi::parseMessagesResponse(
            messageResponse(QStringLiteral(R"([{"type":"text","text":"{\"translation\":\"佢"}])"),
                            QStringLiteral("max_tokens")),
            sampleRequest());
        QVERIFY(!p.ok);
        QCOMPARE(p.error.kind, ErrorKind::BadResponse);
        QVERIFY(p.error.message.contains(QStringLiteral("cut off")));
    }

    void parseGarbage()
    {
        QCOMPARE(ClaudeApi::parseMessagesResponse("<html>", sampleRequest()).error.kind, ErrorKind::BadResponse);
        QCOMPARE(ClaudeApi::parseMessagesResponse(messageResponse(QStringLiteral("[]")), sampleRequest()).error.kind,
                 ErrorKind::BadResponse);
        const QByteArray notJson = messageResponse(QStringLiteral(R"([{"type":"text","text":"Sorry, no."}])"));
        QCOMPARE(ClaudeApi::parseMessagesResponse(notJson, sampleRequest()).error.kind, ErrorKind::BadResponse);
    }

    void errorJson()
    {
        const QByteArray body =
            R"({"type":"error","error":{"type":"not_found_error","message":"model: claude-nonexistent-1"},"request_id":"req_1"})";
        QCOMPARE(ClaudeApi::errorMessage(body), QStringLiteral("model: claude-nonexistent-1"));
        QCOMPARE(ClaudeApi::errorType(body), QStringLiteral("not_found_error"));
        QVERIFY(ClaudeApi::errorMessage("not json").isEmpty());

        const ParsedTranslation p = ClaudeApi::parseMessagesResponse(body, sampleRequest());
        QVERIFY(!p.ok);
        QVERIFY(p.error.detail.contains(QStringLiteral("claude-nonexistent-1")));
    }

    void httpStatusMapping_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<ErrorKind>("kind");
        QTest::newRow("400") << 400 << ErrorKind::InvalidRequest;
        QTest::newRow("401") << 401 << ErrorKind::Auth;
        QTest::newRow("403") << 403 << ErrorKind::Auth;
        QTest::newRow("404") << 404 << ErrorKind::InvalidRequest;
        QTest::newRow("413") << 413 << ErrorKind::InvalidRequest;
        QTest::newRow("429") << 429 << ErrorKind::RateLimited;
        QTest::newRow("500") << 500 << ErrorKind::Server;
        QTest::newRow("502") << 502 << ErrorKind::Server;
        QTest::newRow("503") << 503 << ErrorKind::Server;
        QTest::newRow("504") << 504 << ErrorKind::Server;
        QTest::newRow("529") << 529 << ErrorKind::Server;
    }

    void httpStatusMapping()
    {
        QFETCH(int, status);
        QFETCH(ErrorKind, kind);
        const QByteArray body = R"({"type":"error","error":{"type":"some_error","message":"API says no"}})";
        const TranslationError e = ClaudeApi::mapError(httpResult(status, body));
        QCOMPARE(e.kind, kind);
        QCOMPARE(e.httpStatus, status);
        QVERIFY(!e.message.isEmpty());
        QVERIFY(e.detail.contains(QStringLiteral("API says no")));
        QVERIFY(e.detail.contains(QStringLiteral("some_error")));
    }

    void authMessage()
    {
        const TranslationError e = ClaudeApi::mapError(httpResult(401, R"({"type":"error","error":{"type":"authentication_error","message":"invalid x-api-key"}})"));
        QCOMPARE(e.message, QStringLiteral("Your Claude API key was rejected — check it in Settings."));
    }

    void invalidRequestIncludesApiMessage()
    {
        const TranslationError e = ClaudeApi::mapError(
            httpResult(404, R"({"type":"error","error":{"type":"not_found_error","message":"model: claude-foo"}})"));
        QCOMPARE(e.kind, ErrorKind::InvalidRequest);
        QVERIFY(e.message.contains(QStringLiteral("model: claude-foo")));
    }

    void rateLimitMentionsRetryAfter()
    {
        HttpResult r = httpResult(429, R"({"type":"error","error":{"type":"rate_limit_error","message":"slow down"}})");
        r.retryAfterMs = 42000;
        const TranslationError e = ClaudeApi::mapError(r);
        QCOMPARE(e.kind, ErrorKind::RateLimited);
        QVERIFY(e.message.contains(QStringLiteral("42 s")));
    }

    void networkAndTimeout()
    {
        HttpResult net;
        net.networkError = QNetworkReply::ConnectionRefusedError;
        net.errorString = QStringLiteral("Connection refused");
        QCOMPARE(ClaudeApi::mapError(net).kind, ErrorKind::Network);
        QCOMPARE(ClaudeApi::mapError(net).detail, QStringLiteral("Connection refused"));

        HttpResult dns;
        dns.networkError = QNetworkReply::HostNotFoundError;
        QCOMPARE(ClaudeApi::mapError(dns).kind, ErrorKind::Network);

        HttpResult timeout;
        timeout.networkError = QNetworkReply::OperationCanceledError;
        timeout.timedOut = true;
        QCOMPARE(ClaudeApi::mapError(timeout).kind, ErrorKind::Timeout);
    }

    void retryClassification()
    {
        QVERIFY(isRetryableHttpResult(httpResult(429)));
        QVERIFY(isRetryableHttpResult(httpResult(500)));
        QVERIFY(isRetryableHttpResult(httpResult(503)));
        QVERIFY(isRetryableHttpResult(httpResult(529)));
        QVERIFY(!isRetryableHttpResult(httpResult(200)));
        QVERIFY(!isRetryableHttpResult(httpResult(400)));
        QVERIFY(!isRetryableHttpResult(httpResult(401)));
        QVERIFY(!isRetryableHttpResult(httpResult(404)));
        HttpResult closed;
        closed.networkError = QNetworkReply::RemoteHostClosedError;
        QVERIFY(isRetryableHttpResult(closed));
        HttpResult refused;
        refused.networkError = QNetworkReply::ConnectionRefusedError;
        QVERIFY(!isRetryableHttpResult(refused));
        HttpResult timeout;
        timeout.networkError = QNetworkReply::OperationCanceledError;
        timeout.timedOut = true;
        QVERIFY(!isRetryableHttpResult(timeout));
    }

    void retryAfterParsing()
    {
        QCOMPARE(parseRetryAfterMs(QByteArray(), QByteArray()), -1);
        QCOMPARE(parseRetryAfterMs("7", QByteArray()), 7000);
        QCOMPARE(parseRetryAfterMs("1.5", QByteArray()), 1500);
        QCOMPARE(parseRetryAfterMs("7", "250"), 250);  // retry-after-ms wins
        QCOMPARE(parseRetryAfterMs("garbage", QByteArray()), -1);
        QCOMPARE(parseRetryAfterMs("-3", QByteArray()), -1);
        const QByteArray httpDate =
            QDateTime::currentDateTimeUtc().addSecs(30).toString(Qt::RFC2822Date).toLatin1();
        const int ms = parseRetryAfterMs(httpDate, QByteArray());
        QVERIFY2(ms > 25000 && ms <= 31000, qPrintable(QString::number(ms)));
    }

    void modelList()
    {
        const QByteArray body =
            R"({"data":[{"type":"model","id":"claude-opus-5-5","display_name":"Claude Opus 5.5"},{"type":"model","id":"claude-sonnet-5-5"},{"type":"model","id":"claude-haiku-4-5"}],"has_more":false})";
        QString err;
        const QStringList ids = ClaudeApi::parseModelList(body, &err);
        QVERIFY(err.isEmpty());
        QCOMPARE(ids, (QStringList{QStringLiteral("claude-opus-5-5"), QStringLiteral("claude-sonnet-5-5"),
                                   QStringLiteral("claude-haiku-4-5")}));
        ClaudeApi::parseModelList("{\"nope\":1}", &err);
        QVERIFY(!err.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstClaudeApi)
#include "tst_claudeapi.moc"
