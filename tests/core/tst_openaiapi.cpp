#include "core/OpenAIProvider.h"
#include "core/PromptBuilder.h"
#include "core/ProviderDefaults.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

using namespace sct;

namespace {

TranslationRequest sampleRequest()
{
    TranslationRequest r;
    r.text = QStringLiteral("Thank you very much");
    r.tone = Tone::Polite;
    return r;
}

QJsonObject bodyFor(const QString &model, const QString &quality = QStringLiteral("balanced"), int variant = 0)
{
    return QJsonDocument::fromJson(OpenAiApi::buildChatBody(sampleRequest(), model, quality, variant)).object();
}

QByteArray chatResponse(const QJsonObject &message, const QString &finishReason = QStringLiteral("stop"))
{
    QJsonObject choice;
    choice.insert(QStringLiteral("index"), 0);
    choice.insert(QStringLiteral("message"), message);
    choice.insert(QStringLiteral("finish_reason"), finishReason);
    QJsonObject root;
    root.insert(QStringLiteral("id"), QStringLiteral("chatcmpl-1"));
    root.insert(QStringLiteral("object"), QStringLiteral("chat.completion"));
    root.insert(QStringLiteral("model"), QStringLiteral("gpt-6.1-sol-2026-09-29"));
    root.insert(QStringLiteral("choices"), QJsonArray{choice});
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QJsonObject assistantMessage(const QJsonValue &content, const QJsonValue &refusal = QJsonValue::Null)
{
    QJsonObject m;
    m.insert(QStringLiteral("role"), QStringLiteral("assistant"));
    m.insert(QStringLiteral("content"), content);
    m.insert(QStringLiteral("refusal"), refusal);
    return m;
}

const QString kAnswer = QStringLiteral(
    R"({"translation":"唔該晒","jyutping":"m4 goi1 saai3","literal":"thanks all","alternatives":[{"text":"多謝晒","jyutping":"do1 ze6 saai3","note":"for a gift"}],"notes":["唔該 is for services."]})");

HttpResult httpResult(int status, const QByteArray &body)
{
    HttpResult r;
    r.status = status;
    r.body = body;
    r.networkError = QNetworkReply::UnknownContentError;
    return r;
}

} // namespace

class TstOpenAiApi : public QObject
{
    Q_OBJECT

private slots:
    void defaults()
    {
        OpenAIProvider p(nullptr);
        QCOMPARE(p.id(), QStringLiteral("openai"));
        QCOMPARE(p.displayName(), QStringLiteral("OpenAI"));
        QCOMPARE(p.defaultModel(), ProviderDefaults::openAiDefaultModel());
        QCOMPARE(p.model(), p.defaultModel());
        QVERIFY(p.suggestedModels().contains(p.defaultModel()));
        QVERIFY(p.suggestedModels().size() >= 2);
        QCOMPARE(p.baseUrl(), QUrl(QStringLiteral("https://api.openai.com")));
    }

    void requestBody()
    {
        const QJsonObject body = bodyFor(QStringLiteral("gpt-6.1-sol"));
        QCOMPARE(body.value(QStringLiteral("model")).toString(), QStringLiteral("gpt-6.1-sol"));
        QCOMPARE(body.value(QStringLiteral("max_completion_tokens")).toInt(), 16000);
        QVERIFY(!body.contains(QStringLiteral("max_tokens")));
        QVERIFY(!body.contains(QStringLiteral("temperature")));
        QVERIFY(!body.contains(QStringLiteral("top_p")));

        const QJsonArray messages = body.value(QStringLiteral("messages")).toArray();
        QCOMPARE(messages.size(), 2);
        QCOMPARE(messages.at(0).toObject().value(QStringLiteral("role")).toString(), QStringLiteral("developer"));
        QCOMPARE(messages.at(0).toObject().value(QStringLiteral("content")).toString(), PromptBuilder::systemPrompt());
        QCOMPARE(messages.at(1).toObject().value(QStringLiteral("role")).toString(), QStringLiteral("user"));
        QCOMPARE(messages.at(1).toObject().value(QStringLiteral("content")).toString(),
                 PromptBuilder::userMessage(sampleRequest()));

        const QJsonObject format = body.value(QStringLiteral("response_format")).toObject();
        QCOMPARE(format.value(QStringLiteral("type")).toString(), QStringLiteral("json_schema"));
        const QJsonObject jsonSchema = format.value(QStringLiteral("json_schema")).toObject();
        QCOMPARE(jsonSchema.value(QStringLiteral("name")).toString(), QStringLiteral("cantonese_translation"));
        QCOMPARE(jsonSchema.value(QStringLiteral("strict")).toBool(), true);
        QCOMPARE(jsonSchema.value(QStringLiteral("schema")).toObject(), PromptBuilder::responseSchema());

        QVERIFY(OpenAiApi::buildChatBody(sampleRequest(), QStringLiteral("gpt-6.1-sol"), QStringLiteral("fast"))
                    .contains(PromptBuilder::responseSchemaJson()));
    }

