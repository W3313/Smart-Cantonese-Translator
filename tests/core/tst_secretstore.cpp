#include "core/SecretStore.h"

#include <QtTest>

using namespace sct;

class TstSecretStore : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip_data()
    {
        QTest::addColumn<QString>("plain");
        QTest::newRow("ascii key") << QStringLiteral("sk-ant-api03-abcdefghijklmnopqrstuvwxyz0123456789");
        QTest::newRow("unicode") << QStringLiteral("密碼 🔑 pässwörd");
        QTest::newRow("single char") << QStringLiteral("x");
        QTest::newRow("long") << QString(5000, QLatin1Char('k'));
    }

    void roundTrip()
    {
        QFETCH(QString, plain);
        const QByteArray stored = SecretStore::protect(plain);
        QVERIFY(!stored.isEmpty());
#ifdef Q_OS_WIN
        QVERIFY(stored.startsWith("dpapi:"));
#else
        QVERIFY(stored.startsWith("obf:"));
#endif
        if (plain.size() > 3)
            QVERIFY(!stored.contains(plain.toUtf8()));
        QCOMPARE(SecretStore::unprotect(stored), plain);
    }

    void emptyInput()
    {
        QVERIFY(SecretStore::protect(QString()).isEmpty());
        QVERIFY(SecretStore::unprotect(QByteArray()).isEmpty());
        QVERIFY(SecretStore::unprotect("   ").isEmpty());
    }

    void garbage_data()
    {
        QTest::addColumn<QByteArray>("stored");
        QTest::newRow("plain text") << QByteArray("sk-plaintext-key");
        QTest::newRow("unknown scheme") << QByteArray("rot13:YWJj");
        QTest::newRow("obf prefix only") << QByteArray("obf:");
        QTest::newRow("obf bad base64") << QByteArray("obf:!!!not base64***");
        QTest::newRow("obf random payload") << (QByteArray("obf:") + QByteArray("random bytes here").toBase64());
        QTest::newRow("dpapi bad base64") << QByteArray("dpapi:@@@@");
        QTest::newRow("dpapi random payload") << (QByteArray("dpapi:") + QByteArray(64, 'z').toBase64());
        QTest::newRow("binary") << QByteArray("\x00\x01\x02\xff\xfe", 5);
    }

    void garbage()
    {
        QFETCH(QByteArray, stored);
        QVERIFY(SecretStore::unprotect(stored).isEmpty());
    }

    void toleratesSurroundingWhitespace()
    {
        const QByteArray stored = SecretStore::protect(QStringLiteral("key-123"));
        QCOMPARE(SecretStore::unprotect(" " + stored + "\n"), QStringLiteral("key-123"));
    }
};

QTEST_GUILESS_MAIN(TstSecretStore)
#include "tst_secretstore.moc"
