#include "tts/SystemSpeechEngine.h"

#include "tts/SsmlBuilder.h"

#include <QLocale>
#include <QSet>
#include <QSignalBlocker>
#include <QTextToSpeech>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace sct {

namespace {

std::size_t langIndex(Language lang)
{
    return lang == Language::Cantonese ? 0 : 1;
}

QString genderString(QVoice::Gender g)
{
    switch (g) {
    case QVoice::Male:
        return QStringLiteral("Male");
    case QVoice::Female:
        return QStringLiteral("Female");
    default:
        return QString();
    }
}

// If speech has not started this long after a (possibly stale) "Ready", the
// engine silently dropped the request.
constexpr int StartCheckMs = 2000;
// Backends probed at startup when the first one lacks a Cantonese voice.
constexpr int MaxBackendsProbed = 2;

} // namespace

SystemSpeechEngine::SystemSpeechEngine(QObject *parent)
    : SpeechEngine(parent)
    , m_startCheck(new QTimer(this))
{
    m_startCheck->setSingleShot(true);
    m_startCheck->setInterval(StartCheckMs);
    connect(m_startCheck, &QTimer::timeout, this, [this] {
        if (m_state == State::Loading && m_tts && m_tts->state() != QTextToSpeech::Speaking)
            setState(State::Idle);
    });

    const QStringList order = orderBackends(QTextToSpeech::availableEngines());
    int probed = 0;
    for (const QString &name : order) {
        if (probed >= MaxBackendsProbed)
            break;
        auto *tts = new QTextToSpeech(name, this);
        if (tts->state() == QTextToSpeech::Error) {
            delete tts;
            continue;
        }
        ++probed;
        const QList<Entry> entries = enumerateVoices(tts);
        const bool cantonese = hasCantonese(entries);
        if (!m_tts) {
            adopt(tts, name, entries);
            if (cantonese)
                break;
            continue;  // try the next backend for a Cantonese voice
        }
        if (cantonese) {
            delete m_tts;
            m_tts = nullptr;
            adopt(tts, name, entries);
            break;
        }
        delete tts;
    }
}

SystemSpeechEngine::~SystemSpeechEngine()
{
    if (m_tts) {
        m_tts->disconnect(this);
        if (m_state != State::Idle)
            m_tts->stop();
    }
}

QStringList SystemSpeechEngine::orderBackends(const QStringList &available)
{
    static const char *const preferred[] = {"winrt", "sapi", "speechd", "flite"};
    QStringList out;
    for (const char *p : preferred) {
        const QString name = QString::fromLatin1(p);
        if (available.contains(name))
            out << name;
    }
    for (const QString &name : available) {
        if (!out.contains(name) && name != QLatin1String("mock"))
            out << name;
    }
    return out;
}

void SystemSpeechEngine::adopt(QTextToSpeech *tts, const QString &backend,
                               const QList<Entry> &entries)
{
    m_tts = tts;
    m_backend = backend;
    m_entries = entries;
    connect(m_tts, &QTextToSpeech::stateChanged, this,
            [this](QTextToSpeech::State s) { onTtsStateChanged(int(s)); });
    connect(m_tts, &QTextToSpeech::errorOccurred, this,
            [this](QTextToSpeech::ErrorReason, const QString &message) {
                if (m_state != State::Idle && !m_stopping)
                    fail(message.isEmpty() ? tr("The speech engine reported an error.") : message);
            });
}

QList<SystemSpeechEngine::Entry> SystemSpeechEngine::enumerateVoices(QTextToSpeech *tts)
{
    QList<Entry> out;
    QSet<QString> seen;
    auto add = [&](const QVoice &v, const QLocale &fallbackLocale) {
        QString tag = voicematch::tagFromLocale(v.locale());
        if (tag.isEmpty())
            tag = voicematch::tagFromLocale(fallbackLocale);
        const QString key = v.name() + QLatin1Char('|') + tag;
        if (v.name().isEmpty() || seen.contains(key))
            return;
        seen.insert(key);
        out.append(Entry{v, voicematch::Candidate{v.name(), tag, genderString(v.gender())}});
    };

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    const QList<QVoice> all = tts->findVoices();
    for (const QVoice &v : all)
        add(v, QLocale::c());
#else
    // Qt < 6.6 only lists the voices of the current locale.
    const QSignalBlocker blocker(tts);
    const QLocale savedLocale = tts->locale();
    const QVoice savedVoice = tts->voice();
    const QList<QLocale> locales = tts->availableLocales();
    for (const QLocale &locale : locales) {
        tts->setLocale(locale);
        const QList<QVoice> voices = tts->availableVoices();
        for (const QVoice &v : voices)
            add(v, locale);
    }
    tts->setLocale(savedLocale);
    if (!savedVoice.name().isEmpty())
        tts->setVoice(savedVoice);
#endif
    return out;
}

bool SystemSpeechEngine::hasCantonese(const QList<Entry> &entries)
{
    return std::any_of(entries.cbegin(), entries.cend(), [](const Entry &e) {
        return voicematch::score(e.info, Language::Cantonese) > 0;
    });
}

QList<voicematch::Candidate> SystemSpeechEngine::candidates() const
{
    QList<voicematch::Candidate> list;
    list.reserve(m_entries.size());
    for (const Entry &e : m_entries)
        list.append(e.info);
    return list;
}

QString SystemSpeechEngine::id() const
{
    return QStringLiteral("system");
}