    void reasoningEffort_data()
    {
        QTest::addColumn<QString>("model");
        QTest::addColumn<QString>("quality");
        QTest::addColumn<QString>("expected");  // empty = not sent
        QTest::newRow("gpt-6.1-sol fast") << QStringLiteral("gpt-6.1-sol") << QStringLiteral("fast") << QStringLiteral("low");
        QTest::newRow("gpt-6.1-sol balanced") << QStringLiteral("gpt-6.1-sol") << QStringLiteral("balanced") << QStringLiteral("medium");
        QTest::newRow("gpt-6-astra best") << QStringLiteral("gpt-6-astra") << QStringLiteral("best") << QStringLiteral("high");
        QTest::newRow("gpt-5") << QStringLiteral("gpt-5") << QStringLiteral("best") << QStringLiteral("high");
        QTest::newRow("gpt-5.5") << QStringLiteral("gpt-5.5") << QStringLiteral("fast") << QStringLiteral("low");
        QTest::newRow("gpt-5-mini") << QStringLiteral("gpt-5-mini") << QStringLiteral("balanced") << QStringLiteral("medium");
        QTest::newRow("o3") << QStringLiteral("o3") << QStringLiteral("best") << QStringLiteral("high");
        QTest::newRow("o4-mini") << QStringLiteral("o4-mini") << QStringLiteral("fast") << QStringLiteral("low");
        QTest::newRow("gpt-4o") << QStringLiteral("gpt-4o") << QStringLiteral("best") << QString();
        QTest::newRow("gpt-4.1") << QStringLiteral("gpt-4.1") << QStringLiteral("best") << QString();
        QTest::newRow("gpt-5-chat-latest") << QStringLiteral("gpt-5-chat-latest") << QStringLiteral("best") << QString();
        QTest::newRow("unknown") << QStringLiteral("some-model") << QStringLiteral("best") << QString();
    }

    void reasoningEffort()
    {
        QFETCH(QString, model);
        QFETCH(QString, quality);
        QFETCH(QString, expected);
        const QJsonObject body = bodyFor(model, quality);
        QCOMPARE(body.contains(QStringLiteral("reasoning_effort")), !expected.isEmpty());
        if (!expected.isEmpty())
            QCOMPARE(body.value(QStringLiteral("reasoning_effort")).toString(), expected);
        const QString role =
            body.value(QStringLiteral("messages")).toArray().first().toObject().value(QStringLiteral("role")).toString();
        QCOMPARE(role, expected.isEmpty() ? QStringLiteral("system") : QStringLiteral("developer"));
    }

    void variants()
    {
        const QJsonObject noEffort = bodyFor(QStringLiteral("gpt-6.1-sol"), QStringLiteral("best"),
                                             OpenAiApi::NoReasoningEffort);
        QVERIFY(!noEffort.contains(QStringLiteral("reasoning_effort")));
        QCOMPARE(noEffort.value(QStringLiteral("response_format")).toObject().value(QStringLiteral("type")).toString(),
                 QStringLiteral("json_schema"));

        const QJsonObject jsonMode = bodyFor(QStringLiteral("gpt-4"), QStringLiteral("best"),
                                             OpenAiApi::JsonObjectFormat);
        QCOMPARE(jsonMode.value(QStringLiteral("response_format")).toObject().value(QStringLiteral("type")).toString(),
                 QStringLiteral("json_object"));
        const QString instructions =
            jsonMode.value(QStringLiteral("messages")).toArray().first().toObject().value(QStringLiteral("content")).toString();
        QVERIFY(instructions.contains(QString::fromUtf8(PromptBuilder::responseSchemaJson())));
    }

