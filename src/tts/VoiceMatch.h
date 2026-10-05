#pragma once

// Pure voice classification / ranking logic used by SystemSpeechEngine.
// Kept free of QTextToSpeech so it can be unit-tested with plain data.

#include "core/TranslationTypes.h"

#include <QList>
#include <QString>

class QLocale;

namespace sct::voicematch {

// One installed voice as reported by a speech backend.
struct Candidate
{
    QString name;    // e.g. "Microsoft Tracy"
    QString locale;  // tag as reported, e.g. "zh-HK", "zh_HK", "yue-HK"; may be empty
    QString gender;  // "Female" | "Male" | ""
};

enum class Dialect {
    Unknown,    // empty / "C" / "und" locale
    Cantonese,  // zh-HK, zh-MO, yue-*, zh-yue
    Mandarin,   // zh-CN, zh-TW, zh-SG, plain zh, cmn-*
    English,    // en-*
    Other
};

// "zh_HK" / "ZH-hant-hk" -> "zh-HK" / "zh-Hant-HK" (language lower case,
// script title case, region upper case, '-' separators).
QString normalizeTag(const QString &tag);
// Locale tag for a QLocale that keeps the region ("en-US", "zh-HK", "yue-HK").
// Note QLocale::bcp47Name() drops likely regions ("en-US" -> "en"), so it is
// not used here.
QString tagFromLocale(const QLocale &locale);

Dialect classifyLocale(const QString &tag);

// Person names / words that identify Cantonese voices ("Tracy", "Danny",
// "HiuGaai", "HiuMaan", "WanLung", "Hong Kong", "Cantonese", ...).
bool hasCantoneseNameHint(const QString &voiceName);

// Suitability of a voice for reading lang; 0 = must not be used.
// Cantonese: zh-HK > yue-* > zh-MO; a Mandarin (zh-CN / zh-TW) voice is never
// usable for Cantonese text unless its name explicitly says Cantonese / Hong Kong.
// English: en-US > en > en-GB > other en-*.
int score(const Candidate &voice, Language lang);

// Usable voices for lang, best first (stable for equal scores).
QList<Candidate> rank(const QList<Candidate> &voices, Language lang);

// Index into voices of the voice to use for lang: the user's choice
// (preferredName) when it is installed and usable, else the best match.
// -1 when no voice can read lang.
int pick(const QList<Candidate> &voices, Language lang, const QString &preferredName = QString());

// Friendly multi-line help text explaining how to get a Cantonese voice.
QString cantoneseVoiceHelpText();

} // namespace sct::voicematch
