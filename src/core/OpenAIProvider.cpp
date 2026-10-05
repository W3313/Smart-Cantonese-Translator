#include "core/OpenAIProvider.h"

#include "core/PromptBuilder.h"
#include "core/ProviderDefaults.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>

namespace sct {

namespace OpenAiApi {

namespace {

QJsonObject parseObject(const QByteArray &body, bool *ok)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    *ok = err.error == QJsonParseError::NoError && doc.isObject();
    return doc.object();
}

QJsonObject errorObject(const QByteArray &body)
{
    bool ok = false;
    const QJsonObject obj = parseObject(body, &ok);
    return ok ? obj.value(QStringLiteral("error")).toObject() : QJsonObject();
}

QString excerpt(const QByteArray &body)
{
    constexpr qsizetype kMax = 300;
    const QString s = QString::fromUtf8(body.left(kMax * 4));
    return s.size() <= kMax ? s : s.left(kMax) + QStringLiteral("…");
}

// Message content is normally a string; tolerate the array-of-parts form.
QString contentText(const QJsonValue &content)
{
    if (content.isString())
        return content.toString();
    QString text;
    for (const QJsonValue &part : content.toArray()) {
        const QJsonObject p = part.toObject();
        const QString type = p.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("text") || type == QLatin1String("output_text"))
            text += p.value(QStringLiteral("text")).toString();
    }
    return text;
}

bool useDeveloperRole(const QString &model, int variant)
{
    return supportsReasoningEffort(model) && !(variant & NoReasoningEffort);
}

} // namespace

QUrl defaultBaseUrl()
{
    return QUrl(QStringLiteral("https://api.openai.com"));
}

QString chatCompletionsPath()
{
    return QStringLiteral("/v1/chat/completions");
}

QString modelsPath()
{
    return QStringLiteral("/v1/models");
}

int maxCompletionTokens()
{
    return 16000;
}

bool supportsReasoningEffort(const QString &model)
{
    const QString m = model.trimmed().toLower();
    static const QRegularExpression oSeries(QStringLiteral("^o\\d"));
    if (oSeries.match(m).hasMatch())
        return true;
    static const QRegularExpression gpt(QStringLiteral("^gpt-(\\d+)"));
    const QRegularExpressionMatch match = gpt.match(m);
    if (!match.hasMatch())
        return false;
    if (match.captured(1).toInt() < 5)
        return false;
    // Chat-tuned snapshots such as gpt-5-chat-latest are not reasoning models.
    return !m.contains(QLatin1String("-chat"));
}

QString reasoningEffortForQuality(const QString &quality)
{
    const QString q = quality.trimmed().toLower();
    if (q == QLatin1String("fast"))
        return QStringLiteral("low");
    if (q == QLatin1String("best"))
        return QStringLiteral("high");
    return QStringLiteral("medium");
}

QByteArray buildChatBody(const TranslationRequest &request, const QString &model, const QString &quality,
                         int variant)
{
    const PromptBuilder::Prompt prompt = PromptBuilder::build(request);

    QString instructions = prompt.system;
    QJsonObject responseFormat;
    if (variant & JsonObjectFormat) {
        // Older models without Structured Outputs: JSON mode plus the schema
        // spelled out in the instructions.
        responseFormat.insert(QStringLiteral("type"), QStringLiteral("json_object"));
        instructions += QStringLiteral("\nThe JSON object must follow this JSON schema exactly:\n")
                        + QString::fromUtf8(PromptBuilder::responseSchemaJson()) + QLatin1Char('\n');
    } else {
        QJsonObject jsonSchema;
        jsonSchema.insert(QStringLiteral("name"), PromptBuilder::schemaName());
        jsonSchema.insert(QStringLiteral("strict"), true);
        jsonSchema.insert(QStringLiteral("schema"), PromptBuilder::schemaPlaceholder());
        responseFormat.insert(QStringLiteral("type"), QStringLiteral("json_schema"));
        responseFormat.insert(QStringLiteral("json_schema"), jsonSchema);
    }

    QJsonObject systemMessage;
    systemMessage.insert(QStringLiteral("role"),
                         useDeveloperRole(model, variant) ? QStringLiteral("developer") : QStringLiteral("system"));
    systemMessage.insert(QStringLiteral("content"), instructions);

    QJsonObject userMessage;
    userMessage.insert(QStringLiteral("role"), QStringLiteral("user"));
    userMessage.insert(QStringLiteral("content"), prompt.user);

    QJsonObject body;
    body.insert(QStringLiteral("model"), model);
    body.insert(QStringLiteral("messages"), QJsonArray{systemMessage, userMessage});
    body.insert(QStringLiteral("response_format"), responseFormat);
    body.insert(QStringLiteral("max_completion_tokens"), maxCompletionTokens());
    if (supportsReasoningEffort(model) && !(variant & NoReasoningEffort))
        body.insert(QStringLiteral("reasoning_effort"), reasoningEffortForQuality(quality));
    // No temperature: reasoning models reject it.
    return PromptBuilder::toJsonWithSchema(body);
}

