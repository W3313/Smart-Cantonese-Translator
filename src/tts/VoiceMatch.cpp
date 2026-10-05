#include "tts/VoiceMatch.h"

#include <QCoreApplication>
#include <QLocale>
#include <QStringList>

#include <algorithm>
#include <utility>

namespace sct::voicematch {

namespace {

bool isAllLetters(QStringView s)
{
    return !s.isEmpty() && std::all_of(s.begin(), s.end(), [](QChar c) {
        return c.unicode() < 128 && c.isLetter();
    });
}

bool isAllDigits(QStringView s)
{
    return !s.isEmpty() && std::all_of(s.begin(), s.end(), [](QChar c) {
        return c >= QLatin1Char('0') && c <= QLatin1Char('9');
    });
}

bool isRegionSubtag(QStringView s)
{
    return (s.size() == 2 && isAllLetters(s)) || (s.size() == 3 && isAllDigits(s));
}

struct ParsedTag
{
    QString language;  // lower case, e.g. "zh"
    QString region;    // upper case, e.g. "HK"; empty when absent
    QStringList rest;  // other subtags (script, extlang, variants)
};

ParsedTag parse(const QString &tag)
{
    ParsedTag p;
    const QStringList parts = normalizeTag(tag).split(QLatin1Char('-'), Qt::SkipEmptyParts);
    for (qsizetype i = 0; i < parts.size(); ++i) {
        if (i == 0)
            p.language = parts.at(i);
        else if (p.region.isEmpty() && isRegionSubtag(parts.at(i)))
            p.region = parts.at(i);
        else
            p.rest << parts.at(i).toLower();
    }
    return p;
}

// Splits a voice name into lower-case word tokens ("Microsoft Tracy - Chinese
// (Traditional, Hong Kong S.A.R.)" -> microsoft, tracy, chinese, ...).
QStringList nameTokens(const QString &name)
{
    QStringList tokens;
    QString current;
    for (const QChar c : name) {
        if (c.isLetterOrNumber()) {
            current += c.toLower();
        } else if (!current.isEmpty()) {
            tokens << current;
            current.clear();
        }
    }
    if (!current.isEmpty())
        tokens << current;
    return tokens;
}

// Words that say the voice itself is Cantonese / Hong Kong.
bool hasStrongCantoneseHint(const QString &name)
{
    const QString lower = name.toLower();
    static const QString cjkHints[] = {
        QStringLiteral("粵"), QStringLiteral("粤"), QStringLiteral("廣東"),
        QStringLiteral("广东"), QStringLiteral("香港"),
    };
    for (const QString &h : cjkHints) {
        if (lower.contains(h))
            return true;
    }
    const QStringList tokens = nameTokens(name);
    if (tokens.contains(QLatin1String("cantonese")) || tokens.contains(QLatin1String("yue"))
        || tokens.contains(QLatin1String("hongkong")))
        return true;
    for (qsizetype i = 0; i + 1 < tokens.size(); ++i) {
        if (tokens.at(i) == QLatin1String("hong") && tokens.at(i + 1) == QLatin1String("kong"))
            return true;
    }
    return false;
}

// Known Cantonese voice persona names (Windows OneCore / desktop, Azure).
bool hasPersonaHint(const QString &name)
{
    static const char *const personas[] = {"tracy", "danny", "hiugaai", "hiumaan", "wanlung"};
    const QStringList tokens = nameTokens(name);
    for (const char *p : personas) {
        for (const QString &t : tokens) {
            // "HiuMaanNeural" / "zh-HK-HiuMaanNeural" style names
            if (t == QLatin1String(p) || t.startsWith(QLatin1String(p)))
                return true;
        }
    }
    return false;
}

int cantoneseScore(const Candidate &v)
{
    const ParsedTag tag = parse(v.locale);
    const int personaBonus = hasPersonaHint(v.name) ? 3 : 0;
    switch (classifyLocale(v.locale)) {
    case Dialect::Cantonese:
        if (tag.language == QLatin1String("zh") && tag.region == QLatin1String("HK"))
            return 100 + personaBonus;
        if (tag.language == QLatin1String("yue") || tag.rest.contains(QLatin1String("yue")))
            return 95 + personaBonus;
        return 85 + personaBonus;  // Macau
    case Dialect::Unknown:
        return (hasStrongCantoneseHint(v.name) || hasPersonaHint(v.name)) ? 60 : 0;
    case Dialect::Mandarin:
        // Never read Cantonese with a Mandarin voice; only trust an explicit
        // "Cantonese" / "Hong Kong" in the name (mislabelled locale).
        return hasStrongCantoneseHint(v.name) ? 50 : 0;
    case Dialect::English:
    case Dialect::Other:
        return 0;
    }
    return 0;
}

int englishScore(const Candidate &v)
{
    switch (classifyLocale(v.locale)) {
    case Dialect::English: {
        const QString region = parse(v.locale).region;
        if (region == QLatin1String("US"))
            return 100;
        if (region.isEmpty())
            return 90;
        if (region == QLatin1String("GB"))
            return 85;
        if (region == QLatin1String("AU") || region == QLatin1String("CA")
            || region == QLatin1String("IE") || region == QLatin1String("NZ"))
            return 80;
        return 70;
    }
    case Dialect::Unknown:
        return nameTokens(v.name).contains(QLatin1String("english")) ? 30 : 0;
    case Dialect::Cantonese:
    case Dialect::Mandarin:
    case Dialect::Other:
        return 0;
    }
    return 0;
}

} // namespace

QString normalizeTag(const QString &tag)
{
    QString t = tag.trimmed();
    // POSIX style "zh_HK.UTF-8" / "en_US@euro"
    for (const QChar sep : {QLatin1Char('.'), QLatin1Char('@')}) {
        const qsizetype cut = t.indexOf(sep);
        if (cut >= 0)
            t.truncate(cut);
    }
    t.replace(QLatin1Char('_'), QLatin1Char('-'));
    const QStringList parts = t.split(QLatin1Char('-'), Qt::SkipEmptyParts);
    QStringList out;
    out.reserve(parts.size());
    for (qsizetype i = 0; i < parts.size(); ++i) {
        const QString &p = parts.at(i);
        if (i == 0)
            out << p.toLower();
        else if (p.size() == 4 && isAllLetters(p))
            out << p.left(1).toUpper() + p.mid(1).toLower();
        else if (isRegionSubtag(p))
            out << p.toUpper();
        else
            out << p.toLower();
    }
    return out.join(QLatin1Char('-'));
}

QString tagFromLocale(const QLocale &locale)
{
    if (locale.language() == QLocale::C || locale.language() == QLocale::AnyLanguage)
        return QString();
    return normalizeTag(locale.name());
}

Dialect classifyLocale(const QString &tag)
{
    const ParsedTag p = parse(tag);
    if (p.language.isEmpty() || p.language == QLatin1String("c")
        || p.language == QLatin1String("und") || p.language == QLatin1String("posix"))
        return Dialect::Unknown;
    if (p.language == QLatin1String("yue"))
        return Dialect::Cantonese;
    if (p.language == QLatin1String("zh")) {
        if (p.rest.contains(QLatin1String("yue")) || p.region == QLatin1String("HK")
            || p.region == QLatin1String("MO"))
            return Dialect::Cantonese;
        return Dialect::Mandarin;
    }
    if (p.language == QLatin1String("cmn"))
        return Dialect::Mandarin;
    if (p.language == QLatin1String("en"))
        return Dialect::English;
    return Dialect::Other;
}

bool hasCantoneseNameHint(const QString &voiceName)
{
    return hasStrongCantoneseHint(voiceName) || hasPersonaHint(voiceName);
}

int score(const Candidate &voice, Language lang)
{
    return lang == Language::Cantonese ? cantoneseScore(voice) : englishScore(voice);
}

QList<Candidate> rank(const QList<Candidate> &voices, Language lang)
{
    QList<std::pair<int, qsizetype>> scored;
    for (qsizetype i = 0; i < voices.size(); ++i) {
        const int s = score(voices.at(i), lang);
        if (s > 0)
            scored.append({s, i});
    }
    std::stable_sort(scored.begin(), scored.end(),
                     [](const auto &a, const auto &b) { return a.first > b.first; });
    QList<Candidate> out;
    out.reserve(scored.size());
    for (const auto &entry : std::as_const(scored))
        out.append(voices.at(entry.second));
    return out;
}

int pick(const QList<Candidate> &voices, Language lang, const QString &preferredName)
{
    const QString preferred = preferredName.trimmed();
    if (!preferred.isEmpty()) {
        for (const Qt::CaseSensitivity cs : {Qt::CaseSensitive, Qt::CaseInsensitive}) {
            for (qsizetype i = 0; i < voices.size(); ++i) {
                if (voices.at(i).name.compare(preferred, cs) == 0 && score(voices.at(i), lang) > 0)
                    return int(i);
            }
        }
    }
    int best = -1;
    int bestScore = 0;
    for (qsizetype i = 0; i < voices.size(); ++i) {
        const int s = score(voices.at(i), lang);
        if (s > bestScore) {
            best = int(i);
            bestScore = s;
        }
    }
    return best;
}

QString cantoneseVoiceHelpText()
{
    return QCoreApplication::translate(
        "SpeechService",
        "Reading Cantonese aloud needs a Cantonese (Hong Kong) voice, and none is "
        "installed on this computer. There are two ways to get one:\n"
        "\n"
        "1. Install the free Windows Cantonese voice\n"
        "Open Windows Settings → Time & language → Speech. Under “Manage voices”, "
        "click “Add voices”, search for “Chinese (Traditional, Hong Kong SAR)” and "
        "install it. (Alternatively: Settings → Time & language → Language & region → "
        "Add a language → “Chinese (Traditional, Hong Kong SAR)”, with the "
        "“Text-to-speech” option ticked.) This adds the voices Microsoft Tracy and "
        "Microsoft Danny. Then restart Smart Cantonese Translator.\n"
        "\n"
        "2. Use Azure neural voices (much more natural)\n"
        "Create a free Azure Speech resource (the free F0 tier includes 500,000 "
        "characters a month), then enter its key and region in this app under "
        "Settings → Speech and choose “Azure neural voices”.");
}

} // namespace sct::voicematch
