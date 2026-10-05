#include "tts/AzureSpeechEngine.h"

#include "tts/SsmlBuilder.h"

#include <QAudioDevice>
#include <QAudioOutput>
#include <QBuffer>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace sct {

namespace {

std::size_t langIndex(Language lang)
{
    return lang == Language::Cantonese ? 0 : 1;
}

} // namespace

AzureSpeechEngine::AzureSpeechEngine(QNetworkAccessManager *nam, QObject *parent)
    : SpeechEngine(parent)
    , m_nam(nam)
{
}

AzureSpeechEngine::~AzureSpeechEngine()
{
    abortReply();
    if (m_player) {
        m_player->disconnect(this);
        m_player->stop();
        m_player->setSource(QUrl());
    }
}

QString AzureSpeechEngine::id() const
{
    return QStringLiteral("azure");
}

QString AzureSpeechEngine::displayName() const
{
    return tr("Azure neural voices (more natural)");
}

bool AzureSpeechEngine::isAvailable() const
{
    return !m_key.isEmpty() && !azure::normalizeRegion(m_region).isEmpty();
}

bool AzureSpeechEngine::hasVoiceFor(Language) const
{
    // Azure has neural voices for both languages; they are usable once configured.
    return isAvailable();
}

QList<VoiceInfo> AzureSpeechEngine::voices(Language lang) const
{
    QList<VoiceInfo> list = azure::voiceCatalog(lang);
    // Keep a valid custom voice (e.g. set by hand in the settings file) selectable.
    const QString configured = m_voices[langIndex(lang)].trimmed();
    if (!configured.isEmpty() && azure::resolveVoice(lang, configured) == configured
        && std::none_of(list.cbegin(), list.cend(),
                        [&](const VoiceInfo &v) { return v.id == configured; })) {
        list.append(VoiceInfo{configured, configured, SsmlBuilder::localeOfVoice(configured),
                              QString()});
    }
    return list;
}

void AzureSpeechEngine::setVoice(Language lang, const QString &voiceId)
{
    m_voices[langIndex(lang)] = voiceId.trimmed();
}

QString AzureSpeechEngine::voiceFor(Language lang) const
{
    return azure::resolveVoice(lang, m_voices[langIndex(lang)]);
}

void AzureSpeechEngine::setRate(double rate)
{
    m_rate = std::isnan(rate) ? 0.0 : std::clamp(rate, -1.0, 1.0);
}

void AzureSpeechEngine::setCredentials(const QString &key, const QString &region)
{
    m_key = key.trimmed();
    m_region = azure::normalizeRegion(region);
}

void AzureSpeechEngine::setCacheDirectory(const QString &directory)
{
    m_cache.setDirectory(directory);
}

QString AzureSpeechEngine::cacheDirectory() const
{
    return m_cache.directory();
}

SpeechEngine::State AzureSpeechEngine::state() const
{
    return m_state;
}

void AzureSpeechEngine::speak(const QString &text, Language lang)
{
    stop();
    m_lastFailure = {};
    if (!isAvailable()) {
        fail(azure::FailureKind::NotConfigured,
             tr("Azure Speech is not set up. Add your Speech key and region in Settings → Speech."));
        return;
    }
    start(Job{m_key, m_region, voiceFor(lang), text, m_rate, false});
}

void AzureSpeechEngine::test(const QString &key, const QString &region, const QString &voiceId)
{
    stop();
    m_lastFailure = {};
    const QString k = key.trimmed();
    const QString r = azure::normalizeRegion(region);
    if (k.isEmpty() || r.isEmpty()) {
        m_lastFailure = {azure::FailureKind::NotConfigured,
                         k.isEmpty() ? tr("Enter your Azure Speech key first.")
                                     : tr("Enter your Azure Speech region, for example “eastasia” "
                                          "(shown next to the key in the Azure portal).")};
        emit testFinished(false, m_lastFailure.message);
        return;
    }
    const QString voice = voiceId.trimmed().isEmpty() ? azure::defaultVoice(Language::Cantonese)
                                                      : voiceId.trimmed();
    m_testRunning = true;
    start(Job{k, r, voice, azure::sampleText(voice), m_rate, true});
}