HttpHeaders requestHeaders(const QString &apiKey, bool hasBody)
{
    HttpHeaders h{{QByteArrayLiteral("authorization"), QByteArrayLiteral("Bearer ") + apiKey.toUtf8()},
                  {QByteArrayLiteral("accept"), QByteArrayLiteral("application/json")}};
    if (hasBody)
        h.append({QByteArrayLiteral("content-type"), QByteArrayLiteral("application/json")});
    return h;
}

ParsedTranslation parseChatResponse(const QByteArray &body, const TranslationRequest &request)
{
    bool ok = false;
    const QJsonObject obj = parseObject(body, &ok);
    if (!ok) {
        return ParsedTranslation::failure(ErrorKind::BadResponse,
                                          QStringLiteral("OpenAI sent a response the app couldn't read. Please try again."),
                                          QStringLiteral("Invalid JSON: ") + excerpt(body));
    }
    if (obj.contains(QStringLiteral("error")) && obj.value(QStringLiteral("error")).isObject()) {
        return ParsedTranslation::failure(ErrorKind::Server, QStringLiteral("OpenAI reported an error. Please try again."),
                                          errorMessage(body));
    }

    const QJsonArray choices = obj.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        return ParsedTranslation::failure(ErrorKind::BadResponse,
                                          QStringLiteral("OpenAI returned an empty answer. Please try again."),
                                          QStringLiteral("No choices: ") + excerpt(body));
    }
    const QJsonObject choice = choices.first().toObject();
    const QJsonObject message = choice.value(QStringLiteral("message")).toObject();
    const QString finishReason = choice.value(QStringLiteral("finish_reason")).toString();

    const QJsonValue refusal = message.value(QStringLiteral("refusal"));
    if (refusal.isString() && !refusal.toString().trimmed().isEmpty()) {
        return ParsedTranslation::failure(ErrorKind::Refused,
                                          QStringLiteral("The OpenAI model declined to translate this text."),
                                          refusal.toString().trimmed());
    }
    if (finishReason == QLatin1String("content_filter")) {
        return ParsedTranslation::failure(ErrorKind::Refused,
                                          QStringLiteral("OpenAI's content filter blocked this translation."),
                                          QStringLiteral("finish_reason: content_filter"));
    }
    if (finishReason == QLatin1String("length")) {
        return ParsedTranslation::failure(
            ErrorKind::BadResponse,
            QStringLiteral("The response was cut off — try translating a shorter passage."),
            QStringLiteral("finish_reason: length"));
    }

    const QString text = contentText(message.value(QStringLiteral("content")));
    if (text.trimmed().isEmpty()) {
        return ParsedTranslation::failure(ErrorKind::BadResponse,
                                          QStringLiteral("OpenAI returned an empty answer. Please try again."),
                                          QStringLiteral("Empty message content (finish_reason: %1)").arg(finishReason));
    }

    ParsedTranslation parsed = ResponseParser::parse(text, request);
    if (parsed.ok)
        parsed.result.model = obj.value(QStringLiteral("model")).toString();
    return parsed;
}

QString errorMessage(const QByteArray &body)
{
    return errorObject(body).value(QStringLiteral("message")).toString().trimmed();
}

QString errorCode(const QByteArray &body)
{
    const QJsonValue code = errorObject(body).value(QStringLiteral("code"));
    if (code.isString())
        return code.toString();
    if (code.isDouble())
        return QString::number(code.toInt());
    return {};
}

TranslationError mapError(const HttpResult &result)
{
    TranslationError e = httpErrorFor(QStringLiteral("OpenAI"), result, errorMessage(result.body));
    const QString code = errorCode(result.body);
    if (result.status == 429 && code == QLatin1String("insufficient_quota")) {
        e.message = QStringLiteral("Your OpenAI account has no remaining credit or quota — check your plan and "
                                   "billing on the OpenAI platform.");
    }
    if (!code.isEmpty() && !e.detail.contains(code))
        e.detail = code + QStringLiteral(" — ") + e.detail;
    return e;
}

bool shouldRetry(const HttpResult &result)
{
    if (result.status == 429 && errorCode(result.body) == QLatin1String("insufficient_quota"))
        return false;  // retrying never helps; the account needs credit
    return isRetryableHttpResult(result);
}

