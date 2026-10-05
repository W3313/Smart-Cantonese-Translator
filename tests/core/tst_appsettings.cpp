#include "core/AppSettings.h"
#include "core/OpenAIProvider.h"

#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

using namespace sct;

// QCOMPARE's ADL lookup finds sct::toString(Direction/Tone/ChineseScript),
// which returns QString, so compare those enums as ints.
#define QCOMPARE_ENUM(actual, expected) QCOMPARE(int(actual), int(expected))

class TstAppSettings : public QObject
{
    Q_OBJECT

private:
    QString iniPath() const { return m_dir->filePath(QStringLiteral("settings.ini")); }
    QByteArray iniBytes() const
    {
        QFile f(iniPath());
        if (!f.open(QIODevice::ReadOnly))
            return {};
        return f.readAll();
    }

    std::unique_ptr<QTemporaryDir> m_dir;

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void defaults()
    {
        AppSettings s(iniPath());
        QCOMPARE(s.aiProvider(), QStringLiteral("claude"));
        QVERIFY(s.claudeApiKey().isEmpty());
        QCOMPARE(s.claudeModel(), QStringLiteral("claude-opus-5-5"));
        QVERIFY(s.openAiApiKey().isEmpty());
        OpenAIProvider openAi(nullptr);
        QCOMPARE(s.openAiModel(), openAi.defaultModel());
        QCOMPARE(s.quality(), QStringLiteral("balanced"));

        QCOMPARE_ENUM(s.direction(), Direction::EnglishToCantonese);
        QCOMPARE_ENUM(s.tone(), Tone::Neutral);
        QCOMPARE_ENUM(s.script(), ChineseScript::Traditional);
        QVERIFY(s.showJyutping());
        QVERIFY(s.showAlternatives());
        QVERIFY(s.showNotes());

        QCOMPARE(s.speechEngine(), QStringLiteral("system"));
        QVERIFY(s.systemVoice(Language::Cantonese).isEmpty());
        QVERIFY(s.systemVoice(Language::English).isEmpty());
        QVERIFY(s.azureKey().isEmpty());
        QCOMPARE(s.azureRegion(), QStringLiteral("eastasia"));
        QCOMPARE(s.azureVoice(Language::Cantonese), QStringLiteral("zh-HK-HiuMaanNeural"));
        QCOMPARE(s.azureVoice(Language::English), QStringLiteral("en-US-AvaMultilingualNeural"));
        QCOMPARE(s.speechRate(), 0.0);
        QVERIFY(!s.autoSpeak());

        QCOMPARE(s.theme(), QStringLiteral("system"));
        QCOMPARE(s.fontPointSize(), 13);
        QVERIFY(s.windowGeometry().isEmpty());
        QVERIFY(s.windowState().isEmpty());
        QVERIFY(s.historyVisible());
        QVERIFY(!s.firstRunCompleted());
    }

