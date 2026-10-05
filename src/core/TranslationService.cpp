#include "core/TranslationService.h"

#include "core/AppSettings.h"
#include "core/ClaudeProvider.h"
#include "core/HistoryStore.h"
#include "core/OpenAIProvider.h"
#include "core/ProviderDefaults.h"
#include "core/TranslationProvider.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QTimer>

namespace sct {

namespace {

constexpr int kCacheCapacity = 100;
constexpr int kHistoryLimit = 500;

QString shortProviderName(const QString &providerId)
{
    if (providerId == ProviderDefaults::openAiProviderId())
        return QStringLiteral("OpenAI");
    if (providerId == ProviderDefaults::claudeProviderId())
        return QStringLiteral("Claude");
    return providerId;
}

} // namespace

TranslationService::TranslationService(AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    registerMetaTypes();

    m_nam = new QNetworkAccessManager(this);

    TranslationProvider *claude = new ClaudeProvider(m_nam, this);
    TranslationProvider *openAi = new OpenAIProvider(m_nam, this);
    for (TranslationProvider *p : {claude, openAi}) {
        m_providers.insert(p->id(), p);
        connectProvider(p);
    }

    const QString dataDir = m_settings ? m_settings->dataDirectory() : QString();
    const QString historyPath = dataDir.isEmpty() ? QStringLiteral("history.json")
                                                  : dataDir + QStringLiteral("/history.json");
    m_history = new HistoryStore(historyPath, kHistoryLimit, this);
    m_history->load();

    if (m_settings)
        connect(m_settings, &AppSettings::changed, this, &TranslationService::reloadSettings);
    reloadSettings();
}

TranslationService::~TranslationService()
{
    // Delete the providers (which abort their requests silently) before the
    // network manager they use; nothing may call back into this object.
    const QList<TranslationProvider *> providers = m_providers.values();
    m_providers.clear();
    for (TranslationProvider *p : providers) {
        p->disconnect(this);
        delete p;
    }
}

// ---- translation ------------------------------------------------------------

void TranslationService::translate(const TranslationRequest &request)
{
    if (request.text.trimmed().isEmpty())
        return;

    cancelInFlight(true);  // superseded request: no Cancelled signal
    const quint64 generation = ++m_generation;

    TranslationProvider *p = activeProvider();
    if (!p || !p->isConfigured()) {
        setBusy(false);
        TranslationError e;
        e.kind = ErrorKind::NotConfigured;
        e.message = QStringLiteral("Add your %1 API key in Settings to start translating.")
                        .arg(shortProviderName(activeProviderId()));
        deliverLater([this, generation, e] {
            if (generation == m_generation)
                emit failed(e);
        });
        return;
    }

    const QString key = fullCacheKey(request);
    const auto cached = m_cache.constFind(key);
    if (cached != m_cache.constEnd()) {
        setBusy(false);
        TranslationResult result = cached.value();
        result.request = request;
        result.fromCache = true;
        result.timestamp = QDateTime::currentDateTimeUtc();
        m_cacheOrder.removeAll(key);
        m_cacheOrder.append(key);
        deliverLater([this, generation, request, result] {
            if (generation != m_generation)
                return;
            emit started(request);
            m_history->add(result);
            emit finished(result);
        });
        return;
    }

    m_pendingRequest = request;
    m_pendingCacheKey = key;
    setBusy(true);
    emit started(request);
    p->translate(request);
}

void TranslationService::cancel()
{
    ++m_generation;  // drops queued cache hits / NotConfigured errors
    cancelInFlight(false);
    m_pendingCacheKey.clear();
    setBusy(false);
}

bool TranslationService::isBusy() const
{
    return m_busy;
}

void TranslationService::cancelInFlight(bool silently)
{
    const bool previous = m_suppressCancelled;
    m_suppressCancelled = silently;
    for (TranslationProvider *p : std::as_const(m_providers)) {
        if (p->isBusy())
            p->cancel();
    }
    m_suppressCancelled = previous;
}

void TranslationService::onProviderFinished(TranslationProvider *p, const TranslationResult &result)
{
    if (!m_busy)
        return;  // stale: not waiting for anything

    TranslationResult r = result;
    r.providerId = p->id();
    if (r.model.isEmpty())
        r.model = p->model();
    r.request = m_pendingRequest;
    if (!r.timestamp.isValid())
        r.timestamp = QDateTime::currentDateTimeUtc();
    r.fromCache = false;

    remember(m_pendingCacheKey, r);
    m_pendingCacheKey.clear();
    m_history->add(r);
    setBusy(false);  // before finished(), so a slot may start a new translation
    emit finished(r);
}

void TranslationService::onProviderFailed(TranslationProvider *, const TranslationError &error)
{
    if (error.kind == ErrorKind::Cancelled && m_suppressCancelled)
        return;
    if (!m_busy)
        return;
    m_pendingCacheKey.clear();
    setBusy(false);
    emit failed(error);
}

void TranslationService::deliverLater(const std::function<void()> &fn)
{
    QTimer::singleShot(0, this, fn);
}

void TranslationService::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged(busy);
}

