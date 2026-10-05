#pragma once

// Pure SSML construction for Azure neural voices (unit-tested).

#include <QString>

namespace sct {

struct SsmlRequest
{
    QString text;
    QString voice;  // e.g. "zh-HK-HiuMaanNeural"
    QString lang;   // xml:lang, e.g. "zh-HK"; empty = derived from voice
    double rate = 0.0;  // -1.0 .. 1.0, mapped to -50% .. +50%
    int maxChars = 3000;
};

struct SsmlResult
{
    QString ssml;
    QString spokenText;  // cleaned (and possibly truncated) text that will be read
    bool truncated = false;
    int originalChars = 0;  // length of the cleaned text before truncation
};

class SsmlBuilder
{
public:
    // Azure stops synthesis after 10 minutes of audio; 3000 characters stays
    // well below that at any rate and keeps cached files small.
    static constexpr int DefaultMaxChars = 3000;
    static constexpr int LineBreakMs = 300;
    static constexpr int ParagraphBreakMs = 600;

    // Escapes & < > " ' for use in XML text and attribute values.
    static QString escapeXml(const QString &text);
    // Normalizes line endings, removes control / non-XML characters (keeps
    // '\n'), turns tabs into spaces and trims.
    static QString cleanText(const QString &text);
    // Cuts text to at most maxChars UTF-16 units, preferring a sentence or
    // word boundary near the end and never splitting a surrogate pair.
    static QString truncate(const QString &text, int maxChars, bool *truncated = nullptr);
    // -1..1 -> "-50%".."+50%" (clamped; NaN -> "+0%").
    static QString ratePercent(double rate);
    // "zh-HK-HiuMaanNeural" -> "zh-HK"; "" when it does not look like a voice name.
    static QString localeOfVoice(const QString &voiceName);

    static SsmlResult build(const SsmlRequest &request);
};

} // namespace sct
