#include "FakeAzureServer.h"

#include "tts/AzureSpeechEngine.h"
#include "tts/AzureSupport.h"

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace sct;
using azure::FailureKind;

Q_DECLARE_METATYPE(sct::azure::FailureKind)

namespace {

constexpr int WaitMs = 15000;

bool acceptableAfterDownload(FailureKind kind)
{
    // Headless machines have no audio device; with one, the fake mp3 may fail to decode.
    return kind == FailureKind::None || kind == FailureKind::NoAudioDevice
           || kind == FailureKind::Playback;
}

} // namespace

class TestAzureSpeech : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        // Qt 6.4's GStreamer backend can crash on CI machines with an audio
        // device but incomplete GStreamer plugins; FFmpeg is Qt 6.5+'s default.
        if (qEnvironmentVariableIsEmpty("QT_MEDIA_BACKEND"))
            qputenv("QT_MEDIA_BACKEND", "ffmpeg");
    }

    // ---- pure helpers ----------------------------------------------------

    void normalizeRegion_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");
        QTest::newRow("plain") << "eastasia" << "eastasia";
        QTest::newRow("display name") << " East Asia " << "eastasia";
        QTest::newRow("upper") << "EASTUS" << "eastus";
        QTest::newRow("digits") << "westus2" << "westus2";
        QTest::newRow("endpoint url") << "https://eastasia.api.cognitive.microsoft.com/" << "eastasia";
        QTest::newRow("tts host") << "southeastasia.tts.speech.microsoft.com" << "southeastasia";
        QTest::newRow("custom domain") << "https://myres.cognitiveservices.azure.com/" << "";
        QTest::newRow("junk") << "east asia!" << "";
        QTest::newRow("empty") << "" << "";
    }

    void normalizeRegion()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(azure::normalizeRegion(input), expected);
    }

    void endpoints()
    {
        QCOMPARE(azure::synthesisUrl(QStringLiteral("eastasia")),
                 QUrl(QStringLiteral("https://eastasia.tts.speech.microsoft.com/cognitiveservices/v1")));
        QCOMPARE(azure::voicesListUrl(QStringLiteral("West US 2")),
                 QUrl(QStringLiteral("https://westus2.tts.speech.microsoft.com/cognitiveservices/voices/list")));
        QVERIFY(azure::synthesisUrl(QStringLiteral("!!")).isEmpty());
    }

    void catalog()
    {
        const QList<VoiceInfo> yue = azure::voiceCatalog(Language::Cantonese);
        QStringList ids;
        for (const VoiceInfo &v : yue) {
            ids << v.id;
            QCOMPARE(v.locale, QStringLiteral("zh-HK"));
            QVERIFY(!v.name.isEmpty());
            QVERIFY(v.gender == QLatin1String("Female") || v.gender == QLatin1String("Male"));
        }
        QCOMPARE(ids, QStringList({QStringLiteral("zh-HK-HiuMaanNeural"), QStringLiteral("zh-HK-HiuGaaiNeural"),
                                   QStringLiteral("zh-HK-WanLungNeural")}));
        QCOMPARE(azure::defaultVoice(Language::Cantonese), ids.first());

        const QList<VoiceInfo> en = azure::voiceCatalog(Language::English);
        QVERIFY(en.size() >= 5);
        QCOMPARE(en.first().id, azure::defaultVoice(Language::English));
        QCOMPARE(azure::defaultVoice(Language::English), QStringLiteral("en-US-AvaMultilingualNeural"));
        for (const VoiceInfo &v : en)
            QVERIFY(v.id.startsWith(QLatin1String("en-")));
    }

    void resolveVoice()
    {
        QCOMPARE(azure::resolveVoice(Language::Cantonese, QString()), QStringLiteral("zh-HK-HiuMaanNeural"));
        QCOMPARE(azure::resolveVoice(Language::Cantonese, QStringLiteral("zh-HK-WanLungNeural")),
                 QStringLiteral("zh-HK-WanLungNeural"));
        // Never a Mandarin or English voice for Cantonese text.
        QCOMPARE(azure::resolveVoice(Language::Cantonese, QStringLiteral("zh-CN-XiaoxiaoNeural")),
                 QStringLiteral("zh-HK-HiuMaanNeural"));
        QCOMPARE(azure::resolveVoice(Language::Cantonese, QStringLiteral("en-US-GuyNeural")),
                 QStringLiteral("zh-HK-HiuMaanNeural"));
        QCOMPARE(azure::resolveVoice(Language::English, QStringLiteral("en-GB-SoniaNeural")),
                 QStringLiteral("en-GB-SoniaNeural"));
        QCOMPARE(azure::resolveVoice(Language::English, QStringLiteral("zh-HK-HiuGaaiNeural")),
                 QStringLiteral("en-US-AvaMultilingualNeural"));
        QCOMPARE(azure::resolveVoice(Language::English, QStringLiteral("en-US-\"><x")),
                 QStringLiteral("en-US-AvaMultilingualNeural"));
    }

    void sampleText()
    {
        QVERIFY(azure::sampleText(QStringLiteral("zh-HK-HiuGaaiNeural")).contains(QStringLiteral("你好")));
        QVERIFY(azure::sampleText(QStringLiteral("en-US-GuyNeural")).contains(QStringLiteral("Welcome")));
    }

    void describeFailure_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<QNetworkReply::NetworkError>("error");
        QTest::addColumn<FailureKind>("kind");
        QTest::addColumn<QString>("mentions");
        QTest::newRow("401") << 401 << QNetworkReply::AuthenticationRequiredError << FailureKind::Auth << "key";
        QTest::newRow("401 region") << 401 << QNetworkReply::AuthenticationRequiredError << FailureKind::Auth << "eastasia";
        QTest::newRow("403") << 403 << QNetworkReply::ContentAccessDenied << FailureKind::Auth << "region";
        QTest::newRow("429") << 429 << QNetworkReply::UnknownContentError << FailureKind::RateLimited << "quota";
        QTest::newRow("400") << 400 << QNetworkReply::ProtocolInvalidOperationError << FailureKind::BadRequest << "voice";
        QTest::newRow("415") << 415 << QNetworkReply::UnknownContentError << FailureKind::BadRequest << "415";
        QTest::newRow("404") << 404 << QNetworkReply::ContentNotFoundError << FailureKind::BadRequest << "region";
        QTest::newRow("500") << 500 << QNetworkReply::InternalServerError << FailureKind::Server << "500";
        QTest::newRow("503") << 503 << QNetworkReply::ServiceUnavailableError << FailureKind::Server << "later";
        QTest::newRow("dns") << 0 << QNetworkReply::HostNotFoundError << FailureKind::Network << "offline";
        QTest::newRow("refused") << 0 << QNetworkReply::ConnectionRefusedError << FailureKind::Network << "offline";
        QTest::newRow("tls") << 0 << QNetworkReply::SslHandshakeFailedError << FailureKind::Network << "secure";
        QTest::newRow("timeout") << 0 << QNetworkReply::OperationCanceledError << FailureKind::Timeout << "20 seconds";
        QTest::newRow("timeout2") << 0 << QNetworkReply::TimeoutError << FailureKind::Timeout << "respond";
        QTest::newRow("ok") << 200 << QNetworkReply::NoError << FailureKind::None << "";
    }

    void describeFailure()
    {
        QFETCH(int, status);
        QFETCH(QNetworkReply::NetworkError, error);
        QFETCH(FailureKind, kind);
        QFETCH(QString, mentions);
        const azure::Failure f = azure::describeFailure(status, error, QStringLiteral("eastasia"),
                                                        QStringLiteral("detail"));
        QCOMPARE(f.kind, kind);
        if (kind != FailureKind::None)
            QVERIFY2(f.message.contains(mentions, Qt::CaseInsensitive), qPrintable(f.message));
        else
            QVERIFY(f.message.isEmpty());
    }

    // ---- engine ---------------------------------------------------------

    void notConfigured()
    {
        QNetworkAccessManager nam;
        AzureSpeechEngine e(&nam);
        QCOMPARE(e.id(), QStringLiteral("azure"));
        QVERIFY(e.displayName().contains(QStringLiteral("Azure")));
        QVERIFY(!e.isAvailable());
        QVERIFY(!e.hasVoiceFor(Language::Cantonese));
        QCOMPARE(e.voices(Language::Cantonese).size(), 3);

        QSignalSpy errors(&e, &SpeechEngine::errorOccurred);
        QSignalSpy states(&e, &SpeechEngine::stateChanged);
        e.speak(QStringLiteral("hello"), Language::English);
        QCOMPARE(errors.size(), 1);
        QCOMPARE(e.lastFailure().kind, FailureKind::NotConfigured);
        QCOMPARE(states.size(), 0);
        QCOMPARE(e.state(), SpeechEngine::State::Idle);

        e.setCredentials(QStringLiteral("key"), QStringLiteral("!!"));
        QVERIFY(!e.isAvailable());  // invalid region
    }

    void configurationAndVoices()
    {
        QNetworkAccessManager nam;
        AzureSpeechEngine e(&nam);
        e.setCredentials(QStringLiteral("  key  "), QStringLiteral("East Asia"));
        QVERIFY(e.isAvailable());
        QVERIFY(e.hasVoiceFor(Language::Cantonese));
        QCOMPARE(e.region(), QStringLiteral("eastasia"));
        QCOMPARE(e.voiceFor(Language::Cantonese), QStringLiteral("zh-HK-HiuMaanNeural"));
        e.setVoice(Language::Cantonese, QStringLiteral("zh-HK-WanLungNeural"));
        QCOMPARE(e.voiceFor(Language::Cantonese), QStringLiteral("zh-HK-WanLungNeural"));
        e.setVoice(Language::Cantonese, QStringLiteral("zh-CN-XiaoxiaoNeural"));
        QCOMPARE(e.voiceFor(Language::Cantonese), QStringLiteral("zh-HK-HiuMaanNeural"));

        // A valid voice outside the curated list stays selectable.
        e.setVoice(Language::English, QStringLiteral("en-AU-NatashaNeural"));
        const QList<VoiceInfo> en = e.voices(Language::English);
        QVERIFY(std::any_of(en.cbegin(), en.cend(), [](const VoiceInfo &v) {
            return v.id == QLatin1String("en-AU-NatashaNeural") && v.locale == QLatin1String("en-AU");
        }));
    }

    void requestHeadersBodyAndCache()
    {
        FakeAzureServer server;
        QVERIFY(server.isListening());
        QTemporaryDir tmp;
        QNetworkAccessManager nam;
        AzureSpeechEngine e(&nam);
        e.setCredentials(QStringLiteral("secret-key"), QStringLiteral("eastasia"));
        e.setEndpointOverride(server.url());
        e.setCacheDirectory(tmp.filePath(QStringLiteral("audio-cache")));
        e.setVoice(Language::Cantonese, QStringLiteral("zh-HK-HiuGaaiNeural"));
        e.setRate(0.5);

        QSignalSpy states(&e, &SpeechEngine::stateChanged);
        e.speak(QStringLiteral("早晨！食咗飯未？"), Language::Cantonese);
        QCOMPARE(e.state(), SpeechEngine::State::Loading);
        QCOMPARE(states.size(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(e.state(), SpeechEngine::State::Idle, WaitMs);

        QCOMPARE(server.requests.size(), 1);
        const FakeRequest &r = server.requests.first();
        QCOMPARE(r.method, QByteArray("POST"));
        QCOMPARE(r.path, QByteArray("/cognitiveservices/v1"));
        QCOMPARE(r.headers.value("ocp-apim-subscription-key"), QByteArray("secret-key"));
        QCOMPARE(r.headers.value("content-type"), QByteArray("application/ssml+xml"));
        QCOMPARE(r.headers.value("x-microsoft-outputformat"), QByteArray("audio-24khz-48kbitrate-mono-mp3"));
        QCOMPARE(r.headers.value("user-agent"), QByteArray("SmartCantoneseTranslator"));
        const QString ssml = QString::fromUtf8(r.body);
        QVERIFY2(ssml.contains(QStringLiteral("<voice name=\"zh-HK-HiuGaaiNeural\">")), qPrintable(ssml));
        QVERIFY(ssml.contains(QStringLiteral("xml:lang=\"zh-HK\"")));
        QVERIFY(ssml.contains(QStringLiteral("rate=\"+25%\"")));
        QVERIFY(ssml.contains(QStringLiteral("早晨！食咗飯未？")));

        QVERIFY2(acceptableAfterDownload(e.lastFailure().kind), qPrintable(e.lastFailure().message));
        QCOMPARE(e.cache().fileCount(), 1);

        // Same text again: served from the cache, no request.
        e.speak(QStringLiteral("早晨！食咗飯未？"), Language::Cantonese);
        QTRY_COMPARE_WITH_TIMEOUT(e.state(), SpeechEngine::State::Idle, WaitMs);
        QCOMPARE(server.requests.size(), 1);

        // Different rate -> different audio -> new request.
        e.setRate(0.0);
        e.speak(QStringLiteral("早晨！食咗飯未？"), Language::Cantonese);
        QTRY_COMPARE_WITH_TIMEOUT(e.state(), SpeechEngine::State::Idle, WaitMs);
        QCOMPARE(server.requests.size(), 2);
        QCOMPARE(e.cache().fileCount(), 2);
    }

    void httpErrors_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<FailureKind>("kind");
        QTest::newRow("401") << 401 << FailureKind::Auth;
        QTest::newRow("403") << 403 << FailureKind::Auth;
        QTest::newRow("429") << 429 << FailureKind::RateLimited;
        QTest::newRow("400") << 400 << FailureKind::BadRequest;
        QTest::newRow("500") << 500 << FailureKind::Server;
    }

    void httpErrors()
    {
        QFETCH(int, status);
        QFETCH(FailureKind, kind);
        FakeAzureServer server;
        server.status = status;
        server.contentType = "text/plain";
        server.body = "nope";
        QTemporaryDir tmp;
        QNetworkAccessManager nam;
        AzureSpeechEngine e(&nam);
        e.setCredentials(QStringLiteral("k"), QStringLiteral("eastasia"));
        e.setEndpointOverride(server.url());
        e.setCacheDirectory(tmp.path());

        QSignalSpy errors(&e, &SpeechEngine::errorOccurred);
        e.speak(QStringLiteral("Hello"), Language::English);
        QVERIFY(errors.wait(WaitMs));
        QCOMPARE(e.lastFailure().kind, kind);
        QCOMPARE(errors.first().first().toString(), e.lastFailure().message);
        QCOMPARE(e.state(), SpeechEngine::State::Idle);
        QCOMPARE(e.cache().fileCount(), 0);
    }

    void nonAudioResponseIsRejected()
    {
        FakeAzureServer server;  // e.g. a captive portal answering 200 with HTML
        server.contentType = "text/html";
        server.body = "<html>Please sign in</html>";
        QTemporaryDir tmp;
        QNetworkAccessManager nam;
        AzureSpeechEngine e(&nam);
        e.setCredentials(QStringLiteral("k"), QStringLiteral("eastasia"));
        e.setEndpointOverride(server.url());
        e.setCacheDirectory(tmp.path());
        QSignalSpy errors(&e, &SpeechEngine::errorOccurred);
        e.speak(QStringLiteral("Hello"), Language::English);
        QVERIFY(errors.wait(WaitMs));
        QCOMPARE(e.lastFailure().kind, FailureKind::Network);
        QCOMPARE(e.cache().fileCount(), 0);
    }

    void offline()
    {
        QNetworkAccessManager nam;
        AzureSpeechEngine e(&nam);
        e.setCredentials(QStringLiteral("k"), QStringLiteral("eastasia"));
        e.setEndpointOverride(FakeAzureServer::deadUrl());
        QSignalSpy errors(&e, &SpeechEngine::errorOccurred);
        e.speak(QStringLiteral("Hello"), Language::English);
        QVERIFY(errors.wait(WaitMs));
        QCOMPARE(e.lastFailure().kind, FailureKind::Network);
        QCOMPARE(e.state(), SpeechEngine::State::Idle);
    }

    void stopWhileLoading()
    {
        FakeAzureServer server;
        server.hang = true;
        QNetworkAccessManager nam;
        AzureSpeechEngine e(&nam);
        e.setCredentials(QStringLiteral("k"), QStringLiteral("eastasia"));
        e.setEndpointOverride(server.url());
        QSignalSpy errors(&e, &SpeechEngine::errorOccurred);
        e.speak(QStringLiteral("Hello"), Language::English);
        QCOMPARE(e.state(), SpeechEngine::State::Loading);
        QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 1, WaitMs);
        e.stop();
        QCOMPARE(e.state(), SpeechEngine::State::Idle);
        QTest::qWait(100);
        QCOMPARE(errors.size(), 0);
        QCOMPARE(e.state(), SpeechEngine::State::Idle);
    }

    void longTextIsShortenedWithNotice()
    {
        FakeAzureServer server;
        QTemporaryDir tmp;
        QNetworkAccessManager nam;
        AzureSpeechEngine e(&nam);
        e.setCredentials(QStringLiteral("k"), QStringLiteral("eastasia"));
        e.setEndpointOverride(server.url());
        e.setCacheDirectory(tmp.path());
        QSignalSpy notices(&e, &AzureSpeechEngine::notice);
        e.speak(QStringLiteral("This is a sentence. ").repeated(300), Language::English);
        QCOMPARE(notices.size(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(e.state(), SpeechEngine::State::Idle, WaitMs);
        QCOMPARE(server.requests.size(), 1);
        QVERIFY(server.requests.first().body.size() < 3500);
    }

    void testButton()
    {
        FakeAzureServer server;
        QTemporaryDir tmp;
        QNetworkAccessManager nam;
        AzureSpeechEngine e(&nam);
        e.setEndpointOverride(server.url());
        e.setCacheDirectory(tmp.path());
        QSignalSpy finished(&e, &AzureSpeechEngine::testFinished);

        // Missing key / region: immediate failure, no request.
        e.test(QString(), QStringLiteral("eastasia"), QString());
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.takeFirst().at(0).toBool(), false);
        e.test(QStringLiteral("k"), QStringLiteral("??"), QString());
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.takeFirst().at(0).toBool(), false);
        QCOMPARE(server.requests.size(), 0);

        // Uses the unsaved key and voice, not the engine's settings.
        e.test(QStringLiteral("unsaved-key"), QStringLiteral("westus"), QStringLiteral("zh-HK-WanLungNeural"));
        QVERIFY(finished.wait(WaitMs));
        QCOMPARE(finished.first().at(0).toBool(), true);
        QCOMPARE(server.requests.size(), 1);
        QCOMPARE(server.requests.first().headers.value("ocp-apim-subscription-key"), QByteArray("unsaved-key"));
        const QString ssml = QString::fromUtf8(server.requests.first().body);
        QVERIFY(ssml.contains(QStringLiteral("zh-HK-WanLungNeural")));
        QVERIFY(ssml.contains(QStringLiteral("你好")));
        QTRY_COMPARE_WITH_TIMEOUT(e.state(), SpeechEngine::State::Idle, WaitMs);

        // The test always contacts Azure (a cached sample would hide a bad key).
        finished.clear();
        server.status = 401;
        server.body = "denied";
        QSignalSpy errors(&e, &SpeechEngine::errorOccurred);
        e.test(QStringLiteral("bad-key"), QStringLiteral("westus"), QStringLiteral("zh-HK-WanLungNeural"));
        QVERIFY(finished.wait(WaitMs));
        QCOMPARE(finished.first().at(0).toBool(), false);
        QVERIFY(finished.first().at(1).toString().contains(QStringLiteral("key")));
        QCOMPARE(server.requests.size(), 2);
        QCOMPARE(errors.size(), 0);  // reported via testFinished only
        QCOMPARE(e.state(), SpeechEngine::State::Idle);
    }
};

QTEST_GUILESS_MAIN(TestAzureSpeech)
#include "tst_azurespeech.moc"
