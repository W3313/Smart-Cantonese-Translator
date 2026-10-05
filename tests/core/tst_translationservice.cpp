#include "core/AppSettings.h"
#include "core/HistoryStore.h"
#include "core/TranslationProvider.h"
#include "core/TranslationService.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace sct;

// Scriptable provider: the test decides when and how a request completes.
class FakeProvider : public TranslationProvider
{
    Q_OBJECT

public:
    explicit FakeProvider(const QString &id, QObject *parent = nullptr)
        : TranslationProvider(parent)
        , m_id(id)
    {
    }

    QString id() const override { return m_id; }
    QString displayName() const override { return QStringLiteral("Fake ") + m_id; }
    void setApiKey(const QString &key) override { apiKey = key; }
    void setModel(const QString &m) override { modelId = m.isEmpty() ? QStringLiteral("fake-model") : m; }
    QString model() const override { return modelId; }
    void setQuality(const QString &q) override { quality = q; }
    bool isConfigured() const override { return !apiKey.isEmpty(); }
    bool isBusy() const override { return busy; }
    void translate(const TranslationRequest &request) override
    {
        if (busy)
            cancel();
        busy = true;
        last = request;
        ++translateCalls;
    }
    void cancel() override
    {
        if (!busy)
            return;
        busy = false;
        TranslationError e;
        e.kind = ErrorKind::Cancelled;
        emit failed(e);
    }
    void listModels(const QString &keyOverride) override
    {
        lastListKey = keyOverride;
        emit modelsListed({QStringLiteral("fake-a"), QStringLiteral("fake-b")});
    }
    QStringList suggestedModels() const override { return {QStringLiteral("fake-model")}; }
    QString defaultModel() const override { return QStringLiteral("fake-model"); }

    void complete(const QString &translation)
    {
        busy = false;
        TranslationResult r;
        r.translation = translation;
        emit finished(r);
    }
    void fail(ErrorKind kind)
    {
        busy = false;
        TranslationError e;
        e.kind = kind;
        e.message = QStringLiteral("fake failure");
        emit failed(e);
    }

    QString m_id;
    QString apiKey;
    QString modelId = QStringLiteral("fake-model");
    QString quality;
    bool busy = false;
    int translateCalls = 0;
    TranslationRequest last;
    QString lastListKey;
};

class TstTranslationService : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<AppSettings> m_settings;
    std::unique_ptr<TranslationService> m_service;
    FakeProvider *m_fake = nullptr;

    static TranslationRequest req(const QString &text, Tone tone = Tone::Neutral)
    {
        TranslationRequest r;
        r.text = text;
        r.tone = tone;
        return r;
    }

