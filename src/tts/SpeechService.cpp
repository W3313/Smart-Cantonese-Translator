#include "tts/SpeechService.h"

#include "core/AppSettings.h"
#include "tts/AzureSpeechEngine.h"
#include "tts/SystemSpeechEngine.h"
#include "tts/VoiceMatch.h"

#include <QDir>
#include <QNetworkAccessManager>

namespace sct {

namespace {

const QString kSystemId = QStringLiteral("system");
const QString kAzureId = QStringLiteral("azure");

} // namespace

SpeechService::SpeechService(AppSettings *settings, QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_nam(nam ? nam : new QNetworkAccessManager(this))
{
    auto *system = new SystemSpeechEngine(this);
    auto *azure = new AzureSpeechEngine(m_nam, this);
    m_system = system;
    m_azure = azure;

    for (SpeechEngine *e : {m_system, m_azure}) {
        connect(e, &SpeechEngine::stateChanged, this, &SpeechService::updateFlags);
        connect(e, &SpeechEngine::errorOccurred, this,
                [this, e](const QString &message) { onEngineError(e, message); });
        connect(e, &SpeechEngine::voicesChanged, this, &SpeechService::voicesChanged);
    }
    connect(azure, &AzureSpeechEngine::notice, this, &SpeechService::notice);
    connect(azure, &AzureSpeechEngine::testFinished, this, &SpeechService::azureTestFinished);

    if (m_settings)
        connect(m_settings, &AppSettings::changed, this, &SpeechService::reloadSettings);
    reloadSettings();
}

SpeechService::~SpeechService()
{
    // Tear the engines down before the (possibly owned) network manager and
    // without delivering their last signals to a half-destroyed service.
    m_current = nullptr;
    for (SpeechEngine *e : {m_azure, m_system}) {
        e->disconnect(this);
        e->stop();
    }
    delete m_azure;
    delete m_system;
    m_azure = nullptr;
    m_system = nullptr;
}

AzureSpeechEngine *SpeechService::azureEngine() const
{
    return static_cast<AzureSpeechEngine *>(m_azure);
}

SystemSpeechEngine *SpeechService::systemEngine() const
{
    return static_cast<SystemSpeechEngine *>(m_system);
}

SpeechEngine *SpeechService::engine(const QString &engineId) const
{
    if (engineId == kSystemId)
        return m_system;
    if (engineId == kAzureId)
        return m_azure;
    return nullptr;
}

SpeechEngine *SpeechService::activeEngine() const
{
    SpeechEngine *e = engine(m_engineId);
    return e ? e : m_system;
}

SpeechEngine *SpeechService::engineFor(Language lang) const
{
    SpeechEngine *preferred = activeEngine();
    SpeechEngine *other = preferred == m_azure ? m_system : m_azure;
    if (preferred->isAvailable() && preferred->hasVoiceFor(lang))
        return preferred;
    if (other->isAvailable() && other->hasVoiceFor(lang))
        return other;
    // Nothing can speak lang: the system engine explains what to install.
    return m_system;
}

void SpeechService::reloadSettings()
{
    if (!m_settings)
        return;

    const QString oldEngineId = m_engineId;
    const bool azureWasAvailable = m_azure->isAvailable();

    const QString engineId = m_settings->speechEngine();
    m_engineId = engine(engineId) ? engineId : kSystemId;

    const double rate = m_settings->speechRate();
    for (const Language lang : {Language::Cantonese, Language::English}) {
        m_system->setVoice(lang, m_settings->systemVoice(lang));
        m_azure->setVoice(lang, m_settings->azureVoice(lang));
    }
    m_system->setRate(rate);
    m_azure->setRate(rate);
    azureEngine()->setCredentials(m_settings->azureKey(), m_settings->azureRegion());

    const QString dataDir = m_settings->dataDirectory();
    azureEngine()->setCacheDirectory(
        dataDir.isEmpty() ? QString() : QDir(dataDir).filePath(QStringLiteral("audio-cache")));

    if (oldEngineId != m_engineId || azureWasAvailable != m_azure->isAvailable())
        emit voicesChanged();
}

void SpeechService::speak(const QString &text, Language lang)
{
    stop();
    if (text.trimmed().isEmpty())
        return;

    m_lastText = text;
    m_lastLang = lang;
    m_testingAzure = false;
    m_current = engineFor(lang);
    m_current->speak(text, lang);
    updateFlags();
}

void SpeechService::stop()
{
    m_testingAzure = false;
    m_current = nullptr;
    // Stop idle engines too: the system engine reports Idle once it gave up
    // waiting for a slow backend that may still start speaking later.
    for (SpeechEngine *e : {m_system, m_azure})
        e->stop();
    updateFlags();
}

void SpeechService::onEngineError(SpeechEngine *source, const QString &message)
{
    if (source != m_current)
        return;  // stale: the user already stopped or started something else

    if (source == m_azure && !m_testingAzure && !m_lastText.isEmpty()
        && m_system->isAvailable() && m_system->hasVoiceFor(m_lastLang)) {
#ifdef Q_OS_WIN
        emit notice(tr("Azure voice unavailable — used Windows voice instead: %1").arg(message));
#else
        emit notice(tr("Azure voice unavailable — used system voice instead: %1").arg(message));
#endif
        if (source != m_current)
            return;  // a notice handler stopped or restarted speech
        m_current = m_system;
        m_system->speak(m_lastText, m_lastLang);
        updateFlags();
        return;
    }

    m_current = nullptr;
    updateFlags();
    emit errorOccurred(message);
}

void SpeechService::testAzure(const QString &key, const QString &region, const QString &voiceId)
{
    stop();
    m_lastText.clear();
    m_testingAzure = true;
    m_current = m_azure;
    azureEngine()->test(key, region, voiceId);
    updateFlags();
}

void SpeechService::updateFlags()
{
    const bool loading = isLoading();
    const bool speaking = isSpeaking();
    if (loading != m_loading) {
        m_loading = loading;
        emit loadingChanged(loading);
    }
    if (speaking != m_speaking) {
        m_speaking = speaking;
        emit speakingChanged(speaking);
    }
}

bool SpeechService::isSpeaking() const
{
    if (!m_current)
        return false;
    const SpeechEngine::State s = m_current->state();
    return s == SpeechEngine::State::Loading || s == SpeechEngine::State::Speaking;
}

bool SpeechService::isLoading() const
{
    return m_current && m_current->state() == SpeechEngine::State::Loading;
}

QStringList SpeechService::engineIds() const
{
    return {kSystemId, kAzureId};
}

QString SpeechService::engineDisplayName(const QString &engineId) const
{
    const SpeechEngine *e = engine(engineId);
    return e ? e->displayName() : QString();
}

QList<VoiceInfo> SpeechService::voices(const QString &engineId, Language lang) const
{
    const SpeechEngine *e = engine(engineId);
    return e ? e->voices(lang) : QList<VoiceInfo>();
}

bool SpeechService::canSpeak(Language lang) const
{
    const SpeechEngine *e = engineFor(lang);
    return e->isAvailable() && e->hasVoiceFor(lang);
}

QString SpeechService::effectiveEngineId(Language lang) const
{
    return engineFor(lang)->id();
}

void SpeechService::refreshVoices()
{
    systemEngine()->refreshVoices();
}

QString SpeechService::cantoneseVoiceHelpText()
{
    return voicematch::cantoneseVoiceHelpText();
}

} // namespace sct
