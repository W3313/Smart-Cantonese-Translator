#pragma once

#include "core/TranslationTypes.h"

#include <QString>

namespace sct {

// Outcome of parsing a provider response. On success `result` holds the
// translation fields (provider/model/timestamp are stamped by the caller);
// otherwise `error` describes the failure.
struct ParsedTranslation
{
    bool ok = false;
    TranslationResult result;
    TranslationError error;

    static ParsedTranslation success(const TranslationResult &r)
    {
        ParsedTranslation p;
        p.ok = true;
        p.result = r;
        return p;
    }
    static ParsedTranslation failure(ErrorKind kind, const QString &message, const QString &detail = QString())
    {
        ParsedTranslation p;
        p.error.kind = kind;
        p.error.message = message;
        p.error.detail = detail;
        return p;
    }
};

namespace ResponseParser {

constexpr int kMaxAlternatives = 3;
constexpr int kMaxNotes = 3;

// Finds the outermost JSON object in model output, tolerating ```json fences
// and prose before/after it. Returns an empty string when there is none.
QString extractJsonObject(const QString &text);

// Parses the model's JSON answer ({translation, jyutping, literal,
// alternatives, notes}) into a result. Strings are trimmed, empty items
// dropped, alternatives/notes capped at 3 and cleared when the request did not
// ask for them. result.request is set to `request`. Fails with BadResponse.
ParsedTranslation parse(const QString &modelText, const TranslationRequest &request);

} // namespace ResponseParser

} // namespace sct
