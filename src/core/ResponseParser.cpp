#include "core/ResponseParser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>

namespace sct::ResponseParser {

namespace {

constexpr int kMaxCandidates = 32;  // '{' positions to try before giving up

QString normalizeNewlines(const QString &s)
{
    QString out = s;
    out.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    out.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return out;
}

// Trims the text and collapses runs of spaces inside each line while keeping
// line breaks (Jyutping mirrors the line structure of the Cantonese text).
QString cleanLines(const QString &s)
{
    QStringList lines = normalizeNewlines(s).split(QLatin1Char('\n'));
    for (QString &line : lines)
        line = line.simplified();
    return lines.join(QLatin1Char('\n')).trimmed();
}

QString cleanText(const QString &s)
{
    return normalizeNewlines(s).trimmed();
}

// Index of the brace closing the object that starts at `start`, honouring JSON
// strings and escapes; -1 when unbalanced.
qsizetype matchingBrace(const QString &text, qsizetype start)
{
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (qsizetype i = start; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (inString) {
            if (escaped)
                escaped = false;
            else if (c == QLatin1Char('\\'))
                escaped = true;
            else if (c == QLatin1Char('"'))
                inString = false;
            continue;
        }
        if (c == QLatin1Char('"')) {
            inString = true;
        } else if (c == QLatin1Char('{')) {
            ++depth;
        } else if (c == QLatin1Char('}')) {
            if (--depth == 0)
                return i;
        }
    }
    return -1;
}

bool isJsonObject(const QString &candidate)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(candidate.toUtf8(), &err);
    return err.error == QJsonParseError::NoError && doc.isObject();
}

QString excerpt(const QString &text)
{
    constexpr qsizetype kMax = 300;
    return text.size() <= kMax ? text : text.left(kMax) + QStringLiteral("…");
}

ParsedTranslation badResponse(const QString &detail)
{
    return ParsedTranslation::failure(ErrorKind::BadResponse,
                                      QStringLiteral("The AI's answer couldn't be read. Please try again."),
                                      detail);
}

} // namespace

QString extractJsonObject(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return {};
    if (trimmed.startsWith(QLatin1Char('{')) && isJsonObject(trimmed))
        return trimmed;

    qsizetype from = 0;
    for (int attempt = 0; attempt < kMaxCandidates; ++attempt) {
        const qsizetype start = trimmed.indexOf(QLatin1Char('{'), from);
        if (start < 0)
            break;
        const qsizetype end = matchingBrace(trimmed, start);
        if (end > start) {
            const QString candidate = trimmed.mid(start, end - start + 1);
            if (isJsonObject(candidate))
                return candidate;
        }
        from = start + 1;
    }
    return {};
}

ParsedTranslation parse(const QString &modelText, const TranslationRequest &request)
{
    const QString json = extractJsonObject(modelText);
    if (json.isEmpty())
        return badResponse(QStringLiteral("No JSON object found in the model output: ") + excerpt(modelText));

    const QJsonObject obj = QJsonDocument::fromJson(json.toUtf8()).object();
    const QJsonValue translationValue = obj.value(QStringLiteral("translation"));
    if (!translationValue.isString())
        return badResponse(QStringLiteral("The model output has no \"translation\" string: ") + excerpt(json));

    TranslationResult r;
    r.request = request;
    r.translation = cleanText(translationValue.toString());
    if (r.translation.isEmpty())
        return badResponse(QStringLiteral("The model returned an empty translation: ") + excerpt(json));

    r.jyutping = cleanLines(obj.value(QStringLiteral("jyutping")).toString());
    r.literal = cleanText(obj.value(QStringLiteral("literal")).toString());

    const bool targetIsCantonese = targetLanguage(request.direction) == Language::Cantonese;

    if (request.wantAlternatives) {
        const QJsonArray alts = obj.value(QStringLiteral("alternatives")).toArray();
        QStringList seen{r.translation};
        for (const QJsonValue &v : alts) {
            Alternative a;
            if (v.isObject()) {
                const QJsonObject ao = v.toObject();
                a.text = cleanText(ao.value(QStringLiteral("text")).toString());
                a.jyutping = cleanLines(ao.value(QStringLiteral("jyutping")).toString());
                a.note = cleanText(ao.value(QStringLiteral("note")).toString());
            } else if (v.isString()) {
                a.text = cleanText(v.toString());
            }
            if (a.text.isEmpty() || seen.contains(a.text))
                continue;
            if (!targetIsCantonese)
                a.jyutping.clear();  // English alternatives have no romanisation
            seen.append(a.text);
            r.alternatives.append(a);
            if (r.alternatives.size() >= kMaxAlternatives)
                break;
        }
    }

    if (request.wantNotes) {
        const QJsonValue notesValue = obj.value(QStringLiteral("notes"));
        QJsonArray notes;
        if (notesValue.isArray())
            notes = notesValue.toArray();
        else if (notesValue.isString())
            notes.append(notesValue);
        for (const QJsonValue &v : notes) {
            const QString note = cleanText(v.toString());
            if (note.isEmpty() || r.notes.contains(note))
                continue;
            r.notes.append(note);
            if (r.notes.size() >= kMaxNotes)
                break;
        }
    }

    return ParsedTranslation::success(r);
}

} // namespace sct::ResponseParser
