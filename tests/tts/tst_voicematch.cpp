#include "tts/SystemSpeechEngine.h"
#include "tts/VoiceMatch.h"

#include <QLocale>
#include <QtTest>

using namespace sct;
using voicematch::Candidate;
using voicematch::Dialect;

Q_DECLARE_METATYPE(sct::voicematch::Dialect)

namespace {

Candidate v(const char *name, const char *locale, const char *gender = "")
{
    return Candidate{QString::fromUtf8(name), QString::fromLatin1(locale), QString::fromLatin1(gender)};
}

// Roughly what a Windows 11 PC with en-US, zh-CN, zh-TW and zh-HK voices reports.
QList<Candidate> windowsVoices()
{
    return {
        v("Microsoft David", "en-US", "Male"),
        v("Microsoft Huihui", "zh-CN", "Female"),
        v("Microsoft Hanhan", "zh-TW", "Female"),
        v("Microsoft Tracy", "zh-HK", "Female"),
        v("Microsoft Danny", "zh-HK", "Male"),
        v("Microsoft Zira", "en-US", "Female"),
        v("Microsoft Hazel", "en-GB", "Female"),
    };
}

QString pickedName(const QList<Candidate> &voices, Language lang, const QString &preferred = QString())
{
    const int i = voicematch::pick(voices, lang, preferred);
    return i < 0 ? QString() : voices.at(i).name;
}

} // namespace

