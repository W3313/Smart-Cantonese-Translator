#pragma once

#include "core/TranslationTypes.h"

#include <QList>
#include <QObject>
#include <QString>

namespace sct {

struct VoiceInfo
{
    QString id;       // stable identifier persisted in settings (system: voice name; azure: "zh-HK-HiuMaanNeural")
    QString name;     // display name, e.g. "HiuMaan (female)" / "Microsoft Tracy"
    QString locale;   // BCP-47, e.g. "zh-HK", "en-US"
    QString gender;   // "Female" | "Male" | "" if unknown
};

// One text-to-speech backend.
class SpeechEngine : public QObject
{
    Q_OBJECT

public:
    enum class State { Idle, Loading, Speaking, Error };
    Q_ENUM(State)

    using QObject::QObject;
    ~SpeechEngine() override = default;

    virtual QString id() const = 0;           // "system" | "azure"
    virtual QString displayName() const = 0;  // "Windows voices" | "Azure neural voices"

    // True when the engine can speak at all (system: a TTS backend exists;
    // azure: key and region are set).
    virtual bool isAvailable() const = 0;
    // True when at least one voice for lang exists (system engine: Cantonese
    // needs the Windows "Chinese (Traditional, Hong Kong SAR)" speech pack).
    virtual bool hasVoiceFor(Language lang) const = 0;
    virtual QList<VoiceInfo> voices(Language lang) const = 0;

    // voiceId empty = engine picks the best voice for lang.
    virtual void setVoice(Language lang, const QString &voiceId) = 0;
    // -1.0 (slow) .. 1.0 (fast); 0 = normal.
    virtual void setRate(double rate) = 0;

    // Stops any current speech, then speaks text. Errors are reported via
    // errorOccurred() and the state returns to Idle afterwards.
    virtual void speak(const QString &text, Language lang) = 0;
    virtual void stop() = 0;
    virtual State state() const = 0;

signals:
    void stateChanged(sct::SpeechEngine::State state);
    void errorOccurred(const QString &message);
    void voicesChanged();
};

} // namespace sct
