#include "ui/SpeechController.h"

#include "tts/SpeechService.h"
#include "ui/Controls.h"

#include <QTimer>

namespace sct {

SpeechController::SpeechController(SpeechService *speech, QObject *parent)
    : QObject(parent)
    , m_speech(speech)
{
    if (!m_speech)
        return;
    connect(m_speech, &SpeechService::speakingChanged, this, &SpeechController::syncButton);
    connect(m_speech, &SpeechService::loadingChanged, this, &SpeechController::syncButton);
    connect(m_speech, &SpeechService::errorOccurred, this, [this](const QString &msg) {
        m_errorSeen = true;
        syncButton();
        emit message(msg);
    });
}

void SpeechController::attach(ui::SpeakButton *button, Source source)
{
    if (!button)
        return;
    connect(button, &ui::SpeakButton::clicked, this, [this, button, source = std::move(source)] {
        toggle(button, source ? source() : Utterance{});
    });
}

void SpeechController::toggle(ui::SpeakButton *button, const Utterance &utterance)
{
    if (m_speech && button && button == m_active && m_owns && m_speech->isSpeaking()) {
        stop();
        return;
    }
    speak(utterance, button);
}

void SpeechController::speak(const Utterance &utterance, ui::SpeakButton *button)
{
    if (!m_speech)
        return;
    const QString text = utterance.text.trimmed();
    if (text.isEmpty()) {
        emit message(tr("Nothing to read aloud yet."));
        return;
    }
    if (!m_speech->canSpeak(utterance.language)) {
        emit voiceUnavailable(utterance.language);
        return;
    }
    if (m_speech->isSpeaking())
        m_speech->stop();
    if (m_active && m_active != button)
        m_active->setSpeechState(ui::SpeakButton::State::Idle);
    m_active = button;
    m_owns = true;
    m_errorSeen = false;
    m_speech->speak(text, utterance.language);
    if (!m_active)
        return;
    if (m_errorSeen) {
        m_active->setSpeechState(ui::SpeakButton::State::Idle);
        m_owns = false;
        return;
    }
    // Engines may switch state asynchronously; show feedback right away and
    // fall back to idle if nothing actually started.
    m_active->setSpeechState(m_speech->isLoading() ? ui::SpeakButton::State::Loading
                                                   : ui::SpeakButton::State::Speaking);
    QPointer<ui::SpeakButton> started = m_active;
    QTimer::singleShot(2000, this, [this, started] {
        if (started && started == m_active && m_owns && m_speech && !m_speech->isSpeaking()) {
            started->setSpeechState(ui::SpeakButton::State::Idle);
            m_owns = false;
        }
    });
}

void SpeechController::stop()
{
    if (m_speech)
        m_speech->stop();
    if (m_active)
        m_active->setSpeechState(ui::SpeakButton::State::Idle);
    m_owns = false;
}

bool SpeechController::isSpeaking() const { return m_speech && m_speech->isSpeaking(); }

void SpeechController::syncButton()
{
    // Only mirror speech this controller started (Settings > Test voice uses
    // the same service).
    if (!m_speech || !m_owns)
        return;
    const bool busy = m_speech->isSpeaking();
    if (!m_active) {
        if (!busy)
            m_owns = false;
        return;
    }
    if (m_speech->isLoading()) {
        m_active->setSpeechState(ui::SpeakButton::State::Loading);
    } else if (busy) {
        m_active->setSpeechState(ui::SpeakButton::State::Speaking);
    } else {
        m_active->setSpeechState(ui::SpeakButton::State::Idle);
        m_owns = false;
    }
}

} // namespace sct
