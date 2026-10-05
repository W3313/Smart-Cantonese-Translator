#pragma once

#include "core/ResponseParser.h"
#include "core/TranslationProvider.h"

#include <QByteArray>
#include <QList>
#include <QNetworkReply>
#include <QPair>
#include <QPointer>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;
class QTimer;

namespace sct {

using HttpHeaders = QList<QPair<QByteArray, QByteArray>>;

// Raw outcome of one HTTP exchange (the last attempt, after retries).
struct HttpResult
{
    int status = 0;  // HTTP status code; 0 when no HTTP response arrived
    QByteArray body;
    QNetworkReply::NetworkError networkError = QNetworkReply::NoError;
    QString errorString;
    bool timedOut = false;
    int retryAfterMs = -1;  // from retry-after / retry-after-ms; -1 when absent

    bool isSuccess() const
    {
        return status >= 200 && status < 300 && networkError == QNetworkReply::NoError && !timedOut;
    }
};

// A request built by a concrete provider; the path is appended to the base URL
// and may contain a query string.
struct HttpCall
{
    QByteArray verb = QByteArrayLiteral("POST");
    QString path;
    HttpHeaders headers;
    QByteArray body;
};

// Maps a failed exchange to a user-facing error. providerName is "Claude" or
// "OpenAI"; apiMessage is the message from the provider's error JSON (may be
// empty).
TranslationError httpErrorFor(const QString &providerName, const HttpResult &result, const QString &apiMessage);

// Parses Retry-After (delta seconds or HTTP-date) and retry-after-ms header
// values. Returns milliseconds, or -1 when absent or invalid.
int parseRetryAfterMs(const QByteArray &retryAfter, const QByteArray &retryAfterMs);

// 408/409/429/5xx/529 and transient network failures are worth retrying;
// timeouts are not (the user already waited a full timeout).
bool isRetryableHttpResult(const HttpResult &result);

// Shared HTTPS plumbing for the AI providers: request dispatch, automatic
// retries with backoff (honouring Retry-After), transfer timeout, error
// mapping and cancellation. Subclasses only build requests and parse replies.
class HttpProvider : public TranslationProvider
{
    Q_OBJECT

public:
    explicit HttpProvider(QNetworkAccessManager *nam, QObject *parent = nullptr);
    ~HttpProvider() override;

    void setApiKey(const QString &key) override;
    QString apiKey() const { return m_apiKey; }
    void setModel(const QString &model) override;  // empty -> defaultModel()
    QString model() const override { return m_model; }
    void setQuality(const QString &quality) override;  // invalid -> "balanced"
    QString quality() const { return m_quality; }

    bool isConfigured() const override;
    bool isBusy() const override { return m_translating; }

    void translate(const TranslationRequest &request) override;
    void cancel() override;
    void listModels(const QString &apiKeyOverride = QString()) override;

    // Overrides the API origin (tests, corporate gateways). An empty URL
    // restores the provider's official endpoint.
    void setBaseUrl(const QUrl &url);
    QUrl baseUrl() const;
    void setTransferTimeout(int msecs);  // default 120000
    // Delay before each automatic retry; the list length is the maximum number
    // of retries. Default {1000, 3000}.
    void setRetryDelays(const QList<int> &delaysMs);
    // Longest Retry-After the provider waits for automatically. Default 20000.
    void setMaxRetryAfter(int msecs);

    QNetworkAccessManager *networkManager() const { return m_nam; }

protected:
    virtual QUrl defaultBaseUrl() const = 0;
    virtual QString shortName() const = 0;  // "Claude" / "OpenAI" for messages
    // `variant` is a provider-defined bitmask of request fallbacks (0 = normal).
    virtual HttpCall translateCall(const TranslationRequest &request, int variant) const = 0;
    virtual HttpCall listModelsCall(const QString &key) const = 0;
    virtual ParsedTranslation parseTranslateResponse(const QByteArray &body,
                                                     const TranslationRequest &request) const = 0;
    virtual QStringList parseModelList(const QByteArray &body, QString *errorDetail) const = 0;
    virtual TranslationError errorFor(const HttpResult &result) const = 0;
    virtual bool shouldRetry(const HttpResult &result) const { return isRetryableHttpResult(result); }
    // After a final failure of translateCall(variant): return another variant
    // to try once (e.g. without an unsupported parameter), or -1.
    virtual int fallbackVariant(const HttpResult &result, int variant) const;

private:
    struct Operation
    {
        HttpCall call;
        int retriesDone = 0;
        quint64 token = 0;  // bumped on abort so stale callbacks are ignored
        QPointer<QNetworkReply> reply;
        QTimer *retryTimer = nullptr;
        std::function<void(const HttpResult &)> onDone;
    };

    void start(Operation *op, const HttpCall &call, std::function<void(const HttpResult &)> onDone);
    void sendAttempt(Operation *op);
    void onReplyFinished(Operation *op, QNetworkReply *reply, quint64 token);
    void abort(Operation *op);
    QUrl urlFor(const QString &path) const;

    void onTranslateDone(const HttpResult &result);
    void finishTranslate(const ParsedTranslation &outcome);

    QNetworkAccessManager *m_nam = nullptr;
    QString m_apiKey;
    QString m_model;
    QString m_quality;
    QUrl m_baseUrl;
    int m_transferTimeoutMs = 120000;
    QList<int> m_retryDelaysMs{1000, 3000};
    int m_maxRetryAfterMs = 20000;

    Operation m_translateOp;
    Operation m_listOp;
    bool m_translating = false;
    TranslationRequest m_request;
    int m_variant = 0;
};

} // namespace sct