    void persistence()
    {
        const QByteArray geometry("\x01\x02\x00\xff geometry", 13);
        {
            AppSettings s(iniPath());
            s.setAiProvider(QStringLiteral("openai"));
            s.setClaudeApiKey(QStringLiteral("sk-ant-test"));
            s.setClaudeModel(QStringLiteral("claude-sonnet-5-5"));
            s.setOpenAiApiKey(QStringLiteral("sk-openai-test"));
            s.setOpenAiModel(QStringLiteral("gpt-6-luna"));
            s.setQuality(QStringLiteral("best"));
            s.setDirection(Direction::CantoneseToEnglish);
            s.setTone(Tone::Casual);
            s.setScript(ChineseScript::Simplified);
            s.setShowJyutping(false);
            s.setShowAlternatives(false);
            s.setShowNotes(false);
            s.setSpeechEngine(QStringLiteral("azure"));
            s.setSystemVoice(Language::Cantonese, QStringLiteral("Microsoft Tracy"));
            s.setAzureKey(QStringLiteral("azure-secret"));
            s.setAzureRegion(QStringLiteral("EastUS"));
            s.setAzureVoice(Language::Cantonese, QStringLiteral("zh-HK-WanLungNeural"));
            s.setSpeechRate(0.25);
            s.setAutoSpeak(true);
            s.setTheme(QStringLiteral("dark"));
            s.setFontPointSize(16);
            s.setWindowGeometry(geometry);
            s.setWindowState(QByteArray("state"));
            s.setHistoryVisible(false);
            s.setFirstRunCompleted(true);
            s.sync();
        }
        AppSettings s(iniPath());
        QCOMPARE(s.aiProvider(), QStringLiteral("openai"));
        QCOMPARE(s.claudeApiKey(), QStringLiteral("sk-ant-test"));
        QCOMPARE(s.claudeModel(), QStringLiteral("claude-sonnet-5-5"));
        QCOMPARE(s.openAiApiKey(), QStringLiteral("sk-openai-test"));
        QCOMPARE(s.openAiModel(), QStringLiteral("gpt-6-luna"));
        QCOMPARE(s.quality(), QStringLiteral("best"));
        QCOMPARE_ENUM(s.direction(), Direction::CantoneseToEnglish);
        QCOMPARE_ENUM(s.tone(), Tone::Casual);
        QCOMPARE_ENUM(s.script(), ChineseScript::Simplified);
        QVERIFY(!s.showJyutping());
        QVERIFY(!s.showAlternatives());
        QVERIFY(!s.showNotes());
        QCOMPARE(s.speechEngine(), QStringLiteral("azure"));
        QCOMPARE(s.systemVoice(Language::Cantonese), QStringLiteral("Microsoft Tracy"));
        QVERIFY(s.systemVoice(Language::English).isEmpty());
        QCOMPARE(s.azureKey(), QStringLiteral("azure-secret"));
        QCOMPARE(s.azureRegion(), QStringLiteral("eastus"));
        QCOMPARE(s.azureVoice(Language::Cantonese), QStringLiteral("zh-HK-WanLungNeural"));
        QCOMPARE(s.speechRate(), 0.25);
        QVERIFY(s.autoSpeak());
        QCOMPARE(s.theme(), QStringLiteral("dark"));
        QCOMPARE(s.fontPointSize(), 16);
        QCOMPARE(s.windowGeometry(), geometry);
        QCOMPARE(s.windowState(), QByteArray("state"));
        QVERIFY(!s.historyVisible());
        QVERIFY(s.firstRunCompleted());
    }

    void secretsAreNotStoredInPlaintext()
    {
        const QString claudeKey = QStringLiteral("sk-ant-api03-SUPERSECRETVALUE");
        const QString openAiKey = QStringLiteral("sk-proj-ANOTHERSECRETVALUE");
        {
            AppSettings s(iniPath());
            s.setClaudeApiKey(claudeKey);
            s.setOpenAiApiKey(QStringLiteral("  ") + openAiKey + QStringLiteral("\n"));  // trimmed
            s.setAzureKey(QStringLiteral("AZURESECRETVALUE"));
            s.sync();
            QCOMPARE(s.openAiApiKey(), openAiKey);
        }
        const QByteArray ini = iniBytes();
        QVERIFY(!ini.isEmpty());
        QVERIFY(!ini.contains("SUPERSECRETVALUE"));
        QVERIFY(!ini.contains("ANOTHERSECRETVALUE"));
        QVERIFY(!ini.contains("AZURESECRETVALUE"));
        QVERIFY(ini.contains("claudeApiKey"));
        QVERIFY(ini.contains("obf:") || ini.contains("dpapi:"));

        AppSettings s(iniPath());
        QCOMPARE(s.claudeApiKey(), claudeKey);
        QCOMPARE(s.openAiApiKey(), openAiKey);

        // Clearing a key removes it from storage.
        s.setClaudeApiKey(QString());
        s.sync();
        QVERIFY(s.claudeApiKey().isEmpty());
        QSettings raw(iniPath(), QSettings::IniFormat);
        QVERIFY(!raw.contains(QStringLiteral("secrets/claudeApiKey")));
        QVERIFY(raw.contains(QStringLiteral("secrets/openAiApiKey")));
    }

    void corruptSecretReadsAsEmpty()
    {
        {
            QSettings raw(iniPath(), QSettings::IniFormat);
            raw.setValue(QStringLiteral("secrets/claudeApiKey"), QStringLiteral("plaintext-should-not-work"));
        }
        AppSettings s(iniPath());
        QVERIFY(s.claudeApiKey().isEmpty());
    }

