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

public slots:
    void reloadSettings();

signals:
    void speakingChanged(bool speaking);
    void loadingChanged(bool loading);
    void errorOccurred(const QString &message);
    void notice(const QString &message);  // non-fatal info, e.g. "Azure unavailable - used Windows voice"
    void azureTestFinished(bool ok, const QString &message);
    void voicesChanged();

private:
    SpeechEngine *engine(const QString &engineId) const;
    SpeechEngine *activeEngine() const;

    AppSettings *m_settings = nullptr;
    QNetworkAccessManager *m_nam = nullptr;
    SpeechEngine *m_system = nullptr;
    SpeechEngine *m_azure = nullptr;
    SpeechEngine *m_current = nullptr;  // engine currently speaking
};

} // namespace sct
