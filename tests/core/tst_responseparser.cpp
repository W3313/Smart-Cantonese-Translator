#include "core/ResponseParser.h"

#include <QtTest>

using namespace sct;

namespace {

const char kClean[] = R"({"translation":"佢喺邊度呀？","jyutping":"keoi5 hai2 bin1 dou6 aa3?","literal":"he at where (soft question)?","alternatives":[{"text":"佢去咗邊呀？","jyutping":"keoi5 heoi3 zo2 bin1 aa3?","note":"Asks where he went."}],"notes":["呀 softens the question."]})";

TranslationRequest enToYue()
{
    TranslationRequest r;
    r.text = QStringLiteral("Where is he?");
    return r;
}

} // namespace

class TstResponseParser : public QObject
{
    Q_OBJECT

private slots:
    void cleanJson()
    {
        const ParsedTranslation p = ResponseParser::parse(QString::fromUtf8(kClean), enToYue());
        QVERIFY(p.ok);
        QCOMPARE(p.result.translation, QStringLiteral("佢喺邊度呀？"));
        QCOMPARE(p.result.jyutping, QStringLiteral("keoi5 hai2 bin1 dou6 aa3?"));
        QCOMPARE(p.result.literal, QStringLiteral("he at where (soft question)?"));
        QCOMPARE(p.result.alternatives.size(), 1);
        QCOMPARE(p.result.alternatives.first().text, QStringLiteral("佢去咗邊呀？"));
        QCOMPARE(p.result.alternatives.first().jyutping, QStringLiteral("keoi5 heoi3 zo2 bin1 aa3?"));
        QCOMPARE(p.result.alternatives.first().note, QStringLiteral("Asks where he went."));
        QCOMPARE(p.result.notes, QStringList{QStringLiteral("呀 softens the question.")});
        QCOMPARE(p.result.request.text, QStringLiteral("Where is he?"));
    }

    void wrapped_data()
    {
        QTest::addColumn<QString>("text");
        const QString json = QString::fromUtf8(kClean);
        QTest::newRow("json fence") << (QStringLiteral("```json\n") + json + QStringLiteral("\n```"));
        QTest::newRow("bare fence") << (QStringLiteral("```\n") + json + QStringLiteral("\n```\n"));
        QTest::newRow("prose around") << (QStringLiteral("Sure! Here is the translation:\n\n") + json
                                          + QStringLiteral("\n\nLet me know if you need {anything} else."));
        QTest::newRow("decoy braces before")
            << (QStringLiteral("Format {like this}: ") + json);
        QTest::newRow("whitespace") << (QStringLiteral("\n\n   ") + json + QStringLiteral("   \n"));
    }

    void wrapped()
    {
        QFETCH(QString, text);
        const ParsedTranslation p = ResponseParser::parse(text, enToYue());
        QVERIFY2(p.ok, qPrintable(p.error.detail));
        QCOMPARE(p.result.translation, QStringLiteral("佢喺邊度呀？"));
    }

