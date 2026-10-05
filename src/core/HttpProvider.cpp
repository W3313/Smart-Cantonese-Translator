#include "core/HttpProvider.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QTimer>

#include <chrono>
#include <utility>

namespace sct {

namespace {

constexpr qint64 kMaxRetryAfterMs = 24LL * 60 * 60 * 1000;  // clamp absurd header values

QString bodyExcerpt(const QByteArray &body)
{
    constexpr qsizetype kMax = 500;
    const QString s = QString::fromUtf8(body.left(kMax * 4)).trimmed();
    return s.size() <= kMax ? s : s.left(kMax) + QStringLiteral("…");
}

bool isSslError(QNetworkReply::NetworkError e)
{
    return e == QNetworkReply::SslHandshakeFailedError;
}

bool isProxyError(QNetworkReply::NetworkError e)
{
    return e >= QNetworkReply::ProxyConnectionRefusedError && e <= QNetworkReply::UnknownProxyError;
}

} // namespace

// ---- free helpers -------------------------------------------------------------

int parseRetryAfterMs(const QByteArray &retryAfter, const QByteArray &retryAfterMs)
{
    bool ok = false;
    const QByteArray ms = retryAfterMs.trimmed();
    if (!ms.isEmpty()) {
        const double v = ms.toDouble(&ok);
        if (ok && v >= 0)
            return int(qMin<qint64>(qint64(v), kMaxRetryAfterMs));
    }

    const QByteArray ra = retryAfter.trimmed();
    if (ra.isEmpty())
        return -1;
    const double secs = ra.toDouble(&ok);
    if (ok) {
        if (secs < 0)
            return -1;
        return int(qMin<qint64>(qint64(secs * 1000.0), kMaxRetryAfterMs));
    }
    // HTTP-date form, e.g. "Wed, 21 Oct 2026 07:28:00 GMT".
    const QDateTime when = QDateTime::fromString(QString::fromLatin1(ra), Qt::RFC2822Date);
    if (!when.isValid())
        return -1;
    const qint64 delta = QDateTime::currentDateTimeUtc().msecsTo(when.toUTC());
    return int(qBound<qint64>(0, delta, kMaxRetryAfterMs));
}

bool isRetryableHttpResult(const HttpResult &r)
{
    if (r.isSuccess() || r.timedOut)
        return false;
    if (r.status >= 300) {
        return r.status == 408 || r.status == 429 || r.status >= 500;  // 5xx includes 529 overloaded
    }
    switch (r.networkError) {
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::UnknownNetworkError:
    case QNetworkReply::ProxyConnectionClosedError:
        return true;
    default:
        return false;
    }
}

TranslationError httpErrorFor(const QString &providerName, const HttpResult &r, const QString &apiMessage)
{
    TranslationError e;
    e.httpStatus = r.status;
    const QString apiOrBody = apiMessage.isEmpty() ? bodyExcerpt(r.body) : apiMessage;

    if (r.timedOut) {
        e.kind = ErrorKind::Timeout;
        e.message = QStringLiteral("%1 took too long to respond. Please try again.").arg(providerName);
        e.detail = r.errorString;
        return e;
    }

    if (r.status >= 300) {
        e.detail = QStringLiteral("HTTP %1").arg(r.status);
        if (!apiOrBody.isEmpty())
            e.detail += QStringLiteral(": ") + apiOrBody;

        switch (r.status) {
        case 401:
        case 403:
            e.kind = ErrorKind::Auth;
            e.message = QStringLiteral("Your %1 API key was rejected — check it in Settings.").arg(providerName);
            return e;
        case 408:
            e.kind = ErrorKind::Timeout;
            e.message = QStringLiteral("%1 took too long to respond. Please try again.").arg(providerName);
            return e;
        case 413:
            e.kind = ErrorKind::InvalidRequest;
            e.message = QStringLiteral("The text is too long to translate in one go — try a shorter passage.");
            return e;
        case 429: {
            e.kind = ErrorKind::RateLimited;
            e.message = QStringLiteral("%1 is receiving too many requests right now. Please wait a moment and try again.")
                            .arg(providerName);
            if (r.retryAfterMs > 0) {
                const int secs = (r.retryAfterMs + 999) / 1000;
                e.message = QStringLiteral("%1 is receiving too many requests right now. Please try again in %2 s.")
                                .arg(providerName)
                                .arg(secs);
            }
            return e;
        }
        case 529:
            e.kind = ErrorKind::Server;
            e.message = QStringLiteral("%1 is overloaded right now. Please try again in a moment.").arg(providerName);
            return e;
        default:
            break;
        }

        if (r.status >= 500) {
            e.kind = ErrorKind::Server;
            e.message = QStringLiteral("%1 is temporarily unavailable (server error %2). Please try again shortly.")
                            .arg(providerName)
                            .arg(r.status);
        } else if (r.status >= 400) {
            e.kind = ErrorKind::InvalidRequest;
            e.message = apiMessage.isEmpty()
                            ? QStringLiteral("%1 rejected the request (HTTP %2).").arg(providerName).arg(r.status)
                            : QStringLiteral("%1 rejected the request: %2").arg(providerName, apiMessage);
        } else {
            e.kind = ErrorKind::BadResponse;
            e.message = QStringLiteral("Unexpected response from %1 (HTTP %2).").arg(providerName).arg(r.status);
        }
        return e;
    }

    // No usable HTTP response: a network-level failure.
    e.kind = ErrorKind::Network;
    e.detail = r.errorString;
    if (isSslError(r.networkError)) {
        e.message = QStringLiteral("A secure connection to %1 couldn't be established. Check your network, "
                                   "proxy or antivirus settings.")
                        .arg(providerName);
    } else if (isProxyError(r.networkError)) {
        e.message = QStringLiteral("Couldn't reach %1 through your proxy. Check your proxy settings.").arg(providerName);
    } else {
        e.message = QStringLiteral("Couldn't connect to %1. Check your internet connection and try again.")
                        .arg(providerName);
    }
    return e;
}

// ---- HttpProvider ---------------------------------------------------------------

HttpProvider::HttpProvider(QNetworkAccessManager *nam, QObject *parent)
    : TranslationProvider(parent)
    , m_nam(nam ? nam : new QNetworkAccessManager(this))
    , m_quality(QStringLiteral("balanced"))
{
    for (Operation *op : {&m_translateOp, &m_listOp}) {
        op->retryTimer = new QTimer(this);
        op->retryTimer->setSingleShot(true);
        connect(op->retryTimer, &QTimer::timeout, this, [this, op] { sendAttempt(op); });
    }
}

HttpProvider::~HttpProvider()
{
    abort(&m_translateOp);
    abort(&m_listOp);
}

void HttpProvider::setApiKey(const QString &key)
{
    m_apiKey = key.trimmed();
}

void HttpProvider::setModel(const QString &model)
{
    const QString m = model.trimmed();
    m_model = m.isEmpty() ? defaultModel() : m;
}

void HttpProvider::setQuality(const QString &quality)
{
    const QString q = quality.trimmed().toLower();
    if (q == QLatin1String("fast") || q == QLatin1String("balanced") || q == QLatin1String("best"))
        m_quality = q;
    else
        m_quality = QStringLiteral("balanced");
}

bool HttpProvider::isConfigured() const
{
    return !m_apiKey.isEmpty() && !m_model.isEmpty();
}

void HttpProvider::setBaseUrl(const QUrl &url)
{
    m_baseUrl = url;
}

QUrl HttpProvider::baseUrl() const
{
    return m_baseUrl.isEmpty() ? defaultBaseUrl() : m_baseUrl;
}

void HttpProvider::setTransferTimeout(int msecs)
{
    m_transferTimeoutMs = qMax(0, msecs);
}

void HttpProvider::setRetryDelays(const QList<int> &delaysMs)
{
    m_retryDelaysMs = delaysMs;
}

void HttpProvider::setMaxRetryAfter(int msecs)
{
    m_maxRetryAfterMs = qMax(0, msecs);
}

int HttpProvider::fallbackVariant(const HttpResult &, int) const
{
    return -1;
}

void HttpProvider::translate(const TranslationRequest &request)
{
    if (m_translating)
        cancel();  // emits failed(Cancelled) for the previous request

    m_translating = true;
    m_request = request;
    m_variant = 0;

    if (!isConfigured()) {
        abort(&m_translateOp);
        const quint64 token = m_translateOp.token;
        QTimer::singleShot(0, this, [this, token] {
            if (!m_translating || token != m_translateOp.token)
                return;
            finishTranslate(ParsedTranslation::failure(
                ErrorKind::NotConfigured,
                QStringLiteral("Add your %1 API key in Settings to start translating.").arg(shortName())));
        });
        return;
    }

    start(&m_translateOp, translateCall(request, m_variant), [this](const HttpResult &r) { onTranslateDone(r); });
}

void HttpProvider::cancel()
{
    if (!m_translating)
        return;
    abort(&m_translateOp);
    m_translating = false;
    TranslationError e;
    e.kind = ErrorKind::Cancelled;
    e.message = QStringLiteral("Translation cancelled.");
    emit failed(e);
}

void HttpProvider::listModels(const QString &apiKeyOverride)
{
    const QString key = apiKeyOverride.trimmed().isEmpty() ? m_apiKey : apiKeyOverride.trimmed();
    if (key.isEmpty()) {
        abort(&m_listOp);
        const quint64 token = m_listOp.token;
        QTimer::singleShot(0, this, [this, token] {
            if (token != m_listOp.token)
                return;
            TranslationError e;
            e.kind = ErrorKind::NotConfigured;
            e.message = QStringLiteral("Enter your %1 API key first.").arg(shortName());
            emit modelsListFailed(e);
        });
        return;
    }

    start(&m_listOp, listModelsCall(key), [this](const HttpResult &r) {
        if (!r.isSuccess()) {
            emit modelsListFailed(errorFor(r));
            return;
        }
        QString detail;
        const QStringList models = parseModelList(r.body, &detail);
        if (!detail.isEmpty()) {
            TranslationError e;
            e.kind = ErrorKind::BadResponse;
            e.message = QStringLiteral("%1 returned a model list the app couldn't read.").arg(shortName());
            e.detail = detail;
            e.httpStatus = r.status;
            emit modelsListFailed(e);
            return;
        }
        emit modelsListed(models);
    });
}

void HttpProvider::onTranslateDone(const HttpResult &result)
{
    if (!result.isSuccess()) {
        const int next = fallbackVariant(result, m_variant);
        if (next >= 0 && next != m_variant) {
            m_variant = next;
            start(&m_translateOp, translateCall(m_request, m_variant),
                  [this](const HttpResult &r) { onTranslateDone(r); });
            return;
        }
        ParsedTranslation outcome;
        outcome.error = errorFor(result);
        finishTranslate(outcome);
        return;
    }

    ParsedTranslation outcome = parseTranslateResponse(result.body, m_request);
    if (outcome.ok) {
        outcome.result.providerId = id();
        if (outcome.result.model.isEmpty())
            outcome.result.model = m_model;
        outcome.result.request = m_request;
        outcome.result.timestamp = QDateTime::currentDateTimeUtc();
        outcome.result.fromCache = false;
    } else if (outcome.error.httpStatus == 0) {
        outcome.error.httpStatus = result.status;
    }
    finishTranslate(outcome);
}

void HttpProvider::finishTranslate(const ParsedTranslation &outcome)
{
    m_translating = false;
    if (outcome.ok)
        emit finished(outcome.result);
    else
        emit failed(outcome.error);
}

// ---- request plumbing -------------------------------------------------------

QUrl HttpProvider::urlFor(const QString &path) const
{
    QString base = baseUrl().toString();
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    return QUrl(base + path);
}

void HttpProvider::start(Operation *op, const HttpCall &call, std::function<void(const HttpResult &)> onDone)
{
    abort(op);
    op->call = call;
    op->retriesDone = 0;
    op->onDone = std::move(onDone);
    sendAttempt(op);
}

void HttpProvider::sendAttempt(Operation *op)
{
    QNetworkRequest request(urlFor(op->call.path));
    for (const auto &header : std::as_const(op->call.headers))
        request.setRawHeader(header.first, header.second);
    if (m_transferTimeoutMs > 0) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        request.setTransferTimeout(std::chrono::milliseconds(m_transferTimeoutMs));
#else
        request.setTransferTimeout(m_transferTimeoutMs);
#endif
    }