    void headers()
    {
        const HttpHeaders h = OpenAiApi::requestHeaders(QStringLiteral("sk-proj-1"), true);
        bool auth = false, type = false;
        for (const auto &p : h) {
            if (p.first.toLower() == "authorization") {
                QCOMPARE(p.second, QByteArray("Bearer sk-proj-1"));
                auth = true;
            }
            if (p.first.toLower() == "content-type")
                type = true;
        }
        QVERIFY(auth);
        QVERIFY(type);
        QCOMPARE(OpenAiApi::chatCompletionsPath(), QStringLiteral("/v1/chat/completions"));
    }

    void parseContent()
    {
        const ParsedTranslation p =
            OpenAiApi::parseChatResponse(chatResponse(assistantMessage(kAnswer)), sampleRequest());
        QVERIFY2(p.ok, qPrintable(p.error.detail));
        QCOMPARE(p.result.translation, QStringLiteral("唔該晒"));
        QCOMPARE(p.result.jyutping, QStringLiteral("m4 goi1 saai3"));
        QCOMPARE(p.result.alternatives.size(), 1);
        QCOMPARE(p.result.notes.size(), 1);
        QCOMPARE(p.result.model, QStringLiteral("gpt-6.1-sol-2026-09-29"));
    }

    void parseContentParts()
    {
        QJsonObject part;
        part.insert(QStringLiteral("type"), QStringLiteral("text"));
        part.insert(QStringLiteral("text"), kAnswer);
        const ParsedTranslation p =
            OpenAiApi::parseChatResponse(chatResponse(assistantMessage(QJsonArray{part})), sampleRequest());
        QVERIFY2(p.ok, qPrintable(p.error.detail));
    }

    void parseRefusal()
    {
        const ParsedTranslation p = OpenAiApi::parseChatResponse(
            chatResponse(assistantMessage(QJsonValue::Null, QStringLiteral("I'm sorry, I can't help with that."))),
            sampleRequest());
        QVERIFY(!p.ok);
        QCOMPARE(p.error.kind, ErrorKind::Refused);
        QVERIFY(p.error.detail.contains(QStringLiteral("can't help")));
    }

    void parseLength()
    {
        const ParsedTranslation p = OpenAiApi::parseChatResponse(
            chatResponse(assistantMessage(QStringLiteral("{\"translation\":\"唔")), QStringLiteral("length")),
            sampleRequest());
        QVERIFY(!p.ok);
        QCOMPARE(p.error.kind, ErrorKind::BadResponse);
        QVERIFY(p.error.message.contains(QStringLiteral("cut off")));
    }

    void parseContentFilterAndEmpty()
    {
        QCOMPARE(OpenAiApi::parseChatResponse(chatResponse(assistantMessage(QJsonValue::Null), QStringLiteral("content_filter")),
                                              sampleRequest())
                     .error.kind,
                 ErrorKind::Refused);
        QCOMPARE(OpenAiApi::parseChatResponse(R"({"choices":[]})", sampleRequest()).error.kind, ErrorKind::BadResponse);
        QCOMPARE(OpenAiApi::parseChatResponse("oops", sampleRequest()).error.kind, ErrorKind::BadResponse);
        QCOMPARE(OpenAiApi::parseChatResponse(chatResponse(assistantMessage(QStringLiteral(""))), sampleRequest()).error.kind,
                 ErrorKind::BadResponse);
    }

    void errorMapping()
    {
        const QByteArray auth = R"({"error":{"message":"Incorrect API key provided: sk-...","type":"invalid_request_error","param":null,"code":"invalid_api_key"}})";
        const TranslationError a = OpenAiApi::mapError(httpResult(401, auth));
        QCOMPARE(a.kind, ErrorKind::Auth);
        QCOMPARE(a.message, QStringLiteral("Your OpenAI API key was rejected — check it in Settings."));
        QVERIFY(a.detail.contains(QStringLiteral("invalid_api_key")));

        const QByteArray model = R"({"error":{"message":"The model `gpt-99` does not exist or you do not have access to it.","type":"invalid_request_error","param":null,"code":"model_not_found"}})";
        const TranslationError m = OpenAiApi::mapError(httpResult(404, model));
        QCOMPARE(m.kind, ErrorKind::InvalidRequest);
        QVERIFY(m.message.contains(QStringLiteral("gpt-99")));

        const QByteArray quota = R"({"error":{"message":"You exceeded your current quota.","type":"insufficient_quota","param":null,"code":"insufficient_quota"}})";
        const TranslationError q = OpenAiApi::mapError(httpResult(429, quota));
        QCOMPARE(q.kind, ErrorKind::RateLimited);
        QVERIFY(q.message.contains(QStringLiteral("credit")));
        QVERIFY(!OpenAiApi::shouldRetry(httpResult(429, quota)));

        const QByteArray rate = R"({"error":{"message":"Rate limit reached","type":"requests","param":null,"code":"rate_limit_exceeded"}})";
        QVERIFY(OpenAiApi::shouldRetry(httpResult(429, rate)));
        QCOMPARE(OpenAiApi::mapError(httpResult(503, rate)).kind, ErrorKind::Server);
    }

