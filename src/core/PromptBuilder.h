#pragma once

#include "core/TranslationTypes.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace sct::PromptBuilder {

struct Prompt
{
    QString system;  // system / developer instructions
    QString user;    // user turn: settings + delimited source text
};

// The system prompt. It is identical for every request (direction, tone and
// script are stated in the user turn), which keeps it cacheable by providers.
QString systemPrompt();

// The user turn: a short block stating direction, tone, script and whether
// alternatives/notes are wanted, followed by the text in <source_text> tags.
QString userMessage(const TranslationRequest &request);

Prompt build(const TranslationRequest &request);

// Name used for the schema where the API wants one (OpenAI json_schema.name).
QString schemaName();

// The response JSON schema as compact JSON text. Properties are listed in the
// order the model should generate them (translation first, then jyutping...).
// The schema is strict-compatible: every object has additionalProperties:false
// and lists all of its properties in "required".
QByteArray responseSchemaJson();

// Same schema as a QJsonObject (note: QJsonObject sorts keys alphabetically,
// so do not serialize this into a request; use toJsonWithSchema()).
QJsonObject responseSchema();

// Placeholder string value providers put where the schema belongs in a request
// body; toJsonWithSchema() replaces it with responseSchemaJson() so property
// order survives serialization.
QString schemaPlaceholder();
QByteArray toJsonWithSchema(const QJsonObject &body);

} // namespace sct::PromptBuilder
