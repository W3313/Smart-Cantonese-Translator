#pragma once

#include "core/TranslationTypes.h"

#include <QHash>
#include <QObject>
#include <QStringList>

class QNetworkAccessManager;

namespace sct {

class AppSettings;
class HistoryStore;
class TranslationProvider;

// Facade the UI talks to. Owns the providers, a small in-memory LRU result
// cache and the HistoryStore. Reads configuration from AppSettings and
// re-applies it whenever AppSettings::changed() fires.
class TranslationService : public QObject
{
    Q_OBJECT

public:
    explicit TranslationService(AppSettings *settings, QObject *parent = nullptr);
    ~TranslationService() override;

    // Starts a translation with the active provider. Serves from cache when an
    // identical request (same text/direction/tone/script/provider/model) was
    // already answered this session. Empty/whitespace text is ignored.
    void translate(const TranslationRequest &request);
    void cancel();
    bool isBusy() const;

    QString activeProviderId() const;
    bool isActiveProviderConfigured() const;
    QString activeModel() const;

    // Provider metadata for the Settings dialog.
    QStringList providerIds() const;  // {"claude", "openai"}
    QString providerDisplayName(const QString &providerId) const;
    QStringList suggestedModels(const QString &providerId) const;
    QString defaultModel(const QString &providerId) const;

    // Lists models for a provider using the given key (or the saved key when
    // empty). Doubles as "Test connection". Emits modelsListed/modelsListFailed.
    void listModels(const QString &providerId, const QString &apiKeyOverride = QString());

    HistoryStore *history() const { return m_history; }
    QNetworkAccessManager *networkManager() const { return m_nam; }

public slots:
    void reloadSettings();

signals:
    void started(const sct::TranslationRequest &request);
    void finished(const sct::TranslationResult &result);
    void failed(const sct::TranslationError &error);
    void busyChanged(bool busy);
    void modelsListed(const QString &providerId, const QStringList &models);
    void modelsListFailed(const QString &providerId, const sct::TranslationError &error);

private:
    TranslationProvider *provider(const QString &providerId) const;
    TranslationProvider *activeProvider() const;
    void connectProvider(TranslationProvider *p);
    void setBusy(bool busy);
    QString fullCacheKey(const TranslationRequest &request) const;
    void remember(const QString &key, const TranslationResult &result);

    AppSettings *m_settings = nullptr;
    QNetworkAccessManager *m_nam = nullptr;
    HistoryStore *m_history = nullptr;
    QHash<QString, TranslationProvider *> m_providers;
    QHash<QString, TranslationResult> m_cache;
    QStringList m_cacheOrder;  // LRU order, most recent last
    QString m_pendingCacheKey;
    bool m_busy = false;
};

} // namespace sct
