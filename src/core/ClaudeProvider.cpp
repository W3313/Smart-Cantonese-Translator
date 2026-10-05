#include "core/ClaudeProvider.h"

#include "core/PromptBuilder.h"
#include "core/ProviderDefaults.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace sct {

namespace ClaudeApi {

namespace {

QJsonObject parseObject(const QByteArray &body, bool *ok)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    *ok = err.error == QJsonParseError::NoError && doc.isObject();
    return doc.object();
}

QString excerpt(const QByteArray &body)
{
    constexpr qsizetype kMax = 300;
    const QString s = QString::fromUtf8(body.left(kMax * 4));
    return s.size() <= kMax ? s : s.left(kMax) + QStringLiteral("…");
}

} // namespace

QUrl defaultBaseUrl()
{
    return QUrl(QStringLiteral("https://api.anthropic.com"));
}

QString messagesPath()
{
    return QStringLiteral("/v1/messages");
}

QString modelsPath()
{
    return QStringLiteral("/v1/models?limit=100");
}

QByteArray apiVersion()
{
    return QByteArrayLiteral("2023-06-01");
}

QByteArray fallbackBetaValue()
{
    return QByteArrayLiteral("server-side-fallback-2026-07-01");
}

int maxTokens()
{
    return 16000;
}

QString effortForQuality(const QString &quality)
{
    const QString q = quality.trimmed().toLower();
    if (q == QLatin1String("fast"))
        return QStringLiteral("low");
    if (q == QLatin1String("best"))
        return QStringLiteral("high");
    return QStringLiteral("medium");
}

bool supportsEffort(const QString &model)
{
    return !model.contains(QLatin1String("haiku"), Qt::CaseInsensitive);
}

bool supportsServerSideFallback(const QString &model)
{
    static const QStringList models{QStringLiteral("claude-opus-5-5"), QStringLiteral("claude-opus-5"),
                                    QStringLiteral("claude-sonnet-5-5"), QStringLiteral("claude-fable-5-1")};
    return models.contains(model.trimmed());
}

QByteArray buildMessagesBody(const TranslationRequest &request, const QString &model, const QString &quality,
                             int variant)
{
    const PromptBuilder::Prompt prompt = PromptBuilder::build(request);
    QString system = prompt.system;

    QJsonObject outputConfig;
    if (supportsEffort(model) && !(variant & NoEffort))
        outputConfig.insert(QStringLiteral("effort"), effortForQuality(quality));
    if (variant & NoStructuredOutput) {
        // Model without structured outputs: spell the schema out instead
        // (ResponseParser tolerates fences and prose around the JSON).
        system += QStringLiteral("\nThe JSON object must follow this JSON schema exactly:\n")
                  + QString::fromUtf8(PromptBuilder::responseSchemaJson()) + QLatin1Char('\n');
    } else {
        QJsonObject format;
        format.insert(QStringLiteral("type"), QStringLiteral("json_schema"));
        format.insert(QStringLiteral("schema"), PromptBuilder::schemaPlaceholder());
        outputConfig.insert(QStringLiteral("format"), format);
    }

    QJsonObject userMessage;
    userMessage.insert(QStringLiteral("role"), QStringLiteral("user"));
    userMessage.insert(QStringLiteral("content"), prompt.user);

    QJsonObject body;
    body.insert(QStringLiteral("model"), model);
    body.insert(QStringLiteral("max_tokens"), maxTokens());
    body.insert(QStringLiteral("system"), system);
    body.insert(QStringLiteral("messages"), QJsonArray{userMessage});
    if (!outputConfig.isEmpty())
        body.insert(QStringLiteral("output_config"), outputConfig);
    if (supportsServerSideFallback(model))
        body.insert(QStringLiteral("fallbacks"), QStringLiteral("default"));
    // Deliberately no temperature/top_p/top_k, no "thinking" field and no
    // assistant prefill: current models reject them.
    return PromptBuilder::toJsonWithSchema(body);
}

HttpHeaders modelsHeaders(const QString &apiKey)
{
    return {{QByteArrayLiteral("x-api-key"), apiKey.toUtf8()},
            {QByteArrayLiteral("anthropic-version"), apiVersion()},
            {QByteArrayLiteral("accept"), QByteArrayLiteral("application/json")}};
}

HttpHeaders messagesHeaders(const QString &apiKey, const QString &model)
{
    HttpHeaders h = modelsHeaders(apiKey);
    h.append({QByteArrayLiteral("content-type"), QByteArrayLiteral("application/json")});
    if (supportsServerSideFallback(model))
        h.append({QByteArrayLiteral("anthropic-beta"), fallbackBetaValue()});
    return h;
}