    void changedSignal()
    {
        AppSettings s(iniPath());
        QSignalSpy spy(&s, &AppSettings::changed);

        s.setTone(Tone::Casual);
        QCOMPARE(spy.count(), 1);
        s.setTone(Tone::Casual);  // unchanged
        QCOMPARE(spy.count(), 1);
        s.setTone(Tone::Neutral);
        QCOMPARE(spy.count(), 2);
        s.setScript(ChineseScript::Traditional);  // equals default: no change
        QCOMPARE(spy.count(), 2);
        s.setClaudeApiKey(QStringLiteral("k1"));
        QCOMPARE(spy.count(), 3);
        s.setClaudeApiKey(QStringLiteral("k1"));  // same secret, even though DPAPI/obf output differs
        QCOMPARE(spy.count(), 3);
        s.setClaudeModel(QString());  // empty means default = current value
        QCOMPARE(spy.count(), 3);
    }

    void batching()
    {
        AppSettings s(iniPath());
        QSignalSpy spy(&s, &AppSettings::changed);

        s.beginBatch();
        s.setTone(Tone::Polite);
        s.setScript(ChineseScript::Simplified);
        s.setQuality(QStringLiteral("fast"));
        s.beginBatch();  // nested
        s.setTheme(QStringLiteral("light"));
        s.endBatch();
        QCOMPARE(spy.count(), 0);
        s.endBatch();
        QCOMPARE(spy.count(), 1);

        // A batch without real changes emits nothing.
        s.beginBatch();
        s.setTone(Tone::Polite);
        s.setTheme(QStringLiteral("light"));
        s.endBatch();
        QCOMPARE(spy.count(), 1);

        // Unbalanced endBatch() is harmless.
        s.endBatch();
        s.setTone(Tone::Casual);
        QCOMPARE(spy.count(), 2);
    }

    void clampingAndValidation()
    {
        AppSettings s(iniPath());
        s.setSpeechRate(5.0);
        QCOMPARE(s.speechRate(), 1.0);
        s.setSpeechRate(-3.0);
        QCOMPARE(s.speechRate(), -1.0);
        s.setSpeechRate(std::numeric_limits<double>::quiet_NaN());
        QCOMPARE(s.speechRate(), 0.0);
        s.setFontPointSize(100);
        QCOMPARE(s.fontPointSize(), 32);
        s.setFontPointSize(2);
        QCOMPARE(s.fontPointSize(), 8);

        s.setAiProvider(QStringLiteral("bogus"));
        QCOMPARE(s.aiProvider(), QStringLiteral("claude"));
        s.setAiProvider(QStringLiteral(" OpenAI "));
        QCOMPARE(s.aiProvider(), QStringLiteral("openai"));
        s.setQuality(QStringLiteral("ultra"));
        QCOMPARE(s.quality(), QStringLiteral("balanced"));
        s.setTheme(QStringLiteral("neon"));
        QCOMPARE(s.theme(), QStringLiteral("system"));
        s.setSpeechEngine(QStringLiteral("robot"));
        QCOMPARE(s.speechEngine(), QStringLiteral("system"));

        // Out-of-range values written by hand are clamped on read.
        {
            QSettings raw(iniPath(), QSettings::IniFormat);
            raw.setValue(QStringLiteral("ui/fontPointSize"), 99);
            raw.setValue(QStringLiteral("speech/rate"), -7.5);
            raw.setValue(QStringLiteral("translation/tone"), QStringLiteral("shouty"));
        }
        AppSettings fresh(iniPath());
        QCOMPARE(fresh.fontPointSize(), 32);
        QCOMPARE(fresh.speechRate(), -1.0);
        QCOMPARE_ENUM(fresh.tone(), Tone::Neutral);
    }

    void modelsFallBackToDefault()
    {
        AppSettings s(iniPath());
        s.setClaudeModel(QStringLiteral("claude-haiku-4-5"));
        QCOMPARE(s.claudeModel(), QStringLiteral("claude-haiku-4-5"));
        s.setClaudeModel(QStringLiteral("   "));
        QCOMPARE(s.claudeModel(), QStringLiteral("claude-opus-5-5"));
        s.setOpenAiModel(QStringLiteral("some-future-model"));
        QCOMPARE(s.openAiModel(), QStringLiteral("some-future-model"));
    }

    void dataDirectory()
    {
        AppSettings s(iniPath());
        QCOMPARE(s.dataDirectory(), QDir::cleanPath(m_dir->path()));
        QVERIFY(QDir(s.dataDirectory()).exists());
    }
};

QTEST_GUILESS_MAIN(TstAppSettings)
#include "tst_appsettings.moc"
