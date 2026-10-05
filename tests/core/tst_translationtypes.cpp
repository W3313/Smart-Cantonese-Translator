#include "core/TranslationTypes.h"

#include <QtTest>

using namespace sct;

// QCOMPARE's ADL lookup finds sct::toString(Direction/Tone/ChineseScript),
// which returns QString, so compare those enums as ints.
#define QCOMPARE_ENUM(actual, expected) QCOMPARE(int(actual), int(expected))

class TstTranslationTypes : public QObject
{
    Q_OBJECT

private slots:
    void languageInfo()
    {
        QCOMPARE(languageTag(Language::English), QStringLiteral("en-US"));
        QCOMPARE(languageTag(Language::Cantonese), QStringLiteral("zh-HK"));
        QCOMPARE(languageDisplayName(Language::English), QStringLiteral("English"));
        QCOMPARE(languageDisplayName(Language::Cantonese), QStringLiteral("Cantonese (廣東話)"));
        QCOMPARE(sourceLanguage(Direction::EnglishToCantonese), Language::English);
        QCOMPARE(targetLanguage(Direction::EnglishToCantonese), Language::Cantonese);
        QCOMPARE_ENUM(reversed(Direction::CantoneseToEnglish), Direction::EnglishToCantonese);
    }

    void stringIds()
    {
        QCOMPARE(toString(Direction::EnglishToCantonese), QStringLiteral("en2yue"));
        QCOMPARE(toString(Direction::CantoneseToEnglish), QStringLiteral("yue2en"));
        QCOMPARE(toString(Tone::Casual), QStringLiteral("casual"));
        QCOMPARE(toString(Tone::Neutral), QStringLiteral("neutral"));
        QCOMPARE(toString(Tone::Polite), QStringLiteral("polite"));
        QCOMPARE(toString(ChineseScript::Traditional), QStringLiteral("traditional"));
        QCOMPARE(toString(ChineseScript::Simplified), QStringLiteral("simplified"));

        for (Direction d : {Direction::EnglishToCantonese, Direction::CantoneseToEnglish})
            QCOMPARE_ENUM(directionFromString(toString(d)), d);
        for (Tone t : {Tone::Casual, Tone::Neutral, Tone::Polite})
            QCOMPARE_ENUM(toneFromString(toString(t)), t);
        for (ChineseScript s : {ChineseScript::Traditional, ChineseScript::Simplified})
            QCOMPARE_ENUM(scriptFromString(toString(s)), s);

        QCOMPARE_ENUM(directionFromString(QStringLiteral(" YUE2EN ")), Direction::CantoneseToEnglish);
        QCOMPARE_ENUM(directionFromString(QStringLiteral("bogus"), Direction::CantoneseToEnglish),
                 Direction::CantoneseToEnglish);
        QCOMPARE_ENUM(toneFromString(QString(), Tone::Polite), Tone::Polite);
        QCOMPARE_ENUM(toneFromString(QStringLiteral("rude")), Tone::Neutral);
        QCOMPARE_ENUM(scriptFromString(QStringLiteral("cursive")), ChineseScript::Traditional);
    }

    void cacheKey()
    {
        TranslationRequest a;
        a.text = QStringLiteral("Where is he?");
        TranslationRequest b = a;
        b.text = QStringLiteral("  Where is he?\n");
        QCOMPARE(a.cacheKey(), b.cacheKey());  // surrounding whitespace ignored

        TranslationRequest c = a;
        c.tone = Tone::Casual;
        QVERIFY(a.cacheKey() != c.cacheKey());
        TranslationRequest d = a;
        d.script = ChineseScript::Simplified;
        QVERIFY(a.cacheKey() != d.cacheKey());
        TranslationRequest e = a;
        e.direction = Direction::CantoneseToEnglish;
        QVERIFY(a.cacheKey() != e.cacheKey());
        TranslationRequest f = a;
        f.wantAlternatives = false;
        QVERIFY(a.cacheKey() != f.cacheKey());
        TranslationRequest g = a;
        g.wantNotes = false;
        QVERIFY(a.cacheKey() != g.cacheKey());
        TranslationRequest h = a;
        h.text = QStringLiteral("where is he?");
        QVERIFY(a.cacheKey() != h.cacheKey());
    }