ParsedTranslation parseMessagesResponse(const QByteArray &body, const TranslationRequest &request)
{
    bool ok = false;
    const QJsonObject obj = parseObject(body, &ok);
    if (!ok) {
        return ParsedTranslation::failure(ErrorKind::BadResponse,
                                          QStringLiteral("Claude sent a response the app couldn't read. Please try again."),
                                          QStringLiteral("Invalid JSON: ") + excerpt(body));
    }

    if (obj.value(QStringLiteral("type")).toString() == QLatin1String("error")) {
        return ParsedTranslation::failure(ErrorKind::Server,
                                          QStringLiteral("Claude reported an error. Please try again."),
                                          errorType(body) + QStringLiteral(": ") + errorMessage(body));
    }

    const QString stopReason = obj.value(QStringLiteral("stop_reason")).toString();
    if (stopReason == QLatin1String("refusal")) {
        return ParsedTranslation::failure(ErrorKind::Refused,
                                          QStringLiteral("Claude declined to translate this text."),
                                          QStringLiteral("stop_reason: refusal"));
    }
    if (stopReason == QLatin1String("max_tokens")
        || stopReason == QLatin1String("model_context_window_exceeded")) {
        return ParsedTranslation::failure(
            ErrorKind::BadResponse,
            QStringLiteral("The response was cut off — try translating a shorter passage."),
            QStringLiteral("stop_reason: ") + stopReason);
    }

    QString text;
    const QJsonArray content = obj.value(QStringLiteral("content")).toArray();
    for (const QJsonValue &block : content) {
        const QJsonObject b = block.toObject();
        if (b.value(QStringLiteral("type")).toString() == QLatin1String("text"))
            text += b.value(QStringLiteral("text")).toString();
    }
    if (text.trimmed().isEmpty()) {
        return ParsedTranslation::failure(ErrorKind::BadResponse,
                                          QStringLiteral("Claude returned an empty answer. Please try again."),
                                          QStringLiteral("No text content (stop_reason: %1)").arg(stopReason));
    }

    ParsedTranslation parsed = ResponseParser::parse(text, request);
    if (parsed.ok)
        parsed.result.model = obj.value(QStringLiteral("model")).toString();
    return parsed;
}

QString errorMessage(const QByteArray &body)
{
    bool ok = false;
    const QJsonObject obj = parseObject(body, &ok);
    if (!ok)
        return {};
    return obj.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString().trimmed();
}

QString errorType(const QByteArray &body)
{
    bool ok = false;
    const QJsonObject obj = parseObject(body, &ok);
    if (!ok)
        return {};
    return obj.value(QStringLiteral("error")).toObject().value(QStringLiteral("type")).toString();
}

TranslationError mapError(const HttpResult &result)
{
    TranslationError e = httpErrorFor(QStringLiteral("Claude"), result, errorMessage(result.body));
    const QString type = errorType(result.body);
    if (!type.isEmpty() && !e.detail.contains(type))
        e.detail = type + QStringLiteral(" — ") + e.detail;
    return e;
}

int fallbackVariant(const HttpResult &result, const QString &model, int variant)
{
    if (result.status != 400)
        return -1;
    const QString message = errorMessage(result.body).toLower();
    if (!(variant & NoEffort) && supportsEffort(model) && message.contains(QLatin1String("effort")))
        return variant | NoEffort;
    if (!(variant & NoStructuredOutput)
        && (message.contains(QLatin1String("output_config")) || message.contains(QLatin1String("format"))
            || message.contains(QLatin1String("schema")) || message.contains(QLatin1String("structured"))))
        return variant | NoStructuredOutput;
    return -1;
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
        if (!modelId.isEmpty() && !ids.contains(modelId))
            ids.append(modelId);
    }
    return ids;
}

} // namespace ClaudeApi

// ---- ClaudeProvider ---------------------------------------------------------------

ClaudeProvider::ClaudeProvider(QNetworkAccessManager *nam, QObject *parent)
    : HttpProvider(nam, parent)
{
    setModel(ProviderDefaults::claudeDefaultModel());
}

QString ClaudeProvider::id() const
{
    return ProviderDefaults::claudeProviderId();
}

QString ClaudeProvider::displayName() const
{
    return QStringLiteral("Claude (Anthropic)");
}

QStringList ClaudeProvider::suggestedModels() const
{
    return ProviderDefaults::claudeSuggestedModels();
}

QString ClaudeProvider::defaultModel() const
{
    return ProviderDefaults::claudeDefaultModel();
}

QUrl ClaudeProvider::defaultBaseUrl() const
{
    return ClaudeApi::defaultBaseUrl();
}

QString ClaudeProvider::shortName() const
{
    return QStringLiteral("Claude");
}

HttpCall ClaudeProvider::translateCall(const TranslationRequest &request, int variant) const
{
    HttpCall call;
    call.verb = QByteArrayLiteral("POST");
    call.path = ClaudeApi::messagesPath();
    call.headers = ClaudeApi::messagesHeaders(apiKey(), model());
    call.body = ClaudeApi::buildMessagesBody(request, model(), quality(), variant);
    return call;
}

HttpCall ClaudeProvider::listModelsCall(const QString &key) const
{
    HttpCall call;
    call.verb = QByteArrayLiteral("GET");
    call.path = ClaudeApi::modelsPath();
    call.headers = ClaudeApi::modelsHeaders(key);
    return call;
}

ParsedTranslation ClaudeProvider::parseTranslateResponse(const QByteArray &body,
                                                         const TranslationRequest &request) const
{
    return ClaudeApi::parseMessagesResponse(body, request);
}

QStringList ClaudeProvider::parseModelList(const QByteArray &body, QString *errorDetail) const
{
    return ClaudeApi::parseModelList(body, errorDetail);
}

TranslationError ClaudeProvider::errorFor(const HttpResult &result) const
{
    return ClaudeApi::mapError(result);
}

int ClaudeProvider::fallbackVariant(const HttpResult &result, int variant) const
{
    return ClaudeApi::fallbackVariant(result, model(), variant);
}

} // namespace sct