void AzureSpeechEngine::start(const Job &job)
{
    const quint64 generation = m_generation;

    SsmlRequest request;
    request.text = job.text;
    request.voice = job.voice;
    request.rate = job.rate;
    request.maxChars = SsmlBuilder::DefaultMaxChars;
    const SsmlResult ssml = SsmlBuilder::build(request);
    if (ssml.spokenText.isEmpty()) {
        m_testRunning = false;
        if (job.isTest)
            emit testFinished(false, tr("Nothing to read."));
        return;
    }
    if (ssml.truncated) {
        emit notice(tr("This text is long - only the first %1 of %2 characters will be read aloud.")
                        .arg(QString::number(ssml.spokenText.size()),
                             QString::number(ssml.originalChars)));
        if (generation != m_generation)
            return;  // a slot called stop()/speak()
    }

    const QString cacheKey = AudioCache::key(job.voice, job.rate, ssml.spokenText);
    if (!job.isTest) {
        const QString cached = m_cache.lookup(cacheKey);
        if (!cached.isEmpty()) {
            m_playingKey = cacheKey;
            play(cached);
            return;
        }
    }

    if (!m_nam) {
        m_testRunning = false;
        fail(azure::FailureKind::Network, tr("Networking is not available."));
        return;
    }

    QNetworkRequest req(m_endpointOverride.isEmpty() ? azure::synthesisUrl(job.region)
                                                     : m_endpointOverride);
    req.setRawHeader(QByteArrayLiteral("Ocp-Apim-Subscription-Key"), job.key.toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/ssml+xml"));
    req.setRawHeader(QByteArrayLiteral("X-Microsoft-OutputFormat"), QByteArray(azure::OutputFormat));
    req.setHeader(QNetworkRequest::UserAgentHeader, QByteArray(azure::UserAgent));
    req.setTransferTimeout(azure::TransferTimeoutMs);

    setState(State::Loading);
    QNetworkReply *reply = m_nam->post(req, ssml.ssml.toUtf8());
    m_reply = reply;
    const QString region = job.region;
    const QString voice = job.voice;
    const bool isTest = job.isTest;
    connect(reply, &QNetworkReply::finished, this, [this, reply, cacheKey, isTest, voice, region] {
        reply->deleteLater();
        if (reply != m_reply)
            return;  // stopped or superseded
        m_reply = nullptr;

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool ok = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
        const QByteArray data = ok ? reply->readAll() : QByteArray();
        azure::Failure failure;
        if (ok) {
            const QString contentType =
                reply->header(QNetworkRequest::ContentTypeHeader).toString().trimmed();
            if (data.isEmpty()) {
                failure = {azure::FailureKind::Server,
                           tr("Azure Speech returned no audio. Please try again.")};
            } else if (!contentType.isEmpty()
                       && !contentType.startsWith(QLatin1String("audio/"), Qt::CaseInsensitive)) {
                // e.g. a Wi-Fi sign-in page answering instead of Azure
                failure = {azure::FailureKind::Network,
                           tr("Azure Speech returned an unexpected response instead of audio. "
                              "Check your internet connection (a Wi-Fi sign-in page?).")};
            }
        } else {
            failure = azure::describeFailure(status, reply->error(), region, reply->errorString());
            if (failure.kind == azure::FailureKind::None)
                failure = {azure::FailureKind::Server,
                           tr("Azure Speech request failed (HTTP %1).").arg(status)};
        }

        if (failure.kind != azure::FailureKind::None) {
            if (isTest) {
                m_testRunning = false;
                m_lastFailure = failure;
                const quint64 gen = m_generation;
                emit testFinished(false, failure.message);
                if (gen == m_generation)
                    setState(State::Idle);
            } else {
                fail(failure.kind, failure.message);
            }
            return;
        }

        QString storeError;
        const QString path = m_cache.store(cacheKey, data, &storeError);
        m_playingKey = path.isEmpty() ? QString() : cacheKey;
        if (isTest) {
            m_testRunning = false;
            const quint64 gen = m_generation;
            emit testFinished(true, tr("Azure Speech is working. Playing a sample with %1.").arg(voice));
            if (gen != m_generation)
                return;
        }
        play(path, path.isEmpty() ? data : QByteArray());
    });
}

bool AzureSpeechEngine::ensurePlayer()
{
    // Never create a QMediaPlayer without an audio device: some backends
    // (e.g. GStreamer on headless Linux) crash in that case.
    if (QMediaDevices::audioOutputs().isEmpty())
        return false;
    if (!m_player) {
        m_player = new QMediaPlayer(this);
        m_audioOutput = new QAudioOutput(this);
        m_player->setAudioOutput(m_audioOutput);
        connect(m_player, &QMediaPlayer::mediaStatusChanged, this,
                [this](QMediaPlayer::MediaStatus status) { onMediaStatusChanged(int(status)); });
        connect(m_player, &QMediaPlayer::playbackStateChanged, this,
                [this](QMediaPlayer::PlaybackState playbackState) {
                    if (playbackState == QMediaPlayer::StoppedState && m_state == State::Speaking
                        && m_player->mediaStatus() == QMediaPlayer::EndOfMedia)
                        setState(State::Idle);
                });
        connect(m_player, &QMediaPlayer::errorOccurred, this,
                [this](QMediaPlayer::Error error, const QString &errorString) {
                    if (m_state != State::Speaking)
                        return;
                    if (error == QMediaPlayer::FormatError && !m_playingKey.isEmpty())
                        m_cache.remove(m_playingKey);  // corrupt download: fetch again next time
                    onPlayerError(errorString);
                });
    }
    // Follow the current default output (headphones plugged in, etc.).
    m_audioOutput->setDevice(QMediaDevices::defaultAudioOutput());
    return true;
}

void AzureSpeechEngine::play(const QString &filePath, const QByteArray &fallbackData)
{
    if (!ensurePlayer()) {
        fail(azure::FailureKind::NoAudioDevice,
             tr("No audio output device was found. Connect speakers or headphones and try again."));
        return;
    }
    stopPlayer();
    if (!filePath.isEmpty()) {
        m_player->setSource(QUrl::fromLocalFile(filePath));
    } else {
        // Cache not writable: play from memory.
        m_buffer = new QBuffer(this);
        m_buffer->setData(fallbackData);
        m_buffer->open(QIODevice::ReadOnly);
        m_player->setSourceDevice(m_buffer, QUrl(QStringLiteral("speech.mp3")));
    }
    setState(State::Speaking);
    m_player->play();
}

void AzureSpeechEngine::onMediaStatusChanged(int status)
{
    if (m_state != State::Speaking)
        return;
    switch (QMediaPlayer::MediaStatus(status)) {
    case QMediaPlayer::EndOfMedia:
        setState(State::Idle);
        break;
    case QMediaPlayer::InvalidMedia:
        if (!m_playingKey.isEmpty())
            m_cache.remove(m_playingKey);
        fail(azure::FailureKind::Playback, tr("The downloaded audio could not be played."));
        break;
    default:
        break;
    }
}

void AzureSpeechEngine::onPlayerError(const QString &errorString)
{
    fail(azure::FailureKind::Playback, tr("Could not play the audio: %1").arg(errorString));
}

void AzureSpeechEngine::fail(azure::FailureKind kind, const QString &message)
{
    m_lastFailure = {kind, message};
    const quint64 generation = ++m_generation;
    abortReply();
    stopPlayer();
    emit errorOccurred(message);
    // A slot may already have started something new (e.g. the system voice
    // fallback or a retry); only go Idle if not.
    if (generation == m_generation)
        setState(State::Idle);
}

void AzureSpeechEngine::stop()
{
    ++m_generation;
    m_testRunning = false;
    abortReply();
    stopPlayer();
    setState(State::Idle);
}

void AzureSpeechEngine::abortReply()
{
    if (!m_reply)
        return;
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    reply->disconnect(this);
    reply->abort();
    reply->deleteLater();
}

void AzureSpeechEngine::stopPlayer()
{
    if (!m_player)
        return;
    const State saved = m_state;
    m_state = State::Idle;  // ignore player signals caused by stopping
    m_player->stop();
    if (m_buffer || !m_player->source().isEmpty())
        m_player->setSource(QUrl());  // releases the file / buffer
    m_state = saved;
    if (m_buffer) {
        m_buffer->deleteLater();
        m_buffer = nullptr;
    }
}

void AzureSpeechEngine::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}

} // namespace sct