    void requestJsonRoundTrip()
    {
        TranslationRequest r;
        r.text = QStringLiteral("你食咗飯未呀？\nSecond line");
        r.direction = Direction::CantoneseToEnglish;
        r.tone = Tone::Polite;
        r.script = ChineseScript::Simplified;
        r.wantAlternatives = false;
        r.wantNotes = false;

        const TranslationRequest back = TranslationRequest::fromJson(r.toJson());
        QCOMPARE(back.text, r.text);
        QCOMPARE_ENUM(back.direction, r.direction);
        QCOMPARE_ENUM(back.tone, r.tone);
        QCOMPARE_ENUM(back.script, r.script);
        QCOMPARE(back.wantAlternatives, false);
        QCOMPARE(back.wantNotes, false);
        QCOMPARE(r.toJson().value(QStringLiteral("direction")).toString(), QStringLiteral("yue2en"));

        // Missing fields fall back to defaults.
        const TranslationRequest empty = TranslationRequest::fromJson(QJsonObject());
        QCOMPARE_ENUM(empty.direction, Direction::EnglishToCantonese);
        QCOMPARE_ENUM(empty.tone, Tone::Neutral);
        QCOMPARE_ENUM(empty.script, ChineseScript::Traditional);
        QVERIFY(empty.wantAlternatives);
        QVERIFY(empty.wantNotes);
    }

    void resultJsonRoundTrip()
    {
        TranslationResult r;
        r.translation = QStringLiteral("佢喺邊度呀？");
        r.jyutping = QStringLiteral("keoi5 hai2 bin1 dou6 aa3?");
        r.literal = QStringLiteral("he at where (soft question)?");
        r.alternatives.append({QStringLiteral("佢去咗邊呀？"), QStringLiteral("keoi5 heoi3 zo2 bin1 aa3?"),
                               QStringLiteral("Asks where he went.")});
        r.notes = {QStringLiteral("呀 softens the question.")};
        r.providerId = QStringLiteral("claude");
        r.model = QStringLiteral("claude-opus-5-5");
        r.request.text = QStringLiteral("Where is he?");
        r.request.tone = Tone::Casual;
        r.timestamp = QDateTime(QDate(2026, 10, 5), QTime(8, 30, 15, 123), Qt::UTC);
        r.fromCache = true;

        const QJsonObject json = r.toJson();
        QCOMPARE(json.value(QStringLiteral("timestamp")).toString(), QStringLiteral("2026-10-05T08:30:15.123Z"));

        const TranslationResult back = TranslationResult::fromJson(json);
        QCOMPARE(back.translation, r.translation);
        QCOMPARE(back.jyutping, r.jyutping);
        QCOMPARE(back.literal, r.literal);
        QCOMPARE(back.alternatives.size(), 1);
        QCOMPARE(back.alternatives.first().text, r.alternatives.first().text);
        QCOMPARE(back.alternatives.first().jyutping, r.alternatives.first().jyutping);
        QCOMPARE(back.alternatives.first().note, r.alternatives.first().note);
        QCOMPARE(back.notes, r.notes);
        QCOMPARE(back.providerId, r.providerId);
        QCOMPARE(back.model, r.model);
        QCOMPARE(back.request.text, r.request.text);
        QCOMPARE(back.request.tone, Tone::Casual);
        QCOMPARE(back.timestamp, r.timestamp);
        QCOMPARE(back.timestamp.timeSpec(), Qt::UTC);
        QVERIFY(!back.fromCache);  // transient flag is not persisted
        QVERIFY(back.isValid());
    }

    void timestampWithoutMillisAndOffset()
    {
        QJsonObject o;
        o.insert(QStringLiteral("translation"), QStringLiteral("x"));
        o.insert(QStringLiteral("timestamp"), QStringLiteral("2026-10-05T10:00:00+02:00"));
        const TranslationResult r = TranslationResult::fromJson(o);
        QCOMPARE(r.timestamp, QDateTime(QDate(2026, 10, 5), QTime(8, 0), Qt::UTC));
    }

    void metaTypes()
    {
        registerMetaTypes();
        QVERIFY(QMetaType::fromName("sct::TranslationResult").isValid());
        QVERIFY(QMetaType::fromName("sct::TranslationError").isValid());
        QVERIFY(QMetaType::fromName("sct::TranslationRequest").isValid());
    }
};

QTEST_GUILESS_MAIN(TstTranslationTypes)
#include "tst_translationtypes.moc"
