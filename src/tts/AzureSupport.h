#pragma once

// Pure helpers for the Azure Speech text-to-speech REST API (unit-tested).
//
// REST reference (verified 2026-10):
// https://learn.microsoft.com/azure/ai-services/speech-service/rest-text-to-speech
//   POST https://{region}.tts.speech.microsoft.com/cognitiveservices/v1
//   Ocp-Apim-Subscription-Key: <key>
//   Content-Type: application/ssml+xml
//   X-Microsoft-OutputFormat: audio-24khz-48kbitrate-mono-mp3
//   User-Agent: <app name, < 255 chars>
// Voice names: https://learn.microsoft.com/azure/ai-services/speech-service/language-support?tabs=tts

#include "core/TranslationTypes.h"
#include "tts/SpeechEngine.h"

#include <QList>
#include <QNetworkReply>
#include <QString>
#include <QUrl>

namespace sct::azure {

inline constexpr int TransferTimeoutMs = 20000;
inline constexpr const char *OutputFormat = "audio-24khz-48kbitrate-mono-mp3";
inline constexpr const char *UserAgent = "SmartCantoneseTranslator";
inline constexpr const char *DefaultRegion = "eastasia";

// "East Asia" / " EASTASIA " / "https://eastasia.api.cognitive.microsoft.com/"
// -> "eastasia". Empty when it cannot be a region name.
QString normalizeRegion(const QString &region);

// https://{region}.tts.speech.microsoft.com/cognitiveservices/v1
QUrl synthesisUrl(const QString &region);
// https://{region}.tts.speech.microsoft.com/cognitiveservices/voices/list
QUrl voicesListUrl(const QString &region);

// Curated voices shown in the Settings combo boxes (default first).
QList<VoiceInfo> voiceCatalog(Language lang);
// zh-HK-HiuMaanNeural / en-US-AvaMultilingualNeural
QString defaultVoice(Language lang);
// requested if it is a voice for lang (zh-HK-* / yue-* for Cantonese, en-*
// for English), else defaultVoice(lang).
QString resolveVoice(Language lang, const QString &requested);
// Short sample sentence in the voice's language, for the Settings "Test" button.
QString sampleText(const QString &voiceName);

enum class FailureKind {
    None,
    NotConfigured,  // no key / region
    Network,        // offline, DNS, TLS, connection refused
    Timeout,
    Auth,           // 401 / 403: bad key or key from another region
    RateLimited,    // 429: too many requests / free tier quota used up
    BadRequest,     // 400 / 404 / 415: bad SSML, unknown voice, bad region
    Server,         // 5xx, empty audio
    NoAudioDevice,
    Playback        // audio could not be decoded / played
};

struct Failure
{
    FailureKind kind = FailureKind::None;
    QString message;  // short, user-facing, actionable
};

// Maps a finished request to a user-facing failure. httpStatus is 0 when no
// HTTP response arrived. Returns kind None for HTTP 2xx without a network error.
Failure describeFailure(int httpStatus, QNetworkReply::NetworkError error, const QString &region,
                        const QString &errorString = QString());

} // namespace sct::azure