    void fallbackVariants()
    {
        const QByteArray effort = R"({"error":{"message":"Unsupported parameter: 'reasoning_effort' is not supported with this model.","type":"invalid_request_error","param":"reasoning_effort","code":"unsupported_parameter"}})";
        QCOMPARE(OpenAiApi::fallbackVariant(httpResult(400, effort), QStringLiteral("gpt-6.1-sol"), 0),
                 int(OpenAiApi::NoReasoningEffort));
        // Already tried without it: no further fallback for the same error.
        QCOMPARE(OpenAiApi::fallbackVariant(httpResult(400, effort), QStringLiteral("gpt-6.1-sol"),
                                            OpenAiApi::NoReasoningEffort),
                 -1);
        // Never sent for this model, so the error is about something else.
        QCOMPARE(OpenAiApi::fallbackVariant(httpResult(400, effort), QStringLiteral("gpt-4o"), 0), -1);

        const QByteArray format = R"({"error":{"message":"Invalid parameter: 'response_format' of type 'json_schema' is not supported with this model.","type":"invalid_request_error","param":"response_format","code":null}})";
        QCOMPARE(OpenAiApi::fallbackVariant(httpResult(400, format), QStringLiteral("gpt-4"), 0),
                 int(OpenAiApi::JsonObjectFormat));
        QCOMPARE(OpenAiApi::fallbackVariant(httpResult(400, format), QStringLiteral("gpt-4"), OpenAiApi::JsonObjectFormat),
                 -1);

        QCOMPARE(OpenAiApi::fallbackVariant(httpResult(401, effort), QStringLiteral("gpt-6.1-sol"), 0), -1);
    }

    void chatModelFilter_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<bool>("chat");
        for (const char *id : {"gpt-6.1-sol", "gpt-6-astra", "gpt-6-luna", "gpt-5", "gpt-5-mini", "gpt-4o",
                               "gpt-4.1-nano", "o3", "o4-mini", "gpt-5-chat-latest"})
            QTest::newRow(id) << QString::fromLatin1(id) << true;
        for (const char *id : {"gpt-4o-audio-preview", "gpt-4o-realtime-preview", "gpt-4o-mini-tts",
                               "gpt-4o-transcribe", "gpt-image-1", "gpt-4o-search-preview", "text-embedding-3-large",
                               "omni-moderation-latest", "gpt-3.5-turbo-instruct", "dall-e-3", "whisper-1",
                               "gpt-5-codex", "o3-deep-research", "o1-pro", "gpt-5-pro", "davinci-002", "tts-1",
                               "computer-use-preview", "babbage-002"})
            QTest::newRow(id) << QString::fromLatin1(id) << false;
    }

    void chatModelFilter()
    {
        QFETCH(QString, id);
        QFETCH(bool, chat);
        QCOMPARE(OpenAiApi::isChatModelId(id), chat);
    }

    void modelList()
    {
        const QByteArray body = R"({"object":"list","data":[{"id":"whisper-1"},{"id":"gpt-6.1-sol"},{"id":"o3"},{"id":"gpt-6-astra"},{"id":"gpt-4o-mini-tts"},{"id":"gpt-6-luna"}]})";
        QString err;
        const QStringList ids = OpenAiApi::parseModelList(body, &err);
        QVERIFY(err.isEmpty());
        QCOMPARE(ids, (QStringList{QStringLiteral("gpt-6-astra"), QStringLiteral("gpt-6-luna"),
                                   QStringLiteral("gpt-6.1-sol"), QStringLiteral("o3")}));
    }
};

QTEST_GUILESS_MAIN(TstOpenAiApi)
#include "tst_openaiapi.moc"
