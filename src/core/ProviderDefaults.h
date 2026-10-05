#pragma once

// Provider ids and default/suggested model ids shared by AppSettings and the
// providers, so the defaults live in exactly one place.

#include <QString>
#include <QStringList>

namespace sct::ProviderDefaults {

inline QString claudeProviderId() { return QStringLiteral("claude"); }
inline QString openAiProviderId() { return QStringLiteral("openai"); }

// Anthropic: best quality by default. Ids are current as of Oct 2026; the
// Settings combo box is editable, so newer ids work without a code change.
inline QString claudeDefaultModel() { return QStringLiteral("claude-opus-5-5"); }
inline QStringList claudeSuggestedModels()
{
    return {QStringLiteral("claude-opus-5-5"),     // best quality (default)
            QStringLiteral("claude-sonnet-5-5"),   // faster / cheaper
            QStringLiteral("claude-haiku-4-5")};   // fastest / cheapest
}

// OpenAI: GPT-6.1 Sol is OpenAI's balanced GPT-6 model (near GPT-6 Astra
// quality at a fraction of the price) and supports Chat Completions,
// Structured Outputs and reasoning_effort low/medium/high.
inline QString openAiDefaultModel() { return QStringLiteral("gpt-6.1-sol"); }
inline QStringList openAiSuggestedModels()
{
    return {QStringLiteral("gpt-6.1-sol"),   // balanced flagship (default)
            QStringLiteral("gpt-6-astra"),   // highest quality, most expensive
            QStringLiteral("gpt-6-luna")};   // fastest / cheapest
}

inline QString defaultQuality() { return QStringLiteral("balanced"); }

} // namespace sct::ProviderDefaults