class TestVoiceMatch : public QObject
{
    Q_OBJECT

private slots:
    void normalizeTag_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");
        QTest::newRow("underscore") << "zh_HK" << "zh-HK";
        QTest::newRow("case") << "ZH-hant-hk" << "zh-Hant-HK";
        QTest::newRow("posix") << "en_US.UTF-8" << "en-US";
        QTest::newRow("lang only") << "yue" << "yue";
        QTest::newRow("numeric region") << "es-419" << "es-419";
        QTest::newRow("empty") << "" << "";
    }

    void normalizeTag()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(voicematch::normalizeTag(input), expected);
    }

    void classifyLocale_data()
    {
        QTest::addColumn<QString>("tag");
        QTest::addColumn<Dialect>("dialect");
        QTest::newRow("zh-HK") << "zh-HK" << Dialect::Cantonese;
        QTest::newRow("zh_HK") << "zh_HK" << Dialect::Cantonese;
        QTest::newRow("zh-Hant-HK") << "zh-Hant-HK" << Dialect::Cantonese;
        QTest::newRow("yue-HK") << "yue-HK" << Dialect::Cantonese;
        QTest::newRow("yue") << "yue" << Dialect::Cantonese;
        QTest::newRow("zh-MO") << "zh-MO" << Dialect::Cantonese;
        QTest::newRow("zh-yue") << "zh-yue" << Dialect::Cantonese;
        QTest::newRow("zh-CN") << "zh-CN" << Dialect::Mandarin;
        QTest::newRow("zh-TW") << "zh-TW" << Dialect::Mandarin;
        QTest::newRow("zh-Hant-TW") << "zh-Hant-TW" << Dialect::Mandarin;
        QTest::newRow("zh") << "zh" << Dialect::Mandarin;
        QTest::newRow("zh-SG") << "zh-SG" << Dialect::Mandarin;
        QTest::newRow("cmn-CN") << "cmn-CN" << Dialect::Mandarin;
        QTest::newRow("en-US") << "en-US" << Dialect::English;
        QTest::newRow("en") << "en" << Dialect::English;
        QTest::newRow("en-HK") << "en-HK" << Dialect::English;
        QTest::newRow("fr-FR") << "fr-FR" << Dialect::Other;
        QTest::newRow("empty") << "" << Dialect::Unknown;
        QTest::newRow("C") << "C" << Dialect::Unknown;
    }

    void classifyLocale()
    {
        QFETCH(QString, tag);
        QFETCH(Dialect, dialect);
        QCOMPARE(voicematch::classifyLocale(tag), dialect);
    }

    void tagFromLocaleKeepsRegion()
    {
        // QLocale::bcp47Name() would give "en" and "zh" here.
        QCOMPARE(voicematch::tagFromLocale(QLocale(QStringLiteral("en-US"))), QStringLiteral("en-US"));
        QCOMPARE(voicematch::tagFromLocale(QLocale(QStringLiteral("zh-CN"))), QStringLiteral("zh-CN"));
        QCOMPARE(voicematch::tagFromLocale(QLocale(QStringLiteral("zh-HK"))), QStringLiteral("zh-HK"));
        QCOMPARE(voicematch::tagFromLocale(QLocale(QLocale::Cantonese, QLocale::HongKong)),
                 QStringLiteral("yue-HK"));
        QCOMPARE(voicematch::tagFromLocale(QLocale::c()), QString());
        QCOMPARE(voicematch::classifyLocale(voicematch::tagFromLocale(QLocale(QLocale::Cantonese))),
                 Dialect::Cantonese);
    }

    void picksTracyForCantonese()
    {
        QCOMPARE(pickedName(windowsVoices(), Language::Cantonese), QStringLiteral("Microsoft Tracy"));
        QCOMPARE(pickedName(windowsVoices(), Language::English), QStringLiteral("Microsoft David"));
    }

    void neverUsesMandarinForCantonese()
    {
        const QList<Candidate> mandarinOnly = {
            v("Microsoft Huihui", "zh-CN"),
            v("Microsoft Hanhan", "zh-TW"),
            v("Microsoft Yaoyao", "zh-CN"),
            v("Microsoft Zira", "en-US"),
        };
        QCOMPARE(voicematch::pick(mandarinOnly, Language::Cantonese), -1);
        QVERIFY(voicematch::rank(mandarinOnly, Language::Cantonese).isEmpty());
        // Even if the user picked it explicitly.
        QCOMPARE(voicematch::pick(mandarinOnly, Language::Cantonese, QStringLiteral("Microsoft Huihui")), -1);
        // A Cantonese persona name does not rescue a zh-CN locale.
        QCOMPARE(voicematch::score(v("Microsoft Tracy", "zh-CN"), Language::Cantonese), 0);
        // English "Hong Kong" voices are not Cantonese.
        QCOMPARE(voicematch::score(v("Microsoft Sam - English (Hong Kong)", "en-HK"), Language::Cantonese), 0);
    }

    void userChoiceWins()
    {
        QCOMPARE(pickedName(windowsVoices(), Language::Cantonese, QStringLiteral("Microsoft Danny")),
                 QStringLiteral("Microsoft Danny"));
        QCOMPARE(pickedName(windowsVoices(), Language::Cantonese, QStringLiteral("microsoft danny")),
                 QStringLiteral("Microsoft Danny"));
        QCOMPARE(pickedName(windowsVoices(), Language::English, QStringLiteral("Microsoft Hazel")),
                 QStringLiteral("Microsoft Hazel"));
        // Not installed -> best match.
        QCOMPARE(pickedName(windowsVoices(), Language::Cantonese, QStringLiteral("Microsoft Nobody")),
                 QStringLiteral("Microsoft Tracy"));
        // Chosen voice is for the other language -> best match.
        QCOMPARE(pickedName(windowsVoices(), Language::Cantonese, QStringLiteral("Microsoft Zira")),
                 QStringLiteral("Microsoft Tracy"));
    }

    void rankingOrder()
    {
        const QList<Candidate> voices = {
            v("Macau voice", "zh-MO"),
            v("eSpeak Cantonese", "yue"),
            v("Mandarin", "zh-CN"),
            v("Some HK voice", "zh_HK"),
            v("Unknown-locale Cantonese", ""),
        };
        const QList<Candidate> ranked = voicematch::rank(voices, Language::Cantonese);
        QCOMPARE(ranked.size(), 4);
        QCOMPARE(ranked.at(0).name, QStringLiteral("Some HK voice"));
        QCOMPARE(ranked.at(1).name, QStringLiteral("eSpeak Cantonese"));
        QCOMPARE(ranked.at(2).name, QStringLiteral("Macau voice"));
        QCOMPARE(ranked.at(3).name, QStringLiteral("Unknown-locale Cantonese"));
    }

    void nameHeuristics()
    {
        QVERIFY(voicematch::hasCantoneseNameHint(QStringLiteral("Microsoft Tracy Desktop")));
        QVERIFY(voicematch::hasCantoneseNameHint(QStringLiteral("zh-HK-HiuGaaiNeural")));
        QVERIFY(voicematch::hasCantoneseNameHint(QStringLiteral("Chinese (Traditional, Hong Kong S.A.R.)")));
        QVERIFY(voicematch::hasCantoneseNameHint(QStringLiteral("廣東話")));
        QVERIFY(!voicematch::hasCantoneseNameHint(QStringLiteral("Microsoft Huihui")));
        QVERIFY(!voicematch::hasCantoneseNameHint(QStringLiteral("Yuelin")));

        // Locale unknown: trust the name.
        QVERIFY(voicematch::score(v("Microsoft Tracy", ""), Language::Cantonese) > 0);
        QVERIFY(voicematch::score(v("Cantonese", "C"), Language::Cantonese) > 0);
        QCOMPARE(voicematch::score(v("Microsoft Huihui", ""), Language::Cantonese), 0);
        // Locale says Mandarin but the name explicitly says Cantonese: usable,
        // but below any real zh-HK voice.
        const Candidate mislabelled = v("Cantonese (Hong Kong)", "zh-CN");
        QVERIFY(voicematch::score(mislabelled, Language::Cantonese) > 0);
        QVERIFY(voicematch::score(mislabelled, Language::Cantonese)
                < voicematch::score(v("Other", "zh-HK"), Language::Cantonese));
        // Known personas win ties among zh-HK voices.
        QCOMPARE(pickedName({v("Some zh-HK voice", "zh-HK"), v("Microsoft Tracy", "zh-HK")},
                            Language::Cantonese),
                 QStringLiteral("Microsoft Tracy"));
    }

    void englishPreferences()
    {
        QCOMPARE(pickedName({v("Hazel", "en-GB"), v("Zira", "en-US")}, Language::English), QStringLiteral("Zira"));
        QCOMPARE(pickedName({v("Hazel", "en-GB"), v("Heera", "en-IN")}, Language::English), QStringLiteral("Hazel"));
        QCOMPARE(pickedName({v("Heera", "en-IN")}, Language::English), QStringLiteral("Heera"));
        QCOMPARE(pickedName({v("Tracy", "zh-HK")}, Language::English), QString());
        QCOMPARE(voicematch::pick({}, Language::English), -1);
    }

    void backendOrder()
    {
        QCOMPARE(SystemSpeechEngine::orderBackends({QStringLiteral("sapi"), QStringLiteral("mock"),
                                                    QStringLiteral("winrt")}),
                 QStringList({QStringLiteral("winrt"), QStringLiteral("sapi")}));
        QCOMPARE(SystemSpeechEngine::orderBackends({QStringLiteral("flite"), QStringLiteral("other"),
                                                    QStringLiteral("speechd")}),
                 QStringList({QStringLiteral("speechd"), QStringLiteral("flite"), QStringLiteral("other")}));
        QVERIFY(SystemSpeechEngine::orderBackends({}).isEmpty());
    }

    void helpTextMentionsBothOptions()
    {
        const QString help = voicematch::cantoneseVoiceHelpText();
        QVERIFY(help.contains(QStringLiteral("Chinese (Traditional, Hong Kong SAR)")));
        QVERIFY(help.contains(QStringLiteral("Manage voices")));
        QVERIFY(help.contains(QStringLiteral("Azure")));
        QVERIFY(help.contains(QStringLiteral("Settings → Speech")));
    }
};

QTEST_GUILESS_MAIN(TestVoiceMatch)
#include "tst_voicematch.moc"
