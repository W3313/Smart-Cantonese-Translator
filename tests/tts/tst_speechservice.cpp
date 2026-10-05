#include "FakeAzureServer.h"

#include "core/AppSettings.h"
#include "tts/AzureSpeechEngine.h"
#include "tts/SpeechService.h"
#include "tts/SystemSpeechEngine.h"

#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextToSpeech>
#include <QtTest>

#include <memory>

using namespace sct;

namespace {

constexpr int WaitMs = 15000;

bool noSystemBackend()
{
    return QTextToSpeech::availableEngines().isEmpty();
}

bool systemHasVoice(const SpeechService &service, Language lang)
{
    return !service.voices(QStringLiteral("system"), lang).isEmpty();
}

} // namespace

class TestSpeechService : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_tmp;
    std::unique_ptr<AppSettings> m_settings;

    AzureSpeechEngine *azureOf(SpeechService &service)
    {
        return service.findChild<AzureSpeechEngine *>();
    }

    void configureAzure(const QString &engineId = QStringLiteral("azure"))
    {
        m_settings->beginBatch();
        m_settings->setAzureKey(QStringLiteral("test-key"));
        m_settings->setAzureRegion(QStringLiteral("eastasia"));
        m_settings->setSpeechEngine(engineId);
        m_settings->endBatch();
    }

