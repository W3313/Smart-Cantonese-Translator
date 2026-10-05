#include "tts/AudioCache.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

using sct::AudioCache;

namespace {

// Creates a cache file of the given size and modification time.
QString makeEntry(const AudioCache &cache, const QString &key, int bytes, const QDateTime &mtime)
{
    const QString path = cache.filePath(key);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return QString();
    f.write(QByteArray(bytes, 'x'));
    f.flush();  // so closing does not bump the modification time again
    f.setFileTime(mtime, QFileDevice::FileModificationTime);
    f.close();
    return path;
}

QDateTime longAgo(int secondsOffset)
{
    return QDateTime::currentDateTimeUtc().addDays(-10).addSecs(secondsOffset);
}

} // namespace

class TestAudioCache : public QObject
{
    Q_OBJECT

private slots:
    void keyIsDeterministic()
    {
        const QString voice = QStringLiteral("zh-HK-HiuMaanNeural");
        const QString text = QStringLiteral("你好，歡迎！");
        const QString k = AudioCache::key(voice, 0.25, text);
        QCOMPARE(k, AudioCache::key(voice, 0.25, text));
        QCOMPARE(k.size(), qsizetype(40));  // hex SHA-1
        for (const QChar c : k)
            QVERIFY((c >= QLatin1Char('0') && c <= QLatin1Char('9')) || (c >= QLatin1Char('a') && c <= QLatin1Char('f')));

        QVERIFY(k != AudioCache::key(QStringLiteral("zh-HK-WanLungNeural"), 0.25, text));
        QVERIFY(k != AudioCache::key(voice, 0.5, text));
        QVERIFY(k != AudioCache::key(voice, 0.25, text + QLatin1Char('.')));
        // Floating-point noise and signed zero map to the same entry.
        QCOMPARE(AudioCache::key(voice, 0.1, text), AudioCache::key(voice, 0.1000001, text));
        QCOMPARE(AudioCache::key(voice, -0.0, text), AudioCache::key(voice, 0.0, text));
        // Known value: SHA-1 of "v|0|t".
        QCOMPARE(AudioCache::key(QStringLiteral("v"), 0.0, QStringLiteral("t")),
                 QString::fromLatin1(QCryptographicHash::hash("v|0|t", QCryptographicHash::Sha1).toHex()));
    }

    void storeAndLookup()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AudioCache cache(tmp.filePath(QStringLiteral("audio-cache")));
        const QString key = AudioCache::key(QStringLiteral("v"), 0.0, QStringLiteral("hello"));

