#pragma once

#include "tts/AudioCache.h"
#include "tts/AzureSupport.h"
#include "tts/SpeechEngine.h"

#include <QPointer>
#include <QString>
#include <QUrl>

#include <array>

class QAudioOutput;
class QBuffer;
class QMediaPlayer;
class QNetworkAccessManager;
class QNetworkReply;

namespace sct {

// Azure neural voices via the Speech REST API. Audio (mp3) is downloaded into
// an on-disk AudioCache and played with QMediaPlayer, so repeating the same
// text is free and works offline.
//
// States: Loading while downloading, Speaking while playing, Idle otherwise.
class AzureSpeechEngine : public SpeechEngine
{
    Q_OBJECT

public:
    // nam is not owned and must outlive the engine.
    explicit AzureSpeechEngine(QNetworkAccessManager *nam, QObject *parent = nullptr);
    ~AzureSpeechEngine() override;

    QString id() const override;
    QString displayName() const override;
    bool isAvailable() const override;  // key and a valid region are set
    bool hasVoiceFor(Language lang) const override;
    QList<VoiceInfo> voices(Language lang) const override;
    void setVoice(Language lang, const QString &voiceId) override;
    void setRate(double rate) override;
    void speak(const QString &text, Language lang) override;
    void stop() override;
    State state() const override;

    void setCredentials(const QString &key, const QString &region);
    QString region() const { return m_region; }
    // Voice that speak() uses for lang (the configured one, validated).
    QString voiceFor(Language lang) const;

    // Directory of the audio cache (e.g. AppSettings::dataDirectory() + "/audio-cache").
    void setCacheDirectory(const QString &directory);
    QString cacheDirectory() const;
    AudioCache &cache() { return m_cache; }

    // Synthesizes and plays a short sample with the given (possibly unsaved)
    // settings, bypassing the cache lookup. Emits testFinished() once the
    // request completed; playback problems are reported via errorOccurred().
    void test(const QString &key, const QString &region, const QString &voiceId);

    // Why the last speak()/test() failed (kind None if it did not).
    azure::Failure lastFailure() const { return m_lastFailure; }
    bool isTestRunning() const { return m_testRunning; }

    // Tests / diagnostics only: send synthesis requests to this URL instead of
    // https://{region}.tts.speech.microsoft.com/... (empty = regional endpoint).
    void setEndpointOverride(const QUrl &url) { m_endpointOverride = url; }

signals:
    void testFinished(bool ok, const QString &message);
    // Non-fatal information, e.g. that a long text was shortened.
    void notice(const QString &message);

private:
    struct Job
    {
        QString key;
        QString region;
        QString voice;
        QString text;
        double rate = 0.0;
        bool isTest = false;
    };

    void start(const Job &job);
    void play(const QString &filePath, const QByteArray &fallbackData = QByteArray());
    bool ensurePlayer();
    void onMediaStatusChanged(int status);
    void onPlayerError(const QString &errorString);
    void fail(azure::FailureKind kind, const QString &message);
    void abortReply();
    void stopPlayer();
    void setState(State state);

    QNetworkAccessManager *m_nam = nullptr;
    QPointer<QNetworkReply> m_reply;
    QMediaPlayer *m_player = nullptr;      // created lazily (needs an audio device)
    QAudioOutput *m_audioOutput = nullptr;
    QBuffer *m_buffer = nullptr;           // in-memory playback when the cache is unwritable
    AudioCache m_cache;
    QString m_playingKey;

    QString m_key;
    QString m_region;
    QUrl m_endpointOverride;
    std::array<QString, 2> m_voices;  // indexed by Language
    double m_rate = 0.0;
    State m_state = State::Idle;
    quint64 m_generation = 0;  // bumped by every speak/stop; detects re-entrant calls
    bool m_testRunning = false;
    azure::Failure m_lastFailure;
};

} // namespace sct