QString SystemSpeechEngine::displayName() const
{
#ifdef Q_OS_WIN
    return tr("Windows voices");
#else
    return tr("System voices");
#endif
}

bool SystemSpeechEngine::isAvailable() const
{
    // A backend stays in the Error state after a runtime failure (e.g. no
    // audio device) but recovers on the next say(); only a failed
    // initialization makes it unusable.
    return m_tts
           && !(m_tts->state() == QTextToSpeech::Error
                && m_tts->errorReason() == QTextToSpeech::ErrorReason::Initialization);
}

bool SystemSpeechEngine::hasVoiceFor(Language lang) const
{
    return isAvailable() && voicematch::pick(candidates(), lang) >= 0;
}

QList<VoiceInfo> SystemSpeechEngine::voices(Language lang) const
{
    QList<VoiceInfo> out;
    QSet<QString> names;
    const QList<voicematch::Candidate> ranked = voicematch::rank(candidates(), lang);
    for (const voicematch::Candidate &c : ranked) {
        if (names.contains(c.name))
            continue;
        names.insert(c.name);
        out.append(VoiceInfo{c.name, c.name, c.locale, c.gender});
    }
    return out;
}

void SystemSpeechEngine::setVoice(Language lang, const QString &voiceId)
{
    m_preferred[langIndex(lang)] = voiceId.trimmed();
}

void SystemSpeechEngine::setRate(double rate)
{
    m_rate = std::isnan(rate) ? 0.0 : std::clamp(rate, -1.0, 1.0);
}

SpeechEngine::State SystemSpeechEngine::state() const
{
    return m_state;
}

void SystemSpeechEngine::speak(const QString &text, Language lang)
{
    ++m_generation;
    m_startCheck->stop();
    stopBackend();

    if (!isAvailable()) {
        fail(lang == Language::Cantonese
                 ? voicematch::cantoneseVoiceHelpText()
                 : tr("No text-to-speech engine is available on this computer."));
        return;
    }
    const int index = voicematch::pick(candidates(), lang, m_preferred[langIndex(lang)]);
    if (index < 0) {
        fail(lang == Language::Cantonese
                 ? voicematch::cantoneseVoiceHelpText()
                 : tr("No English voice is installed. Add one in Windows Settings → Time & "
                      "language → Speech → Add voices."));
        return;
    }

    QString spoken = SsmlBuilder::cleanText(text);
    if (spoken.isEmpty()) {
        setState(State::Idle);
        return;
    }
    // Qt's SAPI backend sends the text as SAPI XML, so markup characters in
    // user text must be escaped. Other backends take plain text.
    if (m_backend == QLatin1String("sapi"))
        spoken = SsmlBuilder::escapeXml(spoken);

    const quint64 generation = m_generation;
    m_tts->setVoice(m_entries.at(index).voice);
    m_tts->setRate(m_rate);
    m_sawSpeaking = false;
    setState(State::Loading);
    if (generation != m_generation)
        return;
    const bool errorBefore = m_tts->state() == QTextToSpeech::Error;  // left over from last time
    m_tts->say(spoken);
    if (generation != m_generation)
        return;  // a synchronous errorOccurred() was already handled
    if (m_tts->state() == QTextToSpeech::Error && !errorBefore) {
        fail(m_tts->errorString().isEmpty() ? tr("The speech engine could not read this text.")
                                            : m_tts->errorString());
        return;
    }
    if (m_tts->state() == QTextToSpeech::Speaking) {
        m_sawSpeaking = true;
        setState(State::Speaking);
    }
}

void SystemSpeechEngine::stop()
{
    ++m_generation;
    m_startCheck->stop();
    stopBackend();
    setState(State::Idle);
}

void SystemSpeechEngine::stopBackend()
{
    if (!m_tts || m_state == State::Idle)
        return;
    // Signals emitted synchronously while stopping belong to the old text.
    m_stopping = true;
    m_tts->stop();
    m_stopping = false;
}

void SystemSpeechEngine::refreshVoices()
{
    if (!m_tts || m_state != State::Idle)
        return;
    m_entries = enumerateVoices(m_tts);
    emit voicesChanged();
}

void SystemSpeechEngine::onTtsStateChanged(int ttsState)
{
    const auto s = QTextToSpeech::State(ttsState);
    if (m_stopping)
        return;
    if (m_state == State::Idle) {
        // Some backends finish initializing asynchronously.
        if (s == QTextToSpeech::Ready && m_entries.isEmpty()) {
            m_entries = enumerateVoices(m_tts);
            if (!m_entries.isEmpty())
                emit voicesChanged();
        }
        return;
    }
    switch (s) {
    case QTextToSpeech::Speaking:
    case QTextToSpeech::Paused:
        m_sawSpeaking = true;
        m_startCheck->stop();
        setState(State::Speaking);
        break;
    case QTextToSpeech::Ready:
        if (m_sawSpeaking)
            setState(State::Idle);  // finished
        else
            m_startCheck->start();  // stale "Ready" from stopping the previous text?
        break;
    case QTextToSpeech::Error:
        fail(m_tts->errorString().isEmpty() ? tr("The speech engine reported an error.")
                                            : m_tts->errorString());
        break;
    default:  // Synthesizing (Qt >= 6.6)
        break;
    }
}

void SystemSpeechEngine::fail(const QString &message)
{
    const quint64 generation = ++m_generation;
    m_startCheck->stop();
    emit errorOccurred(message);
    if (generation == m_generation)
        setState(State::Idle);
}

void SystemSpeechEngine::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}

} // namespace sct