int fallbackVariant(const HttpResult &result, const QString &model, int variant)
{
    if (result.status != 400)
        return -1;
    const QString message = errorMessage(result.body).toLower();
    const QString param = errorObject(result.body).value(QStringLiteral("param")).toString().toLower();
    const auto mentions = [&](const QString &needle) { return message.contains(needle) || param.contains(needle); };

    if (!(variant & NoReasoningEffort) && supportsReasoningEffort(model) && mentions(QStringLiteral("reasoning")))
        return variant | NoReasoningEffort;
    if (!(variant & JsonObjectFormat)
        && (mentions(QStringLiteral("response_format")) || mentions(QStringLiteral("json_schema"))))
        return variant | JsonObjectFormat;
    return -1;
}

bool isChatModelId(const QString &modelId)
{
    const QString id = modelId.trimmed().toLower();
    static const QRegularExpression oSeries(QStringLiteral("^o\\d"));
    if (!id.startsWith(QLatin1String("gpt-")) && !oSeries.match(id).hasMatch())
        return false;
    static const QStringList excluded{
        QStringLiteral("audio"),     QStringLiteral("realtime"),   QStringLiteral("tts"),
        QStringLiteral("transcribe"), QStringLiteral("image"),      QStringLiteral("search"),
        QStringLiteral("embedding"), QStringLiteral("moderation"), QStringLiteral("instruct"),
        QStringLiteral("codex"),     QStringLiteral("deep-research"), QStringLiteral("computer-use"),
        QStringLiteral("diarize")};
    for (const QString &word : excluded) {
        if (id.contains(word))
            return false;
    }
    // "-pro" variants (o1-pro, gpt-5-pro, ...) are Responses-API only.
    static const QRegularExpression pro(QStringLiteral("-pro(-|$)"));
    return !pro.match(id).hasMatch();
}

QStringList parseModelList(const QByteArray &body, QString *errorDetail)
{
    bool ok = false;
    const QJsonObject obj = parseObject(body, &ok);
    const QJsonValue data = obj.value(QStringLiteral("data"));
    if (!ok || !data.isArray()) {
        if (errorDetail)
            *errorDetail = QStringLiteral("Expected {\"data\": [...]}: ") + excerpt(body);
        return {};
    }
    QStringList ids;
    for (const QJsonValue &v : data.toArray()) {
        const QString modelId = v.toObject().value(QStringLiteral("id")).toString().trimmed();
        if (isChatModelId(modelId) && !ids.contains(modelId))
            ids.append(modelId);
    }
    ids.sort(Qt::CaseInsensitive);
    return ids;
}

} // namespace OpenAiApi

// ---- OpenAIProvider ---------------------------------------------------------------

OpenAIProvider::OpenAIProvider(QNetworkAccessManager *nam, QObject *parent)
    : HttpProvider(nam, parent)
{
    setModel(ProviderDefaults::openAiDefaultModel());
}

QString OpenAIProvider::id() const
{
    return ProviderDefaults::openAiProviderId();
}

QString OpenAIProvider::displayName() const
{
    return QStringLiteral("OpenAI");
}

QStringList OpenAIProvider::suggestedModels() const
{
    return ProviderDefaults::openAiSuggestedModels();
}

QString OpenAIProvider::defaultModel() const
{
    return ProviderDefaults::openAiDefaultModel();
}

QUrl OpenAIProvider::defaultBaseUrl() const
{
    return OpenAiApi::defaultBaseUrl();
}

QString OpenAIProvider::shortName() const
{
    return QStringLiteral("OpenAI");
}

HttpCall OpenAIProvider::translateCall(const TranslationRequest &request, int variant) const
{
    HttpCall call;
    call.verb = QByteArrayLiteral("POST");
    call.path = OpenAiApi::chatCompletionsPath();
    call.headers = OpenAiApi::requestHeaders(apiKey(), true);
    call.body = OpenAiApi::buildChatBody(request, model(), quality(), variant);
    return call;
}

HttpCall OpenAIProvider::listModelsCall(const QString &key) const
{
    HttpCall call;
    call.verb = QByteArrayLiteral("GET");
    call.path = OpenAiApi::modelsPath();
    call.headers = OpenAiApi::requestHeaders(key, false);
    return call;
}

ParsedTranslation OpenAIProvider::parseTranslateResponse(const QByteArray &body,
                                                         const TranslationRequest &request) const
{
    return OpenAiApi::parseChatResponse(body, request);
}

QStringList OpenAIProvider::parseModelList(const QByteArray &body, QString *errorDetail) const
{
    return OpenAiApi::parseModelList(body, errorDetail);
}

TranslationError OpenAIProvider::errorFor(const HttpResult &result) const
{
    return OpenAiApi::mapError(result);
}

bool OpenAIProvider::shouldRetry(const HttpResult &result) const
{
    return OpenAiApi::shouldRetry(result);
}

int OpenAIProvider::fallbackVariant(const HttpResult &result, int variant) const
{
    return OpenAiApi::fallbackVariant(result, model(), variant);
}

} // namespace sct
