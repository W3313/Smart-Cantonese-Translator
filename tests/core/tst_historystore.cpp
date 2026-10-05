#include "core/HistoryStore.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

using namespace sct;

namespace {

TranslationResult makeResult(const QString &source, const QString &translation,
                             Direction direction = Direction::EnglishToCantonese,
                             const QString &jyutping = QString())
{
    TranslationResult r;
    r.request.text = source;
    r.request.direction = direction;
    r.translation = translation;
    r.jyutping = jyutping;
    r.providerId = QStringLiteral("claude");
    r.model = QStringLiteral("claude-opus-5-5");
    r.timestamp = QDateTime::currentDateTimeUtc();
    return r;
}

} // namespace

class TstHistoryStore : public QObject
{
    Q_OBJECT

private:
    QString path() const { return m_dir->filePath(QStringLiteral("sub/history.json")); }
    std::unique_ptr<QTemporaryDir> m_dir;

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void missingFileLoadsEmpty()
    {
        HistoryStore store(path());
        QVERIFY(store.load());
        QVERIFY(store.entries().isEmpty());
    }

    void addNewestFirst()
    {
        HistoryStore store(path());
        QSignalSpy spy(&store, &HistoryStore::changed);
        const QUuid a = store.add(makeResult(QStringLiteral("one"), QStringLiteral("一")));
        const QUuid b = store.add(makeResult(QStringLiteral("two"), QStringLiteral("二")));
        QVERIFY(!a.isNull());
        QVERIFY(!b.isNull());
        QVERIFY(a != b);
        QCOMPARE(store.entries().size(), 2);
        QCOMPARE(store.entries().at(0).id, b);
        QCOMPARE(store.entries().at(0).result.translation, QStringLiteral("二"));
        QCOMPARE(spy.count(), 2);
        QVERIFY(QFile::exists(path()));  // auto-saved, directory created
    }

    void invalidResultIgnored()
    {
        HistoryStore store(path());
        QVERIFY(store.add(makeResult(QStringLiteral("x"), QStringLiteral("   "))).isNull());
        QVERIFY(store.entries().isEmpty());
    }

    void dedupeNewest()
    {
        HistoryStore store(path());
        const QUuid first = store.add(makeResult(QStringLiteral("Hello"), QStringLiteral("你好")));
        store.setStarred(first, true);
        const QUuid again = store.add(makeResult(QStringLiteral("Hello "), QStringLiteral("你好")));
        QCOMPARE(again, first);
        QCOMPARE(store.entries().size(), 1);
        QVERIFY(store.entries().first().starred);  // star kept

        // Different direction, translation, or not the newest -> new entry.
        store.add(makeResult(QStringLiteral("Hello"), QStringLiteral("哈囉")));
        QCOMPARE(store.entries().size(), 2);
        store.add(makeResult(QStringLiteral("Hello"), QStringLiteral("哈囉"), Direction::CantoneseToEnglish));
        QCOMPARE(store.entries().size(), 3);
        store.add(makeResult(QStringLiteral("Hello"), QStringLiteral("你好")));
        QCOMPARE(store.entries().size(), 4);
    }

    void limitWithStarredExempt()
    {
        HistoryStore store(path(), 3);
        const QUuid a = store.add(makeResult(QStringLiteral("a"), QStringLiteral("A")));
        store.add(makeResult(QStringLiteral("b"), QStringLiteral("B")));
        store.add(makeResult(QStringLiteral("c"), QStringLiteral("C")));
        store.add(makeResult(QStringLiteral("d"), QStringLiteral("D")));
        QCOMPARE(store.entries().size(), 3);  // plain limit: oldest (a) evicted
        QCOMPARE(store.entries().last().result.translation, QStringLiteral("B"));

        const QUuid b = store.entries().last().id;
        QVERIFY(!a.isNull());
        QVERIFY(store.setStarred(b, true));
        store.add(makeResult(QStringLiteral("e"), QStringLiteral("E")));
        store.add(makeResult(QStringLiteral("f"), QStringLiteral("F")));

        // Starred entries are exempt and do not count towards the limit.
        QCOMPARE(store.entries().size(), 4);
        QCOMPARE(store.entries().at(0).result.translation, QStringLiteral("F"));
        QCOMPARE(store.entries().at(1).result.translation, QStringLiteral("E"));
        QCOMPARE(store.entries().at(2).result.translation, QStringLiteral("D"));
        QCOMPARE(store.entries().at(3).id, b);  // oldest but starred: kept

        // Even when everything is starred, new translations are still recorded.
        for (const HistoryEntry &e : store.entries())
            store.setStarred(e.id, true);
        store.add(makeResult(QStringLiteral("g"), QStringLiteral("G")));
        QCOMPARE(store.entries().size(), 5);
        QCOMPARE(store.entries().first().result.translation, QStringLiteral("G"));

        // Unstarring brings the limit back into force.
        for (const HistoryEntry &e : store.entries())
            store.setStarred(e.id, false);
        QCOMPARE(store.entries().size(), 3);
        QCOMPARE(store.entries().first().result.translation, QStringLiteral("G"));
    }

