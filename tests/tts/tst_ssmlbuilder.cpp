#include "tts/SsmlBuilder.h"

#include <QXmlStreamReader>
#include <QtTest>

#include <limits>

using sct::SsmlBuilder;
using sct::SsmlRequest;

namespace {

// Text content of the <prosody> element, with <break/> elements as '|'.
QString spokenContent(const QString &ssml, QString *error = nullptr)
{
    QXmlStreamReader xml(ssml);
    QString text;
    bool inProsody = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("prosody"))
            inProsody = true;
        else if (xml.isEndElement() && xml.name() == QLatin1String("prosody"))
            inProsody = false;
        else if (inProsody && xml.isStartElement() && xml.name() == QLatin1String("break"))
            text += QLatin1Char('|');
        else if (inProsody && xml.isCharacters())
            text += xml.text();
    }
    if (error)
        *error = xml.hasError() ? xml.errorString() : QString();
    return text;
}

} // namespace

class TestSsmlBuilder : public QObject
{
    Q_OBJECT

private slots:
    void escapesXmlSpecialCharacters()
    {
        QCOMPARE(SsmlBuilder::escapeXml(QStringLiteral("a & b < c > d \" e ' f")),
                 QStringLiteral("a &amp; b &lt; c &gt; d &quot; e &apos; f"));
        QCOMPARE(SsmlBuilder::escapeXml(QStringLiteral("你好")), QStringLiteral("你好"));
    }

    void buildsExpectedDocument()
    {
        SsmlRequest r;
        r.text = QStringLiteral("你好");
        r.voice = QStringLiteral("zh-HK-HiuMaanNeural");
        const auto result = SsmlBuilder::build(r);
        QCOMPARE(result.ssml,
                 QStringLiteral("<speak version=\"1.0\" xmlns=\"http://www.w3.org/2001/10/synthesis\" "
                                "xml:lang=\"zh-HK\"><voice name=\"zh-HK-HiuMaanNeural\">"
                                "<prosody rate=\"+0%\">你好</prosody></voice></speak>"));
        QVERIFY(!result.truncated);
        QCOMPARE(result.spokenText, QStringLiteral("你好"));
    }

    void escapesUserTextAndStaysWellFormed()
    {
        SsmlRequest r;
        r.text = QStringLiteral("Tom & Jerry <3 \"quotes\" 'apos' </speak><voice name=\"x\">%1");
        r.voice = QStringLiteral("en-US-AvaMultilingualNeural");
        const auto result = SsmlBuilder::build(r);
        QVERIFY(result.ssml.contains(QStringLiteral("Tom &amp; Jerry &lt;3")));
        QVERIFY(!result.ssml.contains(QStringLiteral("<3")));
        QString error;
        QCOMPARE(spokenContent(result.ssml, &error), r.text);
        QVERIFY2(error.isEmpty(), qPrintable(error));
    }

    void escapesVoiceAttribute()
    {
        SsmlRequest r;
        r.text = QStringLiteral("hi");
        r.voice = QStringLiteral("en-US-A\"B");
        r.lang = QStringLiteral("en-US");
        const auto result = SsmlBuilder::build(r);
        QVERIFY(result.ssml.contains(QStringLiteral("name=\"en-US-A&quot;B\"")));
    }

    void langFromVoiceOrExplicit()
    {
        SsmlRequest r;
        r.text = QStringLiteral("Hello");
        r.voice = QStringLiteral("en-GB-SoniaNeural");
        QVERIFY(SsmlBuilder::build(r).ssml.contains(QStringLiteral("xml:lang=\"en-GB\"")));
        r.lang = QStringLiteral("en-US");
        QVERIFY(SsmlBuilder::build(r).ssml.contains(QStringLiteral("xml:lang=\"en-US\"")));
        r.lang.clear();
        r.voice = QStringLiteral("weird");
        QVERIFY(SsmlBuilder::build(r).ssml.contains(QStringLiteral("xml:lang=\"en-US\"")));

        QCOMPARE(SsmlBuilder::localeOfVoice(QStringLiteral("zh-HK-WanLungNeural")), QStringLiteral("zh-HK"));
        QCOMPARE(SsmlBuilder::localeOfVoice(QStringLiteral("en-US-AvaMultilingualNeural")),
                 QStringLiteral("en-US"));
        QCOMPARE(SsmlBuilder::localeOfVoice(QStringLiteral("Microsoft Tracy")), QString());
    }

    void rateMapping_data()
    {
        QTest::addColumn<double>("rate");
        QTest::addColumn<QString>("expected");
        QTest::newRow("slowest") << -1.0 << QStringLiteral("-50%");
        QTest::newRow("slow") << -0.5 << QStringLiteral("-25%");
        QTest::newRow("normal") << 0.0 << QStringLiteral("+0%");
        QTest::newRow("tenth") << 0.1 << QStringLiteral("+5%");
        QTest::newRow("fast") << 0.5 << QStringLiteral("+25%");
        QTest::newRow("fastest") << 1.0 << QStringLiteral("+50%");
        QTest::newRow("clamp high") << 2.0 << QStringLiteral("+50%");
        QTest::newRow("clamp low") << -3.0 << QStringLiteral("-50%");
        QTest::newRow("nan") << std::numeric_limits<double>::quiet_NaN() << QStringLiteral("+0%");
    }