    void bracesInsideStrings()
    {
        const QString text = QStringLiteral(
            R"(Answer: {"translation":"用 {} 括住 \"佢\"","jyutping":"","literal":"","alternatives":[],"notes":["Use } carefully"]} trailing)");
        const ParsedTranslation p = ResponseParser::parse(text, enToYue());
        QVERIFY2(p.ok, qPrintable(p.error.detail));
        QCOMPARE(p.result.translation, QStringLiteral("用 {} 括住 \"佢\""));
        QCOMPARE(p.result.notes, QStringList{QStringLiteral("Use } carefully")});
    }

    void failures_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("empty") << QString();
        QTest::newRow("prose only") << QStringLiteral("I can't help with that.");
        QTest::newRow("missing translation") << QStringLiteral(R"({"jyutping":"nei5 hou2","notes":[]})");
        QTest::newRow("empty translation") << QStringLiteral(R"({"translation":"   ","jyutping":""})");
        QTest::newRow("translation not string") << QStringLiteral(R"({"translation":42})");
        QTest::newRow("array not object") << QStringLiteral(R"(["translation"])");
        QTest::newRow("truncated") << QStringLiteral(R"({"translation":"佢喺邊度)");
    }

    void failures()
    {
        QFETCH(QString, text);
        const ParsedTranslation p = ResponseParser::parse(text, enToYue());
        QVERIFY(!p.ok);
        QCOMPARE(p.error.kind, ErrorKind::BadResponse);
        QVERIFY(!p.error.message.isEmpty());
        QVERIFY(!p.error.detail.isEmpty());
    }

    void capsAndCleanup()
    {
        const QString text = QStringLiteral(R"({
            "translation": "  你好  ",
            "jyutping": "  nei5   hou2 \n  second   line ",
            "literal": "  you good ",
            "alternatives": [
                {"text": "你好", "jyutping": "nei5 hou2", "note": "duplicate of the translation"},
                {"text": "", "jyutping": "", "note": "empty"},
                {"text": " 哈囉 ", "jyutping": " haa1 lo3 ", "note": " casual "},
                {"text": "早晨", "jyutping": "zou2 san4", "note": "morning"},
                {"text": "早晨", "jyutping": "zou2 san4", "note": "duplicate"},
                {"text": "你好嗎", "jyutping": "nei5 hou2 maa3", "note": "how are you"},
                {"text": "喂", "jyutping": "wai3", "note": "fourth - capped"}
            ],
            "notes": ["one", "", "  two  ", "one", "three", "four"]
        })");
        const ParsedTranslation p = ResponseParser::parse(text, enToYue());
        QVERIFY(p.ok);
        QCOMPARE(p.result.translation, QStringLiteral("你好"));
        QCOMPARE(p.result.jyutping, QStringLiteral("nei5 hou2\nsecond line"));
        QCOMPARE(p.result.literal, QStringLiteral("you good"));
        QCOMPARE(p.result.alternatives.size(), ResponseParser::kMaxAlternatives);
        QCOMPARE(p.result.alternatives.at(0).text, QStringLiteral("哈囉"));
        QCOMPARE(p.result.alternatives.at(0).jyutping, QStringLiteral("haa1 lo3"));
        QCOMPARE(p.result.alternatives.at(0).note, QStringLiteral("casual"));
        QCOMPARE(p.result.alternatives.at(1).text, QStringLiteral("早晨"));
        QCOMPARE(p.result.alternatives.at(2).text, QStringLiteral("你好嗎"));
        QCOMPARE(p.result.notes, (QStringList{QStringLiteral("one"), QStringLiteral("two"), QStringLiteral("three")}));
    }

    void multiLineTranslationKeepsLineBreaks()
    {
        const QString text = QStringLiteral(R"({"translation":"第一行\r\n第二行\n\n第三段","jyutping":"dai6 jat1 hong4\ndai6 ji6 hong4","literal":"","alternatives":[],"notes":[]})");
        const ParsedTranslation p = ResponseParser::parse(text, enToYue());
        QVERIFY(p.ok);
        QCOMPARE(p.result.translation, QStringLiteral("第一行\n第二行\n\n第三段"));
        QCOMPARE(p.result.jyutping, QStringLiteral("dai6 jat1 hong4\ndai6 ji6 hong4"));
    }

    void disabledExtrasAreCleared()
    {
        TranslationRequest r = enToYue();
        r.wantAlternatives = false;
        r.wantNotes = false;
        const ParsedTranslation p = ResponseParser::parse(QString::fromUtf8(kClean), r);
        QVERIFY(p.ok);
        QVERIFY(p.result.alternatives.isEmpty());
        QVERIFY(p.result.notes.isEmpty());
    }

    void englishAlternativesHaveNoJyutping()
    {
        TranslationRequest r;
        r.text = QStringLiteral("你食咗飯未呀？");
        r.direction = Direction::CantoneseToEnglish;
        const QString text = QStringLiteral(R"({"translation":"Have you eaten yet?","jyutping":"nei5 sik6 zo2 faan6 mei6 aa3?","literal":"you eat-(done) rice yet?","alternatives":[{"text":"Have you had a meal?","jyutping":"bogus","note":"literal-ish"}],"notes":[]})");
        const ParsedTranslation p = ResponseParser::parse(text, r);
        QVERIFY(p.ok);
        QCOMPARE(p.result.jyutping, QStringLiteral("nei5 sik6 zo2 faan6 mei6 aa3?"));
        QCOMPARE(p.result.alternatives.size(), 1);
        QVERIFY(p.result.alternatives.first().jyutping.isEmpty());
    }

    void lenientShapes()
    {
        // Missing optional fields, alternatives as plain strings, notes as a string.
        const QString text =
            QStringLiteral(R"({"translation":"唔該","alternatives":["多謝","唔該晒"],"notes":"Use 唔該 for services."})");
        const ParsedTranslation p = ResponseParser::parse(text, enToYue());
        QVERIFY(p.ok);
        QVERIFY(p.result.jyutping.isEmpty());
        QVERIFY(p.result.literal.isEmpty());
        QCOMPARE(p.result.alternatives.size(), 2);
        QCOMPARE(p.result.alternatives.at(1).text, QStringLiteral("唔該晒"));
        QCOMPARE(p.result.notes, QStringList{QStringLiteral("Use 唔該 for services.")});
    }

    void extractJsonObject()
    {
        QCOMPARE(ResponseParser::extractJsonObject(QStringLiteral("no json here")), QString());
        QCOMPARE(ResponseParser::extractJsonObject(QStringLiteral("x {\"a\":{\"b\":1}} y")),
                 QStringLiteral("{\"a\":{\"b\":1}}"));
        QCOMPARE(ResponseParser::extractJsonObject(QStringLiteral("{broken {\"ok\":true}")),
                 QStringLiteral("{\"ok\":true}"));
    }
};

QTEST_GUILESS_MAIN(TstResponseParser)
#include "tst_responseparser.moc"
