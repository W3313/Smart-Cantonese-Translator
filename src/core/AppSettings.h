#pragma once

#include "core/TranslationTypes.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVariant>

class QSettings;

namespace sct {

// Typed wrapper around QSettings. All modules read configuration through this.
// API keys are stored encrypted through SecretStore (DPAPI on Windows).
//
// Setters persist immediately and emit changed() once per call when the value
// actually changes. Call beginBatch()/endBatch() around many setters (e.g. the
// Settings dialog "OK" button) to emit changed() only once.
class AppSettings : public QObject
{
    Q_OBJECT

public:
    // Uses the platform default location (registry on Windows).
    explicit AppSettings(QObject *parent = nullptr);
    // Uses an INI file at iniPath (unit tests, portable mode).
    explicit AppSettings(const QString &iniPath, QObject *parent = nullptr);
    ~AppSettings() override;

    void beginBatch();
    void endBatch();
    void sync();

    // ---- AI -------------------------------------------------------------
    QString aiProvider() const;  // "claude" (default) | "openai"
    void setAiProvider(const QString &id);

    QString claudeApiKey() const;
    void setClaudeApiKey(const QString &key);
    QString claudeModel() const;  // default "claude-opus-5-5"
    void setClaudeModel(const QString &model);

    QString openAiApiKey() const;
    void setOpenAiApiKey(const QString &key);
    QString openAiModel() const;  // default chosen by OpenAIProvider::defaultModel()
    void setOpenAiModel(const QString &model);

    QString quality() const;  // "fast" | "balanced" (default) | "best"
    void setQuality(const QString &quality);

    // ---- Translation preferences ------------------------------------------
    Direction direction() const;  // last used direction
    void setDirection(Direction d);
    Tone tone() const;  // default Neutral
    void setTone(Tone t);
    ChineseScript script() const;  // default Traditional
    void setScript(ChineseScript s);
    bool showJyutping() const;  // default true
    void setShowJyutping(bool on);
    bool showAlternatives() const;  // default true
    void setShowAlternatives(bool on);
    bool showNotes() const;  // default true
    void setShowNotes(bool on);

    // ---- Speech -----------------------------------------------------------
    QString speechEngine() const;  // "system" (default) | "azure"
    void setSpeechEngine(const QString &id);
    QString systemVoice(Language lang) const;  // voice name; empty = automatic
    void setSystemVoice(Language lang, const QString &voiceName);
    QString azureKey() const;
    void setAzureKey(const QString &key);
    QString azureRegion() const;  // e.g. "eastasia", "eastus"; default "eastasia"
    void setAzureRegion(const QString &region);
    QString azureVoice(Language lang) const;  // default zh-HK-HiuMaanNeural / en-US-AvaMultilingualNeural
    void setAzureVoice(Language lang, const QString &voiceName);
    double speechRate() const;  // -1.0 (slow) .. 1.0 (fast); default 0.0
    void setSpeechRate(double rate);
    bool autoSpeak() const;  // speak the result automatically after translating; default false
    void setAutoSpeak(bool on);

    // ---- Appearance / window ------------------------------------------------
    QString theme() const;  // "system" (default) | "light" | "dark"
    void setTheme(const QString &theme);
    int fontPointSize() const;  // base point size for text panes; default 13
    void setFontPointSize(int pt);
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &geometry);
    QByteArray windowState() const;
    void setWindowState(const QByteArray &state);
    bool historyVisible() const;  // default true
    void setHistoryVisible(bool on);
    bool firstRunCompleted() const;
    void setFirstRunCompleted(bool done);

    // Directory for history.json, audio cache, logs (created on demand).
    QString dataDirectory() const;

signals:
    void changed();

private:
    void setValue(const QString &key, const QVariant &value);
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    void setSecret(const QString &key, const QString &plain);
    QString secret(const QString &key) const;
    void notifyChanged();

    QSettings *m_settings = nullptr;
    QString m_dataDirOverride;  // set in INI/test mode: directory of the INI file
    int m_batchDepth = 0;
    bool m_pendingChange = false;
};

} // namespace sct