private slots:
    void initTestCase() { registerMetaTypes(); }

    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_settings = std::make_unique<AppSettings>(m_dir->filePath(QStringLiteral("settings.ini")));
        m_service = std::make_unique<TranslationService>(m_settings.get());
        m_fake = new FakeProvider(QStringLiteral("claude"));
        m_service->setProvider(m_fake);
    }

    void cleanup()
    {
        m_service.reset();
        m_settings.reset();
        m_dir.reset();
        m_fake = nullptr;
    }

    void metadata()
    {
        TranslationService plain(m_settings.get());
        QCOMPARE(plain.providerIds(), (QStringList{QStringLiteral("claude"), QStringLiteral("openai")}));
        QCOMPARE(plain.defaultModel(QStringLiteral("claude")), QStringLiteral("claude-opus-5-5"));
        QCOMPARE(plain.defaultModel(QStringLiteral("openai")), m_settings->openAiModel());
        QCOMPARE(plain.providerDisplayName(QStringLiteral("claude")), QStringLiteral("Claude (Anthropic)"));
        QCOMPARE(plain.providerDisplayName(QStringLiteral("openai")), QStringLiteral("OpenAI"));
        QVERIFY(plain.suggestedModels(QStringLiteral("claude")).contains(QStringLiteral("claude-opus-5-5")));
        QVERIFY(plain.suggestedModels(QStringLiteral("nope")).isEmpty());
        QCOMPARE(plain.activeProviderId(), QStringLiteral("claude"));
        QCOMPARE(plain.activeModel(), QStringLiteral("claude-opus-5-5"));
        QVERIFY(!plain.isActiveProviderConfigured());
        QVERIFY(plain.networkManager());
        QVERIFY(plain.history());
        QCOMPARE(plain.history()->filePath(), m_settings->dataDirectory() + QStringLiteral("/history.json"));
    }

    void settingsAreApplied()
    {
        QVERIFY(m_fake->apiKey.isEmpty());
        m_settings->beginBatch();
        m_settings->setClaudeApiKey(QStringLiteral("sk-ant-1"));
        m_settings->setClaudeModel(QStringLiteral("claude-sonnet-5-5"));
        m_settings->setQuality(QStringLiteral("fast"));
        m_settings->endBatch();
        QCOMPARE(m_fake->apiKey, QStringLiteral("sk-ant-1"));
        QCOMPARE(m_fake->modelId, QStringLiteral("claude-sonnet-5-5"));
        QCOMPARE(m_fake->quality, QStringLiteral("fast"));
        QVERIFY(m_service->isActiveProviderConfigured());
        QCOMPARE(m_service->activeModel(), QStringLiteral("claude-sonnet-5-5"));

        m_settings->setAiProvider(QStringLiteral("openai"));
        QCOMPARE(m_service->activeProviderId(), QStringLiteral("openai"));
        QVERIFY(!m_service->isActiveProviderConfigured());
    }

    void notConfigured()
    {
        QSignalSpy failed(m_service.get(), &TranslationService::failed);
        QSignalSpy started(m_service.get(), &TranslationService::started);
        QSignalSpy busy(m_service.get(), &TranslationService::busyChanged);
        m_service->translate(req(QStringLiteral("Hello")));
        QCOMPARE(failed.count(), 0);  // delivered asynchronously
        QVERIFY(failed.wait(2000));
        const TranslationError e = failed.first().first().value<TranslationError>();
        QCOMPARE(e.kind, ErrorKind::NotConfigured);
        QCOMPARE(e.message, QStringLiteral("Add your Claude API key in Settings to start translating."));
        QCOMPARE(started.count(), 0);
        QCOMPARE(busy.count(), 0);
        QCOMPARE(m_fake->translateCalls, 0);

        m_settings->setAiProvider(QStringLiteral("openai"));
        failed.clear();
        m_service->translate(req(QStringLiteral("Hello")));
        QVERIFY(failed.wait(2000));
        QVERIFY(failed.first().first().value<TranslationError>().message.contains(QStringLiteral("OpenAI")));
    }

    void blankTextIgnored()
    {
        m_settings->setClaudeApiKey(QStringLiteral("k"));
        QSignalSpy failed(m_service.get(), &TranslationService::failed);
        QSignalSpy started(m_service.get(), &TranslationService::started);
        m_service->translate(req(QStringLiteral("  \n\t ")));
        QTest::qWait(20);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(started.count(), 0);
        QCOMPARE(m_fake->translateCalls, 0);
    }

    void translateAndCache()
    {
        m_settings->setClaudeApiKey(QStringLiteral("k"));
        QSignalSpy started(m_service.get(), &TranslationService::started);
        QSignalSpy finished(m_service.get(), &TranslationService::finished);
        QSignalSpy busy(m_service.get(), &TranslationService::busyChanged);

        m_service->translate(req(QStringLiteral("Where is he?")));
        QCOMPARE(started.count(), 1);
        QVERIFY(m_service->isBusy());
        QCOMPARE(busy.count(), 1);
        QCOMPARE(m_fake->translateCalls, 1);
        QCOMPARE(m_fake->last.text, QStringLiteral("Where is he?"));

        m_fake->complete(QStringLiteral("佢喺邊度呀？"));
        QCOMPARE(finished.count(), 1);
        QVERIFY(!m_service->isBusy());
        QCOMPARE(busy.count(), 2);
        const TranslationResult r = finished.first().first().value<TranslationResult>();
        QCOMPARE(r.translation, QStringLiteral("佢喺邊度呀？"));
        QCOMPARE(r.providerId, QStringLiteral("claude"));
        QCOMPARE(r.model, QStringLiteral("claude-opus-5-5"));
        QCOMPARE(r.request.text, QStringLiteral("Where is he?"));
        QVERIFY(r.timestamp.isValid());
        QVERIFY(!r.fromCache);
        QCOMPARE(m_service->history()->entries().size(), 1);

        // Same request again: served from the cache, asynchronously.
        m_service->translate(req(QStringLiteral(" Where is he? ")));
        QCOMPARE(m_fake->translateCalls, 1);
        QCOMPARE(finished.count(), 1);
        QVERIFY(finished.wait(2000));
        QCOMPARE(started.count(), 2);
        const TranslationResult cached = finished.at(1).first().value<TranslationResult>();
        QVERIFY(cached.fromCache);
        QCOMPARE(cached.translation, QStringLiteral("佢喺邊度呀？"));
        QCOMPARE(cached.request.text, QStringLiteral(" Where is he? "));
        QCOMPARE(busy.count(), 2);  // no busy flicker for cache hits
        QCOMPARE(m_service->history()->entries().size(), 1);  // deduplicated

        // A different tone is a different request.
        m_service->translate(req(QStringLiteral("Where is he?"), Tone::Casual));
        QCOMPARE(m_fake->translateCalls, 2);
        m_fake->complete(QStringLiteral("佢喺邊呀？"));
        QCOMPARE(finished.count(), 3);

        // Switching model changes the cache key too.
        m_settings->setClaudeModel(QStringLiteral("claude-haiku-4-5"));
        m_service->translate(req(QStringLiteral("Where is he?")));
        QCOMPARE(m_fake->translateCalls, 3);
        m_fake->complete(QStringLiteral("佢喺邊度？"));

        // So does quality: asking for "best" must not replay the earlier answer.
        m_settings->setQuality(QStringLiteral("best"));
        m_service->translate(req(QStringLiteral("Where is he?")));
        QCOMPARE(m_fake->translateCalls, 4);
        m_fake->complete(QStringLiteral("佢而家喺邊度呀？"));
        QCOMPARE(finished.count(), 5);
        QVERIFY(!finished.last().first().value<TranslationResult>().fromCache);
    }

    void cancelForwardsCancelled()
    {
        m_settings->setClaudeApiKey(QStringLiteral("k"));
        QSignalSpy failed(m_service.get(), &TranslationService::failed);
        m_service->translate(req(QStringLiteral("Hello")));
        QVERIFY(m_service->isBusy());
        m_service->cancel();
        QCOMPARE(failed.count(), 1);
        QCOMPARE(failed.first().first().value<TranslationError>().kind, ErrorKind::Cancelled);
        QVERIFY(!m_service->isBusy());
        QVERIFY(!m_fake->isBusy());
        m_service->cancel();  // nothing in flight
        QCOMPARE(failed.count(), 1);
    }

    void supersededRequestIsSilent()
    {
        m_settings->setClaudeApiKey(QStringLiteral("k"));
        QSignalSpy failed(m_service.get(), &TranslationService::failed);
        QSignalSpy finished(m_service.get(), &TranslationService::finished);
        QSignalSpy busy(m_service.get(), &TranslationService::busyChanged);
        m_service->translate(req(QStringLiteral("first")));
        m_service->translate(req(QStringLiteral("second")));
        QCOMPARE(failed.count(), 0);  // the superseded request does not report Cancelled
        QCOMPARE(busy.count(), 1);    // stayed busy throughout
        QCOMPARE(m_fake->translateCalls, 2);
        m_fake->complete(QStringLiteral("第二"));
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first().first().value<TranslationResult>().request.text, QStringLiteral("second"));
    }

    void cancelDropsPendingCacheHit()
    {
        m_settings->setClaudeApiKey(QStringLiteral("k"));
        QSignalSpy finished(m_service.get(), &TranslationService::finished);
        m_service->translate(req(QStringLiteral("Hello")));
        m_fake->complete(QStringLiteral("你好"));
        QCOMPARE(finished.count(), 1);
        m_service->translate(req(QStringLiteral("Hello")));  // cache hit, queued
        m_service->cancel();
        QTest::qWait(30);
        QCOMPARE(finished.count(), 1);
    }

    void providerErrorsAreForwarded()
    {
        m_settings->setClaudeApiKey(QStringLiteral("k"));
        QSignalSpy failed(m_service.get(), &TranslationService::failed);
        m_service->translate(req(QStringLiteral("Hello")));
        m_fake->fail(ErrorKind::RateLimited);
        QCOMPARE(failed.count(), 1);
        QCOMPARE(failed.first().first().value<TranslationError>().kind, ErrorKind::RateLimited);
        QVERIFY(!m_service->isBusy());
        QVERIFY(m_service->history()->entries().isEmpty());

        // Failures are not cached: the next attempt reaches the provider.
        m_service->translate(req(QStringLiteral("Hello")));
        QCOMPARE(m_fake->translateCalls, 2);
    }

    void listModelsForwarded()
    {
        QSignalSpy listed(m_service.get(), &TranslationService::modelsListed);
        QSignalSpy listFailed(m_service.get(), &TranslationService::modelsListFailed);
        m_service->listModels(QStringLiteral("claude"), QStringLiteral("test-key"));
        QCOMPARE(listed.count(), 1);
        QCOMPARE(listed.first().at(0).toString(), QStringLiteral("claude"));
        QCOMPARE(listed.first().at(1).toStringList(), (QStringList{QStringLiteral("fake-a"), QStringLiteral("fake-b")}));
        QCOMPARE(m_fake->lastListKey, QStringLiteral("test-key"));

        m_service->listModels(QStringLiteral("bogus"));
        QVERIFY(listFailed.wait(2000));
        QCOMPARE(listFailed.first().at(0).toString(), QStringLiteral("bogus"));
    }

    void historyLoadedOnStartup()
    {
        m_settings->setClaudeApiKey(QStringLiteral("k"));
        m_service->translate(req(QStringLiteral("Hello")));
        m_fake->complete(QStringLiteral("你好"));
        m_service.reset();

        TranslationService again(m_settings.get());
        QCOMPARE(again.history()->entries().size(), 1);
        QCOMPARE(again.history()->entries().first().result.translation, QStringLiteral("你好"));
    }
};

QTEST_GUILESS_MAIN(TstTranslationService)
#include "tst_translationservice.moc"
