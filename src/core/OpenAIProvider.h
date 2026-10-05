#pragma once

#include "core/HttpProvider.h"

namespace sct {

// Pure request/response helpers for the OpenAI Chat Completions API
// (unit-testable without network access).
namespace OpenAiApi {

QUrl defaultBaseUrl();        // https://api.openai.com
QString chatCompletionsPath(); // /v1/chat/completions
QString modelsPath();          // /v1/models
int maxCompletionTokens();     // 16000 (includes reasoning tokens)

// Request fallbacks, used after the API rejects a parameter for a model.
enum Variant : int {
    Normal = 0,
    NoReasoningEffort = 1,  // omit reasoning_effort (and use the "system" role)
    JsonObjectFormat = 2,   // response_format json_object + schema in the prompt
};

// gpt-5 and later (except *-chat* variants) and o-series models accept
// reasoning_effort. Unknown models get no reasoning_effort.
bool supportsReasoningEffort(const QString &model);
// "fast" -> "low", "balanced" -> "medium", "best" -> "high".
QString reasoningEffortForQuality(const QString &quality);

QByteArray buildChatBody(const TranslationRequest &request, const QString &model, const QString &quality,
                         int variant = Normal);
HttpHeaders requestHeaders(const QString &apiKey, bool hasBody);

// Parses a 200 response: refusal -> Refused, finish_reason "length" ->
// BadResponse, otherwise choices[0].message.content through ResponseParser.
ParsedTranslation parseChatResponse(const QByteArray &body, const TranslationRequest &request);

// error.message / error.code / error.type from {"error":{...}}.
QString errorMessage(const QByteArray &body);
QString errorCode(const QByteArray &body);
TranslationError mapError(const HttpResult &result);
bool shouldRetry(const HttpResult &result);  // like the default, but never for insufficient_quota
// Next request variant to try after `result` (a failed attempt), or -1.
int fallbackVariant(const HttpResult &result, const QString &model, int variant);

// True for chat-capable model ids (gpt-*, o<digit>*), excluding audio,
// realtime, TTS, transcription, image, search, embedding, moderation,
// instruct and Responses-only (codex, pro, deep-research) variants.
bool isChatModelId(const QString &id);
// Chat model ids from GET /v1/models, sorted.
QStringList parseModelList(const QByteArray &body, QString *errorDetail);

} // namespace OpenAiApi

class OpenAIProvider : public HttpProvider
{
    Q_OBJECT

public:
    explicit OpenAIProvider(QNetworkAccessManager *nam, QObject *parent = nullptr);

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
    bool shouldRetry(const HttpResult &result) const override;
    int fallbackVariant(const HttpResult &result, int variant) const override;
};

} // namespace sct
