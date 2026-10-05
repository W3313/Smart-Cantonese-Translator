#pragma once

// Shared value types used by every module (core, tts, ui).
// Implementation of the non-inline functions lives in TranslationTypes.cpp.

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

namespace sct {

enum class Language { English, Cantonese };

enum class Direction { EnglishToCantonese, CantoneseToEnglish };

inline Language sourceLanguage(Direction d)
{
    return d == Direction::EnglishToCantonese ? Language::English : Language::Cantonese;
}

inline Language targetLanguage(Direction d)
{
    return d == Direction::EnglishToCantonese ? Language::Cantonese : Language::English;
}

inline Direction reversed(Direction d)
{
    return d == Direction::EnglishToCantonese ? Direction::CantoneseToEnglish
                                              : Direction::EnglishToCantonese;
}

// BCP-47 tag used for speech: "en-US" or "zh-HK".
QString languageTag(Language lang);
// Human-readable name for UI: "English" or "Cantonese (廣東話)".
QString languageDisplayName(Language lang);

// Register / politeness of the Cantonese output.
enum class Tone { Casual, Neutral, Polite };

enum class ChineseScript { Traditional, Simplified };

// Stable string ids for persistence ("casual", "traditional", "en2yue", ...).
QString toString(Direction d);
QString toString(Tone t);
QString toString(ChineseScript s);
Direction directionFromString(const QString &s, Direction fallback = Direction::EnglishToCantonese);
Tone toneFromString(const QString &s, Tone fallback = Tone::Neutral);
ChineseScript scriptFromString(const QString &s, ChineseScript fallback = ChineseScript::Traditional);

struct TranslationRequest
{
    QString text;
    Direction direction = Direction::EnglishToCantonese;
    Tone tone = Tone::Neutral;
    ChineseScript script = ChineseScript::Traditional;
    bool wantAlternatives = true;
    bool wantNotes = true;

    // Key for the in-memory result cache (does not include provider/model;
    // TranslationService adds those).
    QString cacheKey() const;

    QJsonObject toJson() const;
    static TranslationRequest fromJson(const QJsonObject &obj);
};

struct Alternative
{
    QString text;      // alternative phrasing in the target language
    QString jyutping;  // Jyutping for it when it is Cantonese, empty otherwise
    QString note;      // short English explanation of when/why to use it
};

struct TranslationResult
{
    QString translation;              // main translation, in the target language
    QString jyutping;                 // Jyutping of the Cantonese side (target for EN->YUE, source for YUE->EN)
    QString literal;                  // optional literal / word-by-word gloss in English (may be empty)
    QList<Alternative> alternatives;  // other natural ways to say it (may be empty)
    QStringList notes;                // short English notes on slang, particles, culture (may be empty)

    QString providerId;  // "claude" | "openai"
    QString model;       // model id that produced it
    TranslationRequest request;
    QDateTime timestamp;  // UTC
    bool fromCache = false;

    bool isValid() const { return !translation.trimmed().isEmpty(); }

    QJsonObject toJson() const;
    static TranslationResult fromJson(const QJsonObject &obj);
};

enum class ErrorKind {
    NotConfigured,  // missing API key / provider not set up
    Network,        // DNS, TLS, connection refused, offline
    Timeout,
    Auth,           // 401/403 - bad key
    RateLimited,    // 429
    Server,         // 5xx / 529 overloaded
    Refused,        // model declined (stop_reason refusal / OpenAI refusal)
    BadResponse,    // unparseable or truncated output
    InvalidRequest, // 400 - e.g. unknown model id
    Cancelled
};

struct TranslationError
{
    ErrorKind kind = ErrorKind::BadResponse;
    QString message;  // short, user-facing, actionable
    QString detail;   // technical detail for a "Details" expander / logs
    int httpStatus = 0;
};

// Registers the sct value types with Qt's meta-type system (needed for queued
// signal connections). Idempotent; TranslationService calls it on construction.
void registerMetaTypes();

} // namespace sct

Q_DECLARE_METATYPE(sct::TranslationRequest)
Q_DECLARE_METATYPE(sct::TranslationResult)
Q_DECLARE_METATYPE(sct::TranslationError)