    void rateMapping()
    {
        QFETCH(double, rate);
        QFETCH(QString, expected);
        QCOMPARE(SsmlBuilder::ratePercent(rate), expected);
        SsmlRequest r;
        r.text = QStringLiteral("x");
        r.voice = QStringLiteral("en-US-GuyNeural");
        r.rate = rate;
        QVERIFY(SsmlBuilder::build(r).ssml.contains(QStringLiteral("rate=\"%1\"").arg(expected)));
    }

    void lineBreaksBecomePauses()
    {
        SsmlRequest r;
        r.text = QStringLiteral("Line one\nLine two\r\n\r\nPara  \n");
        r.voice = QStringLiteral("en-US-GuyNeural");
        const auto result = SsmlBuilder::build(r);
        QVERIFY2(result.ssml.contains(QStringLiteral(
                     "Line one<break time=\"300ms\"/>Line two<break time=\"600ms\"/>Para</prosody>")),
                 qPrintable(result.ssml));
        QCOMPARE(spokenContent(result.ssml), QStringLiteral("Line one|Line two|Para"));
    }

    void stripsControlCharacters()
    {
        QString input = QStringLiteral("a");
        input += QChar(0x01);
        input += QStringLiteral("b");
        input += QChar(0x7F);
        input += QStringLiteral("c");
        input += QChar(0x85);  // NEL -> line break
        input += QStringLiteral("d\tz");
        input += QChar(0xFEFF);
        input += QChar(0x1B);
        QCOMPARE(SsmlBuilder::cleanText(input), QStringLiteral("abc\nd z"));
        QCOMPARE(SsmlBuilder::cleanText(QStringLiteral("a\r\nb\rc")), QStringLiteral("a\nb\nc"));
        QCOMPARE(SsmlBuilder::cleanText(QStringLiteral("  \n  ")), QString());

        // Unpaired surrogates are invalid XML; valid pairs (emoji) are kept.
        QString surrogates = QStringLiteral("x");
        surrogates += QChar(0xD800);
        surrogates += QStringLiteral("y");
        surrogates += QChar(0xDC00);
        surrogates += QStringLiteral("😀");
        QCOMPARE(SsmlBuilder::cleanText(surrogates), QStringLiteral("xy😀"));

        SsmlRequest r;
        r.text = input;
        r.voice = QStringLiteral("en-US-GuyNeural");
        QString error;
        spokenContent(SsmlBuilder::build(r).ssml, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
    }

    void shortTextIsNotTruncated()
    {
        SsmlRequest r;
        r.text = QString(SsmlBuilder::DefaultMaxChars, QLatin1Char('a'));
        r.voice = QStringLiteral("en-US-GuyNeural");
        const auto result = SsmlBuilder::build(r);
        QVERIFY(!result.truncated);
        QCOMPARE(result.spokenText.size(), qsizetype(SsmlBuilder::DefaultMaxChars));
        QCOMPARE(result.originalChars, SsmlBuilder::DefaultMaxChars);
    }

    void longTextIsTruncatedAtSentence()
    {
        SsmlRequest r;
        r.text = QStringLiteral("Hello world, this is a sentence. ").repeated(200);  // 6600 chars
        r.voice = QStringLiteral("en-US-GuyNeural");
        const auto result = SsmlBuilder::build(r);
        QVERIFY(result.truncated);
        QVERIFY(result.spokenText.size() <= SsmlBuilder::DefaultMaxChars);
        QVERIFY(result.spokenText.size() > SsmlBuilder::DefaultMaxChars * 9 / 10);
        QVERIFY(result.spokenText.endsWith(QLatin1Char('.')));
        QCOMPARE(result.originalChars, int(SsmlBuilder::cleanText(r.text).size()));

        const QString cantonese = QStringLiteral("我哋今日去飲茶。").repeated(500);
        bool truncated = false;
        const QString cut = SsmlBuilder::truncate(cantonese, 100, &truncated);
        QVERIFY(truncated);
        QVERIFY(cut.size() <= 100);
        QVERIFY(cut.endsWith(QStringLiteral("。")));
    }

    void truncationWithoutBoundaries()
    {
        bool truncated = false;
        const QString hard = SsmlBuilder::truncate(QString(5000, QLatin1Char('a')), 3000, &truncated);
        QVERIFY(truncated);
        QCOMPARE(hard.size(), qsizetype(3000));

        // Never split a surrogate pair at the cut.
        const QString emoji = QString(2999, QLatin1Char('a')) + QStringLiteral("😀") + QString(10, QLatin1Char('b'));
        const QString cut = SsmlBuilder::truncate(emoji, 3000, &truncated);
        QVERIFY(truncated);
        QVERIFY(!cut.isEmpty());
        QVERIFY(!cut.back().isHighSurrogate());
        QVERIFY(cut.size() <= 3000);

        QCOMPARE(SsmlBuilder::truncate(QStringLiteral("short"), 3000, &truncated), QStringLiteral("short"));
        QVERIFY(!truncated);
    }

    void emptyText()
    {
        SsmlRequest r;
        r.text = QStringLiteral(" \n\t ");
        r.voice = QStringLiteral("en-US-GuyNeural");
        const auto result = SsmlBuilder::build(r);
        QVERIFY(result.spokenText.isEmpty());
        QVERIFY(!result.truncated);
    }
};

QTEST_GUILESS_MAIN(TestSsmlBuilder)
#include "tst_ssmlbuilder.moc"
