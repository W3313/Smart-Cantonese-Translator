#pragma once

#include "core/TranslationTypes.h"

#include <QObject>
#include <QStringList>

namespace sct {

// Base class for an AI backend (Claude, OpenAI).
//
// Contract:
//  * translate() is asynchronous. For every call exactly one of finished() or
//    failed() is emitted later (never synchronously from inside translate()).
//  * Calling translate() while busy cancels the previous request first; the
//    cancelled request emits failed() with ErrorKind::Cancelled.
//  * listModels() is also asynchronous and doubles as a cheap "test connection"
//    (it costs no tokens). It emits modelsListed() or modelsListFailed().
class TranslationProvider : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~TranslationProvider() override = default;

    virtual QString id() const = 0;           // "claude" | "openai"
    virtual QString displayName() const = 0;  // "Claude (Anthropic)" | "OpenAI"

    virtual void setApiKey(const QString &key) = 0;
    virtual void setModel(const QString &model) = 0;
    virtual QString model() const = 0;
    // "fast" | "balanced" | "best" - mapped to the provider's effort/reasoning knob.
    virtual void setQuality(const QString &quality) = 0;

    virtual bool isConfigured() const = 0;  // has a non-empty API key and model
    virtual bool isBusy() const = 0;

    virtual void translate(const TranslationRequest &request) = 0;
    virtual void cancel() = 0;

    // Lists chat-capable model ids available to the configured key.
    // apiKeyOverride lets Settings test a key before it is saved.
    virtual void listModels(const QString &apiKeyOverride = QString()) = 0;

    // Models offered in the Settings combo box before listModels() runs.
    virtual QStringList suggestedModels() const = 0;
    virtual QString defaultModel() const = 0;

signals:
    void finished(const sct::TranslationResult &result);
    void failed(const sct::TranslationError &error);
    void modelsListed(const QStringList &models);
    void modelsListFailed(const sct::TranslationError &error);
};

} // namespace sct