    void search()
    {
        HistoryStore store(path());
        store.add(makeResult(QStringLiteral("Where is he?"), QStringLiteral("佢喺邊度呀？"),
                             Direction::EnglishToCantonese, QStringLiteral("keoi5 hai2 bin1 dou6 aa3?")));
        const QUuid thanks = store.add(makeResult(QStringLiteral("Thank you"), QStringLiteral("唔該"),
                                                  Direction::EnglishToCantonese, QStringLiteral("m4 goi1")));
        store.add(makeResult(QStringLiteral("你食咗飯未呀？"), QStringLiteral("Have you eaten yet?"),
                             Direction::CantoneseToEnglish, QStringLiteral("nei5 sik6 zo2 faan6 mei6 aa3?")));

        QCOMPARE(store.search(QString()).size(), 3);
        QCOMPARE(store.search(QStringLiteral("WHERE")).size(), 1);         // source, case-insensitive
        QCOMPARE(store.search(QStringLiteral("邊度")).size(), 1);           // translation
        QCOMPARE(store.search(QStringLiteral("eaten")).size(), 1);         // translation (English)
        QCOMPARE(store.search(QStringLiteral("goi1")).size(), 1);          // jyutping
        QCOMPARE(store.search(QStringLiteral("keoi hai bin")).size(), 1);  // jyutping without tones
        QCOMPARE(store.search(QStringLiteral("nothing-matches")).size(), 0);

        store.setStarred(thanks, true);
        QCOMPARE(store.search(QString(), true).size(), 1);
        QCOMPARE(store.search(QStringLiteral("where"), true).size(), 0);
        QCOMPARE(store.search(QStringLiteral("thank"), true).first().id, thanks);
    }

    void persistAndReload()
    {
        QUuid starredId;
        {
            HistoryStore store(path());
            store.add(makeResult(QStringLiteral("one"), QStringLiteral("一")));
            starredId = store.add(makeResult(QStringLiteral("two"), QStringLiteral("二")));
            store.setStarred(starredId, true);
        }
        HistoryStore reloaded(path());
        QSignalSpy spy(&reloaded, &HistoryStore::changed);
        QVERIFY(reloaded.load());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(reloaded.entries().size(), 2);
        QCOMPARE(reloaded.entries().at(0).id, starredId);
        QVERIFY(reloaded.entries().at(0).starred);
        QCOMPARE(reloaded.entries().at(0).result.request.text, QStringLiteral("two"));
        QVERIFY(reloaded.entries().at(0).result.timestamp.isValid());
        QVERIFY(!reloaded.entries().at(1).starred);

        QFile f(path());
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        QCOMPARE(root.value(QStringLiteral("version")).toInt(), 1);
        QCOMPARE(root.value(QStringLiteral("entries")).toArray().size(), 2);
    }

    void corruptFile()
    {
        QDir().mkpath(QFileInfo(path()).absolutePath());
        {
            QFile f(path());
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("{ this is not json");
        }
        HistoryStore store(path());
        QVERIFY(!store.load());
        QVERIFY(store.entries().isEmpty());
        QVERIFY(QFile::exists(path() + QStringLiteral(".corrupt")));  // kept for inspection

        // The store still works afterwards.
        store.add(makeResult(QStringLiteral("x"), QStringLiteral("y")));
        HistoryStore reloaded(path());
        QVERIFY(reloaded.load());
        QCOMPARE(reloaded.entries().size(), 1);
    }

    void wrongShapeIsCorrupt()
    {
        QDir().mkpath(QFileInfo(path()).absolutePath());
        {
            QFile f(path());
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(R"({"version":1,"entries":"nope"})");
        }
        HistoryStore store(path());
        QVERIFY(!store.load());
        QVERIFY(store.entries().isEmpty());
    }

    void removeAndClear()
    {
        HistoryStore store(path());
        const QUuid a = store.add(makeResult(QStringLiteral("a"), QStringLiteral("A")));
        const QUuid b = store.add(makeResult(QStringLiteral("b"), QStringLiteral("B")));
        store.add(makeResult(QStringLiteral("c"), QStringLiteral("C")));
        QVERIFY(store.remove(a));
        QVERIFY(!store.remove(a));
        QCOMPARE(store.entries().size(), 2);
        QVERIFY(!store.setStarred(QUuid::createUuid(), true));

        store.setStarred(b, true);
        store.clear(true);
        QCOMPARE(store.entries().size(), 1);
        QCOMPARE(store.entries().first().id, b);
        store.clear();
        QVERIFY(store.entries().isEmpty());

        HistoryStore reloaded(path());
        QVERIFY(reloaded.load());
        QVERIFY(reloaded.entries().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstHistoryStore)
#include "tst_historystore.moc"