private slots:
    void initTestCase()
    {
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        // Qt 6.4's GStreamer backend can crash on CI machines with an audio
        // device but incomplete GStreamer plugins; FFmpeg is Qt 6.5+'s default.
        if (qEnvironmentVariableIsEmpty("QT_MEDIA_BACKEND"))
            qputenv("QT_MEDIA_BACKEND", "ffmpeg");
    }

    void init()
    {
        m_tmp = std::make_unique<QTemporaryDir>();
        QVERIFY(m_tmp->isValid());
        m_settings = std::make_unique<AppSettings>(m_tmp->filePath(QStringLiteral("settings.ini")));
    }

    void cleanup()
    {
        m_settings.reset();
        m_tmp.reset();
    }

    void constructsWithoutBackend()
    {
        SpeechService service(m_settings.get(), nullptr);
        QCOMPARE(service.engineIds(), QStringList({QStringLiteral("system"), QStringLiteral("azure")}));
        QVERIFY(!service.engineDisplayName(QStringLiteral("system")).isEmpty());
        QVERIFY(service.engineDisplayName(QStringLiteral("azure")).contains(QStringLiteral("Azure")));
        QVERIFY(service.engineDisplayName(QStringLiteral("nope")).isEmpty());
        QCOMPARE(service.voices(QStringLiteral("azure"), Language::Cantonese).size(), 3);
        QVERIFY(service.voices(QStringLiteral("nope"), Language::English).isEmpty());
        QVERIFY(!service.isSpeaking());
        QVERIFY(!service.isLoading());
        QCOMPARE(service.effectiveEngineId(Language::English), QStringLiteral("system"));
        QVERIFY(service.cantoneseVoiceHelpText().contains(QStringLiteral("Hong Kong SAR")));

        auto *azure = azureOf(service);
        QVERIFY(azure);
        QCOMPARE(azure->cacheDirectory(),
                 QDir(m_settings->dataDirectory()).filePath(QStringLiteral("audio-cache")));

        service.refreshVoices();  // must not crash, with or without a backend
        // Without a Cantonese system voice and without Azure, Cantonese cannot be read.
        QCOMPARE(service.canSpeak(Language::Cantonese), systemHasVoice(service, Language::Cantonese));
        QCOMPARE(service.canSpeak(Language::English), systemHasVoice(service, Language::English));
        if (noSystemBackend()) {
            QVERIFY(!service.canSpeak(Language::Cantonese));
            QVERIFY(!service.canSpeak(Language::English));
        }
    }

    void nullSettingsAreTolerated()
    {
        SpeechService service(nullptr, nullptr);
        service.reloadSettings();
        QCOMPARE(service.effectiveEngineId(Language::Cantonese), QStringLiteral("system"));
    }

    void speakWithoutVoiceReportsHelp()
    {
        SpeechService service(m_settings.get(), nullptr);
        if (systemHasVoice(service, Language::Cantonese))
            QSKIP("A Cantonese system voice is installed.");
        QSignalSpy errors(&service, &SpeechService::errorOccurred);
        QSignalSpy speaking(&service, &SpeechService::speakingChanged);

        // Never falls back to a Mandarin / English voice: explains how to get one.
        service.speak(QStringLiteral("你好"), Language::Cantonese);
        QCOMPARE(errors.size(), 1);
        QCOMPARE(errors.takeFirst().first().toString(), SpeechService::cantoneseVoiceHelpText());
        QVERIFY(!service.isSpeaking());

        service.speak(QStringLiteral("   "), Language::English);  // nothing to read
        QCOMPARE(errors.size(), 0);
        QCOMPARE(speaking.size(), 0);  // never started
        service.stop();
        QCOMPARE(speaking.size(), 0);

        if (noSystemBackend()) {
            service.speak(QStringLiteral("Hello"), Language::English);
            QCOMPARE(errors.size(), 1);
            QVERIFY(!errors.takeFirst().first().toString().isEmpty());
            QCOMPARE(speaking.size(), 0);
        }
    }

    void azureSettingsAreApplied()
    {
        SpeechService service(m_settings.get(), nullptr);
        QSignalSpy voicesChanged(&service, &SpeechService::voicesChanged);
        configureAzure();
        QVERIFY(voicesChanged.size() >= 1);
        QVERIFY(service.canSpeak(Language::Cantonese));
        QVERIFY(service.canSpeak(Language::English));
        QCOMPARE(service.effectiveEngineId(Language::Cantonese), QStringLiteral("azure"));

        // Azure selected but key removed -> system engine.
        m_settings->setAzureKey(QString());
        QCOMPARE(service.effectiveEngineId(Language::Cantonese), QStringLiteral("system"));
        if (noSystemBackend())
            QVERIFY(!service.canSpeak(Language::Cantonese));
    }

    void azureUsedWhenSystemLacksCantonese()
    {
        SpeechService service(m_settings.get(), nullptr);
        if (systemHasVoice(service, Language::Cantonese))
            QSKIP("A Cantonese system voice is installed.");
        configureAzure(QStringLiteral("system"));
        // Windows voices are selected, but there is no Cantonese one: use Azure.
        QCOMPARE(service.effectiveEngineId(Language::Cantonese), QStringLiteral("azure"));
        QVERIFY(service.canSpeak(Language::Cantonese));
    }

    void azureFailureWithoutSystemVoiceIsAnError()
    {
        configureAzure();
        SpeechService service(m_settings.get(), nullptr);
        if (systemHasVoice(service, Language::Cantonese))
            QSKIP("A Cantonese system voice is installed (Azure failures fall back to it).");
        azureOf(service)->setEndpointOverride(FakeAzureServer::deadUrl());

        QSignalSpy errors(&service, &SpeechService::errorOccurred);
        QSignalSpy notices(&service, &SpeechService::notice);
        QSignalSpy speaking(&service, &SpeechService::speakingChanged);
        QSignalSpy loading(&service, &SpeechService::loadingChanged);

        service.speak(QStringLiteral("你好"), Language::Cantonese);
        QVERIFY(service.isSpeaking());
        QVERIFY(service.isLoading());
        QVERIFY(errors.wait(WaitMs));
        QCOMPARE(notices.size(), 0);
        QVERIFY(errors.first().first().toString().contains(QStringLiteral("Azure")));
        QVERIFY(!service.isSpeaking());
        QVERIFY(!service.isLoading());
        QCOMPARE(speaking.size(), 2);
        QCOMPARE(speaking.at(0).first().toBool(), true);
        QCOMPARE(speaking.at(1).first().toBool(), false);
        QCOMPARE(loading.size(), 2);
    }

    void azureFailureFallsBackToSystemVoice()
    {
        configureAzure();
        SpeechService service(m_settings.get(), nullptr);
        if (service.voices(QStringLiteral("system"), Language::English).isEmpty())
            QSKIP("Needs a system TTS backend with an English voice.");
        azureOf(service)->setEndpointOverride(FakeAzureServer::deadUrl());

        QSignalSpy notices(&service, &SpeechService::notice);
        QSignalSpy speaking(&service, &SpeechService::speakingChanged);
        service.speak(QStringLiteral("Hello there"), Language::English);
        QVERIFY(notices.wait(WaitMs));
        QVERIFY(notices.first().first().toString().startsWith(QStringLiteral("Azure voice unavailable")));
        // The system voice took over (it may still fail later, e.g. without an audio device).
        QTRY_VERIFY_WITH_TIMEOUT(!service.isSpeaking(), WaitMs);
        QCOMPARE(speaking.first().first().toBool(), true);
        QCOMPARE(speaking.last().first().toBool(), false);
    }

    void systemVoiceStaysUsableAfterRuntimeError()
    {
        SpeechService service(m_settings.get(), nullptr);
        if (!systemHasVoice(service, Language::English))
            QSKIP("Needs a system TTS backend with an English voice.");
        // Without an audio device each attempt fails at playback; with one it
        // succeeds. Either way the engine must not become "unavailable".
        for (int i = 0; i < 2; ++i) {
            service.speak(QStringLiteral("Testing %1").arg(i), Language::English);
            QTRY_VERIFY_WITH_TIMEOUT(!service.isSpeaking(), WaitMs);
            QVERIFY(service.canSpeak(Language::English));
        }
    }

    void azureDownloadAndCache()
    {
        FakeAzureServer server;
        configureAzure();
        m_settings->setAzureVoice(Language::English, QStringLiteral("en-GB-SoniaNeural"));
        SpeechService service(m_settings.get(), nullptr);
        azureOf(service)->setEndpointOverride(server.url());

        service.speak(QStringLiteral("Good morning"), Language::English);
        QVERIFY(service.isLoading());
        QTRY_VERIFY_WITH_TIMEOUT(!service.isSpeaking(), WaitMs);
        QCOMPARE(server.requests.size(), 1);
        QVERIFY(QString::fromUtf8(server.requests.first().body).contains(QStringLiteral("en-GB-SoniaNeural")));
        QCOMPARE(server.requests.first().headers.value("ocp-apim-subscription-key"), QByteArray("test-key"));

        // Cached: a second speak does not hit the network.
        service.speak(QStringLiteral("Good morning"), Language::English);
        QTRY_VERIFY_WITH_TIMEOUT(!service.isSpeaking(), WaitMs);
        QCOMPARE(server.requests.size(), 1);
    }

    void testAzureButton()
    {
        FakeAzureServer server;
        SpeechService service(m_settings.get(), nullptr);  // Azure not configured in settings
        azureOf(service)->setEndpointOverride(server.url());
        QSignalSpy finished(&service, &SpeechService::azureTestFinished);

        service.testAzure(QString(), QStringLiteral("eastasia"), QString());
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.takeFirst().first().toBool(), false);

        service.testAzure(QStringLiteral("typed-key"), QStringLiteral("eastasia"),
                          QStringLiteral("zh-HK-HiuGaaiNeural"));
        QVERIFY(finished.wait(WaitMs));
        QCOMPARE(finished.first().first().toBool(), true);
        QCOMPARE(server.requests.size(), 1);
        QCOMPARE(server.requests.first().headers.value("ocp-apim-subscription-key"), QByteArray("typed-key"));
        QTRY_VERIFY_WITH_TIMEOUT(!service.isSpeaking(), WaitMs);
    }

    void interruptedAzureTestIsReported()
    {
        FakeAzureServer server;
        server.hang = true;
        SpeechService service(m_settings.get(), nullptr);
        azureOf(service)->setEndpointOverride(server.url());
        QSignalSpy finished(&service, &SpeechService::azureTestFinished);

        service.testAzure(QStringLiteral("typed-key"), QStringLiteral("eastasia"), QString());
        QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 1, WaitMs);
        // e.g. auto-speak after a translation while the Settings test waits.
        service.speak(QStringLiteral("Hello"), Language::English);
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().toBool(), false);
        service.stop();
        QCOMPARE(finished.size(), 1);  // reported once
    }

    void stopAndDestroyWhileLoading()
    {
        FakeAzureServer server;
        server.hang = true;
        configureAzure();
        auto service = std::make_unique<SpeechService>(m_settings.get(), nullptr);
        azureOf(*service)->setEndpointOverride(server.url());
        QSignalSpy errors(service.get(), &SpeechService::errorOccurred);

        service->speak(QStringLiteral("Hello"), Language::English);
        QVERIFY(service->isLoading());
        QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 1, WaitMs);
        service->stop();
        QVERIFY(!service->isSpeaking());
        QVERIFY(!service->isLoading());
        QCOMPARE(errors.size(), 0);

        service->speak(QStringLiteral("Hello again"), Language::English);
        QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 2, WaitMs);
        service.reset();  // with a request in flight
        QTest::qWait(50);
    }

    void sharedNetworkManagerIsNotOwned()
    {
        QNetworkAccessManager nam;
        {
            SpeechService service(m_settings.get(), &nam);
            QVERIFY(service.findChild<QNetworkAccessManager *>() == nullptr);
        }
        // nam is destroyed at scope exit; a double delete would crash here.
    }
};

QTEST_GUILESS_MAIN(TestSpeechService)
#include "tst_speechservice.moc"
