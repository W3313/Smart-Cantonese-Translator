#pragma once

#include "core/TranslationTypes.h"
#include "tts/SpeechEngine.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;

namespace sct {

class AppSettings;
class AzureSpeechEngine;
class SystemSpeechEngine;

// Facade the UI talks to for read-aloud. Owns the system and Azure engines,
// applies AppSettings, and falls back from Azure to the system voice (with a
// notice) when Azure fails, e.g. offline or bad key.
class SpeechService : public QObject
{
    Q_OBJECT

public:
    // nam may be shared with TranslationService; if null, one is created.
    SpeechService(AppSettings *settings, QNetworkAccessManager *nam, QObject *parent = nullptr);
    ~SpeechService() override;

    void speak(const QString &text, Language lang);
    void stop();
    bool isSpeaking() const;  // true while Loading or Speaking
    bool isLoading() const;   // fetching audio (Azure)

    QStringList engineIds() const;  // {"system", "azure"}
    QString engineDisplayName(const QString &engineId) const;
    QList<VoiceInfo> voices(const QString &engineId, Language lang) const;
    // False when the currently selected engine cannot speak lang (e.g. no
    // Cantonese Windows voice installed and Azure not configured).
    bool canSpeak(Language lang) const;
    // Help text explaining how to get a Cantonese voice (shown by the UI).
    static QString cantoneseVoiceHelpText();

    // Speaks a short sample with the given (possibly unsaved) Azure settings.
    // Emits azureTestFinished(ok, message).
    void testAzure(const QString &key, const QString &region, const QString &voiceId);

    // Id of the engine speak(lang) would use right now: the selected engine
    // if it can speak lang, else the other one if it can (e.g. Azure is
    // selected but not configured -> "system"; Windows has no Cantonese voice
    // but Azure is configured -> "azure").
    QString effectiveEngineId(Language lang) const;
    // Re-reads the installed system voices (e.g. after installing the Windows
    // Cantonese voice). No-op while speaking; emits voicesChanged().
    void refreshVoices();

public slots:
    void reloadSettings();

signals:
    void speakingChanged(bool speaking);
    void loadingChanged(bool loading);
    void errorOccurred(const QString &message);
    void notice(const QString &message);  // non-fatal info, e.g. "Azure unavailable - used Windows voice"
    void azureTestFinished(bool ok, const QString &message);
    // Voice lists or speakability changed (installed voices, engine choice,
    // Azure configured or not) - re-query voices()/canSpeak().
    void voicesChanged();

private:
    SpeechEngine *engine(const QString &engineId) const;
    SpeechEngine *activeEngine() const;
    SpeechEngine *engineFor(Language lang) const;
    AzureSpeechEngine *azureEngine() const;
    SystemSpeechEngine *systemEngine() const;
    void onEngineError(SpeechEngine *source, const QString &message);
    void updateFlags();

    AppSettings *m_settings = nullptr;
    QNetworkAccessManager *m_nam = nullptr;
    SpeechEngine *m_system = nullptr;
    SpeechEngine *m_azure = nullptr;
    SpeechEngine *m_current = nullptr;  // engine currently speaking

    QString m_engineId = QStringLiteral("system");  // selected in settings
    QString m_lastText;  // for the Azure -> system fallback
    Language m_lastLang = Language::English;
    bool m_testingAzure = false;
    bool m_speaking = false;  // last emitted values
    bool m_loading = false;
};

} // namespace sct
