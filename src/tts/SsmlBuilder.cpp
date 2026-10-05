#include "tts/SsmlBuilder.h"

#include <QStringList>

#include <algorithm>
#include <cmath>

namespace sct {

namespace {

bool isSentenceEnd(QChar c)
{
    switch (c.unicode()) {
    case u'.': case u'!': case u'?': case u';': case u'\n':
    case u'。':  // 。
    case u'！':  // ！
    case u'？':  // ？
    case u'；':  // ；
    case u'…':  // …
        return true;
    default:
        return false;
    }
}

bool isSoftBreak(QChar c)
{
    switch (c.unicode()) {
    case u',':
    case u'，':  // ，
    case u'、':  // 、
        return true;
    default:
        return c.isSpace();
    }
}

} // namespace

QString SsmlBuilder::escapeXml(const QString &text)
{
    QString out;
    out.reserve(text.size() + text.size() / 8);
    for (const QChar c : text) {
        switch (c.unicode()) {
        case u'&': out += QLatin1String("&amp;"); break;
        case u'<': out += QLatin1String("&lt;"); break;
        case u'>': out += QLatin1String("&gt;"); break;
        case u'"': out += QLatin1String("&quot;"); break;
        case u'\'': out += QLatin1String("&apos;"); break;
        default: out += c; break;
        }
    }
    return out;
}

QString SsmlBuilder::cleanText(const QString &text)
{
    QString out;
    out.reserve(text.size());
    const qsizetype n = text.size();
    for (qsizetype i = 0; i < n; ++i) {
        const QChar c = text.at(i);
        const char16_t u = c.unicode();
        if (u == u'\r') {
            out += QLatin1Char('\n');
            if (i + 1 < n && text.at(i + 1) == QLatin1Char('\n'))
                ++i;
        } else if (u == u'\n' || u == u' ' || u == u' ' || u == u'\u0085') {
            out += QLatin1Char('\n');
        } else if (u == u'\t' || u == u'\v' || u == u'\f') {
            out += QLatin1Char(' ');
        } else if (u < 0x20 || (u >= 0x7F && u <= 0x9F) || u == 0xFEFF || u == 0xFFFE
                   || u == 0xFFFF) {
            // control characters, BOM and XML-invalid noncharacters: drop
        } else if (c.isHighSurrogate()) {
            if (i + 1 < n && text.at(i + 1).isLowSurrogate()) {
                out += c;
                out += text.at(i + 1);
                ++i;
            }  // else: unpaired surrogate (invalid in XML) - drop
        } else if (c.isLowSurrogate()) {
            // unpaired low surrogate - drop
        } else {
            out += c;
        }
    }

    // Trim trailing spaces on each line and the text as a whole.
    QStringList lines = out.split(QLatin1Char('\n'));
    for (QString &line : lines) {
        while (!line.isEmpty() && line.back().isSpace())
            line.chop(1);
    }
    return lines.join(QLatin1Char('\n')).trimmed();
}

QString SsmlBuilder::truncate(const QString &text, int maxChars, bool *truncated)
{
    if (truncated)
        *truncated = false;
    if (maxChars <= 0 || text.size() <= maxChars)
        return text;
    if (truncated)
        *truncated = true;

    qsizetype cut = maxChars;
    if (text.at(cut - 1).isHighSurrogate())
        --cut;  // do not split a surrogate pair

    // Prefer ending at a sentence boundary in the last 40%, else at a word /
    // clause boundary in the last 20%.
    const qsizetype sentenceFloor = qsizetype(cut * 0.6);
    for (qsizetype i = cut - 1; i >= sentenceFloor && i > 0; --i) {
        if (isSentenceEnd(text.at(i)))
            return text.left(i + 1).trimmed();
    }
    const qsizetype softFloor = qsizetype(cut * 0.8);
    for (qsizetype i = cut - 1; i >= softFloor && i > 0; --i) {
        if (isSoftBreak(text.at(i)))
            return text.left(i + 1).trimmed();
    }
    return text.left(cut).trimmed();
}

QString SsmlBuilder::ratePercent(double rate)
{
    if (std::isnan(rate))
        rate = 0.0;
    rate = std::clamp(rate, -1.0, 1.0);
    const long percent = std::lround(rate * 50.0);
    return percent < 0 ? QStringLiteral("-%1%").arg(-percent) : QStringLiteral("+%1%").arg(percent);
}

QString SsmlBuilder::localeOfVoice(const QString &voiceName)
{
    // Azure voice names look like "<lang>-<REGION>-<Name>Neural".
    const QStringList parts = voiceName.trimmed().split(QLatin1Char('-'));
    if (parts.size() < 3 || parts.at(0).size() < 2 || parts.at(0).size() > 3
        || parts.at(1).size() != 2)
        return QString();
    return parts.at(0).toLower() + QLatin1Char('-') + parts.at(1).toUpper();
}

SsmlResult SsmlBuilder::build(const SsmlRequest &request)
{
    SsmlResult result;
    const QString cleaned = cleanText(request.text);
    result.originalChars = int(cleaned.size());
    result.spokenText = truncate(cleaned, request.maxChars, &result.truncated);

    QString lang = request.lang.trimmed();
    if (lang.isEmpty())
        lang = localeOfVoice(request.voice);
    if (lang.isEmpty())
        lang = QStringLiteral("en-US");

    // Lines become short pauses, blank lines (paragraphs) longer ones.
    QString body;
    const QStringList lines = result.spokenText.split(QLatin1Char('\n'));
    int pendingBreaks = 0;
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            ++pendingBreaks;
            continue;
        }
        if (!body.isEmpty()) {
            const int ms = pendingBreaks > 0 ? ParagraphBreakMs : LineBreakMs;
            body += QStringLiteral("<break time=\"%1ms\"/>").arg(ms);
        }
        body += escapeXml(line);
        pendingBreaks = 0;
    }

    result.ssml = QStringLiteral("<speak version=\"1.0\" xmlns=\"http://www.w3.org/2001/10/synthesis\" "
                                 "xml:lang=\"%1\"><voice name=\"%2\"><prosody rate=\"%3\">%4</prosody>"
                                 "</voice></speak>")
                      .arg(escapeXml(lang), escapeXml(request.voice.trimmed()),
                           ratePercent(request.rate), body);
    return result;
}

} // namespace sct