        QVERIFY(cache.lookup(key).isEmpty());
        QString error;
        const QString path = cache.store(key, QByteArrayLiteral("ID3fake-mp3"), &error);
        QVERIFY2(!path.isEmpty(), qPrintable(error));
        QCOMPARE(path, cache.filePath(key));
        QVERIFY(path.endsWith(key + QStringLiteral(".mp3")));
        QCOMPARE(cache.lookup(key), path);
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArrayLiteral("ID3fake-mp3"));
        f.close();  // Windows cannot delete a file that is still open
        QCOMPARE(cache.fileCount(), 1);
        QCOMPARE(cache.totalBytes(), qint64(11));

        cache.remove(key);
        QVERIFY(cache.lookup(key).isEmpty());
    }

    void storeFailures()
    {
        AudioCache noDir;
        QString error;
        QVERIFY(noDir.store(QStringLiteral("abc"), QByteArrayLiteral("x"), &error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(noDir.lookup(QStringLiteral("abc")).isEmpty());

        QTemporaryDir tmp;
        AudioCache cache(tmp.path());
        error.clear();
        QVERIFY(cache.store(QStringLiteral("abc"), QByteArray(), &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void evictsOldestBeyondFileLimit()
    {
        QTemporaryDir tmp;
        AudioCache cache(tmp.path(), 3, 1024 * 1024);
        QStringList paths;
        for (int i = 0; i < 5; ++i)
            paths << makeEntry(cache, QStringLiteral("k%1").arg(i), 10, longAgo(i * 60));
        QCOMPARE(cache.fileCount(), 5);

        QCOMPARE(cache.evict(), 2);
        QCOMPARE(cache.fileCount(), 3);
        QVERIFY(!QFile::exists(paths.at(0)));
        QVERIFY(!QFile::exists(paths.at(1)));
        QVERIFY(QFile::exists(paths.at(2)));
        QVERIFY(QFile::exists(paths.at(4)));
    }

    void evictsOldestBeyondByteLimit()
    {
        QTemporaryDir tmp;
        AudioCache cache(tmp.path(), 100, 250);
        QStringList paths;
        for (int i = 0; i < 4; ++i)
            paths << makeEntry(cache, QStringLiteral("k%1").arg(i), 100, longAgo(i * 60));
        cache.evict();
        QCOMPARE(cache.fileCount(), 2);
        QVERIFY(cache.totalBytes() <= 250);
        QVERIFY(QFile::exists(paths.at(3)));
        QVERIFY(QFile::exists(paths.at(2)));
    }

    void evictionKeepsProtectedFile()
    {
        QTemporaryDir tmp;
        AudioCache cache(tmp.path(), 2, 1024 * 1024);
        QStringList paths;
        for (int i = 0; i < 4; ++i)
            paths << makeEntry(cache, QStringLiteral("k%1").arg(i), 10, longAgo(i * 60));
        cache.evict(paths.at(0));  // oldest, but currently playing
        QCOMPARE(cache.fileCount(), 2);
        QVERIFY(QFile::exists(paths.at(0)));
        QVERIFY(QFile::exists(paths.at(3)));
    }

    void storeEvictsButKeepsNewEntry()
    {
        QTemporaryDir tmp;
        AudioCache cache(tmp.path(), 2, 1024 * 1024);
        // Existing entries look newer than the one being stored.
        const QDateTime future = QDateTime::currentDateTimeUtc().addDays(1);
        const QString a = makeEntry(cache, QStringLiteral("a"), 10, future);
        const QString b = makeEntry(cache, QStringLiteral("b"), 10, future.addSecs(60));
        const QString fresh = cache.store(QStringLiteral("c"), QByteArrayLiteral("new"));
        QVERIFY(!fresh.isEmpty());
        QVERIFY(QFile::exists(fresh));
        QCOMPARE(cache.fileCount(), 2);
        QVERIFY(!QFile::exists(a));
        QVERIFY(QFile::exists(b));
    }

    void lookupMarksRecentlyUsed()
    {
        QTemporaryDir tmp;
        AudioCache cache(tmp.path(), 2, 1024 * 1024);
        const QString oldest = makeEntry(cache, QStringLiteral("old"), 10, longAgo(0));
        const QString middle = makeEntry(cache, QStringLiteral("mid"), 10, longAgo(60));
        const QString newest = makeEntry(cache, QStringLiteral("new"), 10, longAgo(120));
        QCOMPARE(cache.lookup(QStringLiteral("old")), oldest);  // now most recently used
        cache.evict();
        QVERIFY(QFile::exists(oldest));
        QVERIFY(QFile::exists(newest));
        QVERIFY(!QFile::exists(middle));
    }

    void clearRemovesEverything()
    {
        QTemporaryDir tmp;
        AudioCache cache(tmp.path());
        cache.store(QStringLiteral("a"), QByteArrayLiteral("1"));
        cache.store(QStringLiteral("b"), QByteArrayLiteral("2"));
        QFile other(QDir(tmp.path()).filePath(QStringLiteral("keep.txt")));
        QVERIFY(other.open(QIODevice::WriteOnly));
        other.close();
        QCOMPARE(cache.fileCount(), 2);
        cache.clear();
        QCOMPARE(cache.fileCount(), 0);
        QVERIFY(QFile::exists(other.fileName()));  // only *.mp3 files are touched
    }
};

QTEST_GUILESS_MAIN(TestAudioCache)
#include "tst_audiocache.moc"
