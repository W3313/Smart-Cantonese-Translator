#pragma once

#include "core/TranslationTypes.h"

#include <QObject>
#include <QPointer>
#include <functional>

namespace sct {

class SpeechService;

namespace ui {
class SpeakButton;
}

// Connects SpeakButtons to SpeechService. Exactly one button (the one the
// user pressed last) mirrors the service state: speaker -> spinner while
// Azure audio loads -> stop while speaking. Pressing the active button again
// stops speech.
class SpeechController : public QObject
{
    Q_OBJECT

public:
    struct Utterance
    {
        QString text;
        Language language = Language::English;
    };
    using Source = std::function<Utterance()>;

    explicit SpeechController(SpeechService *speech, QObject *parent = nullptr);

    // Clicking button speaks whatever source() returns at that moment.
    void attach(ui::SpeakButton *button, Source source);

    // Speak (or stop, if button is already the active one).
    void toggle(ui::SpeakButton *button, const Utterance &utterance);
    // Speak without a button (auto-speak); uses button for state if given.
    void speak(const Utterance &utterance, ui::SpeakButton *button = nullptr);
    void stop();
    bool isSpeaking() const;

    SpeechService *service() const { return m_speech; }

signals:
    // The selected engine has no voice for lang (e.g. no Cantonese Windows voice).
    void voiceUnavailable(sct::Language lang);
    void message(const QString &text);  // short status-bar text

private:
    void syncButton();

    SpeechService *m_speech = nullptr;
    QPointer<ui::SpeakButton> m_active;
    bool m_errorSeen = false;
    bool m_owns = false;  // the current speech was started through this controller
};

} // namespace sct