QString TranslationService::fullCacheKey(const TranslationRequest &request) const
{
    return activeProviderId() + QLatin1Char('|') + activeModel() + QLatin1Char('|') + request.cacheKey();
}

void TranslationService::remember(const QString &key, const TranslationResult &result)
{
    if (key.isEmpty())
        return;
    TranslationResult stored = result;
    stored.fromCache = false;
    m_cache.insert(key, stored);
    m_cacheOrder.removeAll(key);
    m_cacheOrder.append(key);
    while (m_cacheOrder.size() > kCacheCapacity)
        m_cache.remove(m_cacheOrder.takeFirst());
}

// ---- providers & settings ---------------------------------------------------------

QString TranslationService::activeProviderId() const
{
    const QString id = m_settings ? m_settings->aiProvider() : ProviderDefaults::claudeProviderId();
    return m_providers.contains(id) ? id : ProviderDefaults::claudeProviderId();
}

bool TranslationService::isActiveProviderConfigured() const
{
    const TranslationProvider *p = activeProvider();
    return p && p->isConfigured();
}

QString TranslationService::activeModel() const
{
    const TranslationProvider *p = activeProvider();
    return p ? p->model() : QString();
}

QStringList TranslationService::providerIds() const
{
    return {ProviderDefaults::claudeProviderId(), ProviderDefaults::openAiProviderId()};
}

QString TranslationService::providerDisplayName(const QString &providerId) const
{
    const TranslationProvider *p = provider(providerId);
    return p ? p->displayName() : QString();
}

QStringList TranslationService::suggestedModels(const QString &providerId) const
{
    const TranslationProvider *p = provider(providerId);
    return p ? p->suggestedModels() : QStringList();
}

QString TranslationService::defaultModel(const QString &providerId) const
{
    const TranslationProvider *p = provider(providerId);
    return p ? p->defaultModel() : QString();
}

void TranslationService::listModels(const QString &providerId, const QString &apiKeyOverride)
{
    TranslationProvider *p = provider(providerId);
    if (!p) {
        TranslationError e;
        e.kind = ErrorKind::InvalidRequest;
        e.message = QStringLiteral("Unknown AI provider \"%1\".").arg(providerId);
        deliverLater([this, providerId, e] { emit modelsListFailed(providerId, e); });
        return;
    }
    p->listModels(apiKeyOverride);
}

void TranslationService::reloadSettings()
{
    if (!m_settings)
        return;
    if (TranslationProvider *p = provider(ProviderDefaults::claudeProviderId())) {
        p->setApiKey(m_settings->claudeApiKey());
        p->setModel(m_settings->claudeModel());
        p->setQuality(m_settings->quality());
    }
    if (TranslationProvider *p = provider(ProviderDefaults::openAiProviderId())) {
        p->setApiKey(m_settings->openAiApiKey());
        p->setModel(m_settings->openAiModel());
        p->setQuality(m_settings->quality());
    }
}

void TranslationService::setProvider(TranslationProvider *provider)
{
    if (!provider)
        return;
    const QString id = provider->id();
    TranslationProvider *old = m_providers.value(id);
    if (old == provider)
        return;
    if (old) {
        if (old->isBusy()) {
            const bool previous = m_suppressCancelled;
            m_suppressCancelled = true;
            old->cancel();
            m_suppressCancelled = previous;
            m_pendingCacheKey.clear();
            setBusy(false);
        }
        old->disconnect(this);
        old->deleteLater();
    }
    provider->setParent(this);
    m_providers.insert(id, provider);
    connectProvider(provider);
    reloadSettings();
}

TranslationProvider *TranslationService::provider(const QString &providerId) const
{
    return m_providers.value(providerId, nullptr);
}

TranslationProvider *TranslationService::activeProvider() const
{
    return provider(activeProviderId());
}

void TranslationService::connectProvider(TranslationProvider *p)
{
    connect(p, &TranslationProvider::finished, this,
            [this, p](const TranslationResult &r) { onProviderFinished(p, r); });
    connect(p, &TranslationProvider::failed, this,
            [this, p](const TranslationError &e) { onProviderFailed(p, e); });
    connect(p, &TranslationProvider::modelsListed, this,
            [this, p](const QStringList &models) { emit modelsListed(p->id(), models); });
    connect(p, &TranslationProvider::modelsListFailed, this,
            [this, p](const TranslationError &e) { emit modelsListFailed(p->id(), e); });
}

} // namespace sct
