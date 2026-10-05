#include "core/AppSettings.h"

#include "core/ProviderDefaults.h"
#include "core/SecretStore.h"
#include "core/Version.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

#include <cmath>

namespace sct {

namespace {

// Setting keys (stable; changing them loses users' settings).
const QString kAiProvider = QStringLiteral("ai/provider");
const QString kClaudeModel = QStringLiteral("ai/claudeModel");
const QString kOpenAiModel = QStringLiteral("ai/openAiModel");
const QString kQuality = QStringLiteral("ai/quality");
const QString kClaudeKey = QStringLiteral("secrets/claudeApiKey");
const QString kOpenAiKey = QStringLiteral("secrets/openAiApiKey");
const QString kAzureKey = QStringLiteral("secrets/azureKey");

const QString kDirection = QStringLiteral("translation/direction");
const QString kTone = QStringLiteral("translation/tone");
const QString kScript = QStringLiteral("translation/script");
const QString kShowJyutping = QStringLiteral("translation/showJyutping");
const QString kShowAlternatives = QStringLiteral("translation/showAlternatives");
const QString kShowNotes = QStringLiteral("translation/showNotes");

const QString kSpeechEngine = QStringLiteral("speech/engine");
const QString kSystemVoicePrefix = QStringLiteral("speech/systemVoice/");
const QString kAzureRegion = QStringLiteral("speech/azureRegion");
const QString kAzureVoicePrefix = QStringLiteral("speech/azureVoice/");
const QString kSpeechRate = QStringLiteral("speech/rate");
const QString kAutoSpeak = QStringLiteral("speech/autoSpeak");

const QString kTheme = QStringLiteral("ui/theme");
const QString kFontPointSize = QStringLiteral("ui/fontPointSize");
const QString kWindowGeometry = QStringLiteral("ui/windowGeometry");
const QString kWindowState = QStringLiteral("ui/windowState");
const QString kHistoryVisible = QStringLiteral("ui/historyVisible");
const QString kFirstRunCompleted = QStringLiteral("app/firstRunCompleted");

constexpr int kMinFontPt = 8;
constexpr int kMaxFontPt = 32;
constexpr int kDefaultFontPt = 13;

QString oneOf(const QString &value, const QStringList &allowed, const QString &fallback)
{
    const QString v = value.trimmed().toLower();
    return allowed.contains(v) ? v : fallback;
}

double clampRate(double rate)
{
    if (!std::isfinite(rate))
        return 0.0;
    return qBound(-1.0, rate, 1.0);
}

// QSettings in INI/registry mode may hand back strings for typed values, and
// Qt 6 QVariant comparison does not convert between types. Compare after
// converting the stored value to the new value's type.
bool sameStoredValue(const QVariant &stored, const QVariant &value)
{
    if (!stored.isValid())
        return false;
    if (stored.metaType() == value.metaType())
        return stored == value;
    QVariant converted = stored;
    if (!converted.convert(value.metaType()))
        return false;
    return converted == value;
}

} // namespace

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
    , m_settings(new QSettings(QSettings::NativeFormat, QSettings::UserScope,
                               QStringLiteral(SCT_ORG_NAME), QStringLiteral(SCT_APP_ID), this))
{
}

AppSettings::AppSettings(const QString &iniPath, QObject *parent)
    : QObject(parent)
    , m_settings(new QSettings(iniPath, QSettings::IniFormat, this))
    , m_dataDirOverride(QFileInfo(iniPath).absolutePath())
{
}

AppSettings::~AppSettings()
{
    if (m_settings)
        m_settings->sync();
}

void AppSettings::beginBatch()
{
    ++m_batchDepth;
}

void AppSettings::endBatch()
{
    if (m_batchDepth == 0)
        return;
    if (--m_batchDepth == 0 && m_pendingChange) {
        m_pendingChange = false;
        emit changed();
    }
}

void AppSettings::sync()
{
    m_settings->sync();
}

// ---- private helpers ----------------------------------------------------------

void AppSettings::setValue(const QString &key, const QVariant &value)
{
    if (sameStoredValue(m_settings->value(key), value))
        return;
    m_settings->setValue(key, value);
    notifyChanged();
}

QVariant AppSettings::value(const QString &key, const QVariant &defaultValue) const
{
    return m_settings->value(key, defaultValue);
}

void AppSettings::setSecret(const QString &key, const QString &plain)
{
    const QString trimmed = plain.trimmed();
    if (trimmed == secret(key))
        return;
    if (trimmed.isEmpty()) {
        m_settings->remove(key);
    } else {
        const QByteArray stored = SecretStore::protect(trimmed);
        if (stored.isEmpty())
            return;  // encryption failed; never fall back to plaintext
        m_settings->setValue(key, QString::fromLatin1(stored));
    }
    notifyChanged();
}

QString AppSettings::secret(const QString &key) const
{
    const QString stored = m_settings->value(key).toString();
    if (stored.isEmpty())
        return {};
    return SecretStore::unprotect(stored.toLatin1());
}

void AppSettings::notifyChanged()
{
    if (m_batchDepth > 0) {
        m_pendingChange = true;
        return;
    }
    emit changed();
}

// ---- AI ---------------------------------------------------------------------

QString AppSettings::aiProvider() const
{
    return oneOf(value(kAiProvider).toString(),
                 {ProviderDefaults::claudeProviderId(), ProviderDefaults::openAiProviderId()},
                 ProviderDefaults::claudeProviderId());
}

void AppSettings::setAiProvider(const QString &id)
{
    const QString v = oneOf(id, {ProviderDefaults::claudeProviderId(), ProviderDefaults::openAiProviderId()},
                            ProviderDefaults::claudeProviderId());
    if (v == aiProvider())
        return;
    setValue(kAiProvider, v);
}

QString AppSettings::claudeApiKey() const
{
    return secret(kClaudeKey);
}

void AppSettings::setClaudeApiKey(const QString &key)
{
    setSecret(kClaudeKey, key);
}

QString AppSettings::claudeModel() const
{
    const QString m = value(kClaudeModel).toString().trimmed();
    return m.isEmpty() ? ProviderDefaults::claudeDefaultModel() : m;
}

void AppSettings::setClaudeModel(const QString &model)
{
    const QString m = model.trimmed();
    if ((m.isEmpty() ? ProviderDefaults::claudeDefaultModel() : m) == claudeModel())
        return;
    setValue(kClaudeModel, m);
}

QString AppSettings::openAiApiKey() const
{
    return secret(kOpenAiKey);
}

void AppSettings::setOpenAiApiKey(const QString &key)
{
    setSecret(kOpenAiKey, key);
}

QString AppSettings::openAiModel() const
{
    const QString m = value(kOpenAiModel).toString().trimmed();
    return m.isEmpty() ? ProviderDefaults::openAiDefaultModel() : m;
}

void AppSettings::setOpenAiModel(const QString &model)
{
    const QString m = model.trimmed();
    if ((m.isEmpty() ? ProviderDefaults::openAiDefaultModel() : m) == openAiModel())
        return;
    setValue(kOpenAiModel, m);
}

QString AppSettings::quality() const
{
    return oneOf(value(kQuality).toString(),
                 {QStringLiteral("fast"), QStringLiteral("balanced"), QStringLiteral("best")},
                 ProviderDefaults::defaultQuality());
}

void AppSettings::setQuality(const QString &quality)
{
    const QString v = oneOf(quality, {QStringLiteral("fast"), QStringLiteral("balanced"), QStringLiteral("best")},
                            ProviderDefaults::defaultQuality());
    if (v == this->quality())
        return;
    setValue(kQuality, v);
}

// ---- Translation preferences ------------------------------------------------

Direction AppSettings::direction() const
{
    return directionFromString(value(kDirection).toString(), Direction::EnglishToCantonese);
}

void AppSettings::setDirection(Direction d)
{
    if (d == direction())
        return;
    setValue(kDirection, toString(d));
}

Tone AppSettings::tone() const
{
    return toneFromString(value(kTone).toString(), Tone::Neutral);
}

void AppSettings::setTone(Tone t)
{
    if (t == tone())
        return;
    setValue(kTone, toString(t));
}

ChineseScript AppSettings::script() const
{
    return scriptFromString(value(kScript).toString(), ChineseScript::Traditional);
}

void AppSettings::setScript(ChineseScript s)
{
    if (s == script())
        return;
    setValue(kScript, toString(s));
}

bool AppSettings::showJyutping() const
{
    return value(kShowJyutping, true).toBool();
}

void AppSettings::setShowJyutping(bool on)
{
    if (on == showJyutping())
        return;
    setValue(kShowJyutping, on);
}

bool AppSettings::showAlternatives() const
{
    return value(kShowAlternatives, true).toBool();
}

void AppSettings::setShowAlternatives(bool on)
{
    if (on == showAlternatives())
        return;
    setValue(kShowAlternatives, on);
}

bool AppSettings::showNotes() const
{
    return value(kShowNotes, true).toBool();
}

void AppSettings::setShowNotes(bool on)
{
    if (on == showNotes())
        return;
    setValue(kShowNotes, on);
}

// ---- Speech -----------------------------------------------------------------

QString AppSettings::speechEngine() const
{
    return oneOf(value(kSpeechEngine).toString(), {QStringLiteral("system"), QStringLiteral("azure")},
                 QStringLiteral("system"));
}

void AppSettings::setSpeechEngine(const QString &id)
{
    const QString v = oneOf(id, {QStringLiteral("system"), QStringLiteral("azure")}, QStringLiteral("system"));
    if (v == speechEngine())
        return;
    setValue(kSpeechEngine, v);
}

QString AppSettings::systemVoice(Language lang) const
{
    return value(kSystemVoicePrefix + languageTag(lang)).toString();
}

void AppSettings::setSystemVoice(Language lang, const QString &voiceName)
{
    if (voiceName == systemVoice(lang))
        return;
    setValue(kSystemVoicePrefix + languageTag(lang), voiceName);
}

QString AppSettings::azureKey() const
{
    return secret(kAzureKey);
}

void AppSettings::setAzureKey(const QString &key)
{
    setSecret(kAzureKey, key);
}

QString AppSettings::azureRegion() const
{
    const QString r = value(kAzureRegion).toString().trimmed();
    return r.isEmpty() ? QStringLiteral("eastasia") : r;
}

void AppSettings::setAzureRegion(const QString &region)
{
    const QString r = region.trimmed().toLower();
    if ((r.isEmpty() ? QStringLiteral("eastasia") : r) == azureRegion())
        return;
    setValue(kAzureRegion, r);
}

QString AppSettings::azureVoice(Language lang) const
{
    const QString v = value(kAzureVoicePrefix + languageTag(lang)).toString().trimmed();
    if (!v.isEmpty())
        return v;
    return lang == Language::Cantonese ? QStringLiteral("zh-HK-HiuMaanNeural")
                                       : QStringLiteral("en-US-AvaMultilingualNeural");
}

void AppSettings::setAzureVoice(Language lang, const QString &voiceName)
{
    const QString v = voiceName.trimmed();
    if (!v.isEmpty() && v == azureVoice(lang))
        return;
    if (v.isEmpty() && value(kAzureVoicePrefix + languageTag(lang)).toString().isEmpty())
        return;
    setValue(kAzureVoicePrefix + languageTag(lang), v);
}

double AppSettings::speechRate() const
{
    return clampRate(value(kSpeechRate, 0.0).toDouble());
}

void AppSettings::setSpeechRate(double rate)
{
    const double r = clampRate(rate);
    if (qFuzzyCompare(1.0 + r, 1.0 + speechRate()))
        return;
    setValue(kSpeechRate, r);
}

bool AppSettings::autoSpeak() const
{
    return value(kAutoSpeak, false).toBool();
}

void AppSettings::setAutoSpeak(bool on)
{
    if (on == autoSpeak())
        return;
    setValue(kAutoSpeak, on);
}

// ---- Appearance / window ----------------------------------------------------

QString AppSettings::theme() const
{
    return oneOf(value(kTheme).toString(),
                 {QStringLiteral("system"), QStringLiteral("light"), QStringLiteral("dark")},
                 QStringLiteral("system"));
}

void AppSettings::setTheme(const QString &theme)
{
    const QString v = oneOf(theme, {QStringLiteral("system"), QStringLiteral("light"), QStringLiteral("dark")},
                            QStringLiteral("system"));
    if (v == this->theme())
        return;
    setValue(kTheme, v);
}

int AppSettings::fontPointSize() const
{
    bool ok = false;
    const int pt = value(kFontPointSize, kDefaultFontPt).toInt(&ok);
    return ok ? qBound(kMinFontPt, pt, kMaxFontPt) : kDefaultFontPt;
}

void AppSettings::setFontPointSize(int pt)
{
    const int v = qBound(kMinFontPt, pt, kMaxFontPt);
    if (v == fontPointSize())
        return;
    setValue(kFontPointSize, v);
}

QByteArray AppSettings::windowGeometry() const
{
    return value(kWindowGeometry).toByteArray();
}

void AppSettings::setWindowGeometry(const QByteArray &geometry)
{
    if (geometry == windowGeometry())
        return;
    setValue(kWindowGeometry, geometry);
}

QByteArray AppSettings::windowState() const
{
    return value(kWindowState).toByteArray();
}

void AppSettings::setWindowState(const QByteArray &state)
{
    if (state == windowState())
        return;
    setValue(kWindowState, state);
}

bool AppSettings::historyVisible() const
{
    return value(kHistoryVisible, true).toBool();
}

void AppSettings::setHistoryVisible(bool on)
{
    if (on == historyVisible())
        return;
    setValue(kHistoryVisible, on);
}

bool AppSettings::firstRunCompleted() const
{
    return value(kFirstRunCompleted, false).toBool();
}

void AppSettings::setFirstRunCompleted(bool done)
{
    if (done == firstRunCompleted())
        return;
    setValue(kFirstRunCompleted, done);
}

QString AppSettings::dataDirectory() const
{
    QString dir = m_dataDirOverride;
    if (dir.isEmpty()) {
        if (!QCoreApplication::organizationName().isEmpty() && !QCoreApplication::applicationName().isEmpty()) {
            dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        } else {
            // main() has not set the application identity; build the same path
            // explicitly so the location never depends on the executable name.
            dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                  + QLatin1Char('/') + QStringLiteral(SCT_ORG_NAME) + QLatin1Char('/')
                  + QStringLiteral(SCT_APP_ID);
        }
    }
    QDir().mkpath(dir);
    return QDir::cleanPath(dir);
}

} // namespace sct
