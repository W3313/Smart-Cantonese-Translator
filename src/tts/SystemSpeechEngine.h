#pragma once

#include "tts/SpeechEngine.h"
#include "tts/VoiceMatch.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QVoice>

#include <array>

class QTextToSpeech;
class QTimer;

namespace sct {

// Voices installed in the operating system, via QTextToSpeech.
// Windows: prefers the "winrt" backend (OneCore voices such as Microsoft
// Tracy / Danny for zh-HK), then "sapi". If the preferred backend has no
// Cantonese voice but the next one does, the next one is used.
//
// Never reads Cantonese text with a Mandarin (zh-CN / zh-TW) voice: without a
// Cantonese voice hasVoiceFor(Cantonese) is false and speak() reports
// voicematch::cantoneseVoiceHelpText().
class SystemSpeechEngine : public SpeechEngine
{
    Q_OBJECT

public:
    explicit SystemSpeechEngine(QObject *parent = nullptr);
    ~SystemSpeechEngine() override;

    QString id() const override;
    QString displayName() const override;
    bool isAvailable() const override;
    bool hasVoiceFor(Language lang) const override;
    QList<VoiceInfo> voices(Language lang) const override;
    void setVoice(Language lang, const QString &voiceId) override;
    void setRate(double rate) override;
    void speak(const QString &text, Language lang) override;
    void stop() override;
    State state() const override;

    // QTextToSpeech backend in use ("winrt", "sapi", "speechd", ...), empty if none.
    QString backendName() const { return m_backend; }
    // Re-reads the installed voices (e.g. after the user installed a voice
    // pack). Does nothing while speaking. Emits voicesChanged().
    void refreshVoices();

    // How long speak() waits for a backend that reports nothing at all before
    // showing Idle again (it is still tracked if it starts later). Default 10 s.
    void setNoResponseTimeout(int msecs) { m_noResponseMs = msecs; }

    // Backends in the order they are tried: winrt, sapi, speechd, flite, others.
    static QStringList orderBackends(const QStringList &available);

private:
    struct Entry
    {
        QVoice voice;
        voicematch::Candidate info;
    };

    void adopt(QTextToSpeech *tts, const QString &backend, const QList<Entry> &entries);
    static QList<Entry> enumerateVoices(QTextToSpeech *tts);
    static bool hasCantonese(const QList<Entry> &entries);
    QList<voicematch::Candidate> candidates() const;
    void onTtsStateChanged(int ttsState);
    void stopBackend();
    void fail(const QString &message);
    void setState(State state);

    QTextToSpeech *m_tts = nullptr;
    QString m_backend;
    QList<Entry> m_entries;
    std::array<QString, 2> m_preferred;  // user-chosen voice names, indexed by Language
    double m_rate = 0.0;
    int m_noResponseMs = 10000;
    State m_state = State::Idle;
    quint64 m_generation = 0;
    bool m_sawSpeaking = false;
    // The start check gave up on the current text (state went Idle) but the
    // backend may still start it late; it is then tracked again.
    bool m_awaitingLateStart = false;
    bool m_stopping = false;
    QTimer *m_startCheck = nullptr;
};

} // namespace sct
