#pragma once

#include "core/HttpProvider.h"

namespace sct {

// Pure request/response helpers for the Anthropic Messages API (unit-testable
// without network access).
namespace ClaudeApi {

QUrl defaultBaseUrl();          // https://api.anthropic.com
QString messagesPath();         // /v1/messages
QString modelsPath();           // /v1/models?limit=100
QByteArray apiVersion();        // 2023-06-01
QByteArray fallbackBetaValue(); // anthropic-beta value enabling server-side refusal fallbacks
int maxTokens();                // 16000

// "fast" -> "low", "balanced" -> "medium", "best" -> "high".
QString effortForQuality(const QString &quality);
// Haiku models reject output_config.effort.
bool supportsEffort(const QString &model);
// Models that accept "fallbacks": "default" (with the beta header).
bool supportsServerSideFallback(const QString &model);

// JSON body for POST /v1/messages (property order of the schema preserved).
QByteArray buildMessagesBody(const TranslationRequest &request, const QString &model, const QString &quality);
// Headers for /v1/messages; the beta header is only added for models that
// support server-side fallbacks.
HttpHeaders messagesHeaders(const QString &apiKey, const QString &model);
// Headers for GET /v1/models.
HttpHeaders modelsHeaders(const QString &apiKey);

// Parses a 200 response: checks stop_reason, concatenates "text" blocks only
// and hands the text to ResponseParser. result.model is the model reported by
// the API (empty if absent).
ParsedTranslation parseMessagesResponse(const QByteArray &body, const TranslationRequest &request);

// error.message / error.type from {"type":"error","error":{...}}.
QString errorMessage(const QByteArray &body);
QString errorType(const QByteArray &body);
TranslationError mapError(const HttpResult &result);

// data[].id from GET /v1/models (API order: newest first).
QStringList parseModelList(const QByteArray &body, QString *errorDetail);

} // namespace ClaudeApi

class ClaudeProvider : public HttpProvider
{
    Q_OBJECT

public:
    explicit ClaudeProvider(QNetworkAccessManager *nam, QObject *parent = nullptr);

    QString id() const override;
    QString displayName() const override;
    QStringList suggestedModels() const override;
    QString defaultModel() const override;

protected:
    QUrl defaultBaseUrl() const override;
    QString shortName() const override;
    HttpCall translateCall(const TranslationRequest &request, int variant) const override;
    HttpCall listModelsCall(const QString &key) const override;
    ParsedTranslation parseTranslateResponse(const QByteArray &body,
                                             const TranslationRequest &request) const override;
    QStringList parseModelList(const QByteArray &body, QString *errorDetail) const override;
    TranslationError errorFor(const HttpResult &result) const override;
};

} // namespace sct