    QNetworkReply *reply = nullptr;
    if (op->call.verb == "GET")
        reply = m_nam->get(request);
    else if (op->call.verb == "POST")
        reply = m_nam->post(request, op->call.body);
    else
        reply = m_nam->sendCustomRequest(request, op->call.verb, op->call.body);

    op->reply = reply;
    const quint64 token = op->token;
    connect(reply, &QNetworkReply::finished, this, [this, op, reply, token] { onReplyFinished(op, reply, token); });
}

void HttpProvider::onReplyFinished(Operation *op, QNetworkReply *reply, quint64 token)
{
    reply->deleteLater();
    if (token != op->token || op->reply != reply)
        return;
    op->reply.clear();

    HttpResult r;
    const QVariant statusAttr = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    r.status = statusAttr.isValid() ? statusAttr.toInt() : 0;
    r.body = reply->readAll();
    r.networkError = reply->error();
    r.errorString = reply->errorString();
    // We never abort a reply without disconnecting first, so a cancelled
    // operation reaching this point means the transfer timeout fired.
    r.timedOut = r.networkError == QNetworkReply::OperationCanceledError
                 || r.networkError == QNetworkReply::TimeoutError;
    r.retryAfterMs = parseRetryAfterMs(reply->rawHeader("retry-after"), reply->rawHeader("retry-after-ms"));

    if (!r.isSuccess() && op->retriesDone < m_retryDelaysMs.size() && shouldRetry(r)) {
        int delay = m_retryDelaysMs.at(op->retriesDone);
        bool retry = true;
        if (r.retryAfterMs >= 0) {
            if (r.retryAfterMs > m_maxRetryAfterMs)
                retry = false;  // too long to wait silently; report it instead
            else
                delay = r.retryAfterMs;
        }
        if (retry) {
            ++op->retriesDone;
            op->retryTimer->start(qMax(0, delay));
            return;
        }
    }

    // Copy: the callback may restart this operation and replace onDone.
    const auto done = op->onDone;
    if (done)
        done(r);
}

void HttpProvider::abort(Operation *op)
{
    ++op->token;
    if (op->retryTimer)
        op->retryTimer->stop();
    if (QNetworkReply *reply = op->reply.data()) {
        op->reply.clear();
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
}

} // namespace sct
