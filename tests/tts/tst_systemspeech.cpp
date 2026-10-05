// SystemSpeechEngine against scripted fake QTextToSpeech backends (static
// plugins named like the Windows ones), so its state handling is tested
// without a real speech engine or audio device.

#define QT_STATICPLUGIN

#include "tts/SystemSpeechEngine.h"

#include <QAudioDevice>
#include <QMediaDevices>
#include <QSignalSpy>
#include <QTextToSpeechEngine>
#include <QTextToSpeechPlugin>
#include <QTimer>
#include <QVoice>
#include <QtPlugin>
#include <QtTest>

using namespace sct;

// A speech backend the test drives by hand.
class FakeTtsEngine : public QTextToSpeechEngine
{
    Q_OBJECT

public:
    static inline FakeTtsEngine *current = nullptr;  // most recently created
    static inline bool autoStart = true;  // say() starts speaking on the next event-loop turn

    explicit FakeTtsEngine(QObject *parent)
        : QTextToSpeechEngine(parent)
    {
        current = this;
    }
    ~FakeTtsEngine() override
    {
        if (current == this)
            current = nullptr;
    }

    QList<QLocale> availableLocales() const override { return {english(), cantonese()}; }
    QList<QVoice> availableVoices() const override
    {
        if (m_locale == cantonese())
            return {createVoice(QStringLiteral("Fake Tracy"), cantonese(), QVoice::Female, QVoice::Adult, {})};
        return {createVoice(QStringLiteral("Fake Zira"), english(), QVoice::Female, QVoice::Adult, {})};
    }

    void say(const QString &text) override
    {
        ++sayCount;
        lastText = text;
        if (autoStart)
            QTimer::singleShot(0, this, [this] { setTtsState(QTextToSpeech::Speaking); });
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    void synthesize(const QString &) override {}
#endif
    void stop(QTextToSpeech::BoundaryHint) override
    {
        ++stopCount;
        setTtsState(QTextToSpeech::Ready);
    }
    void pause(QTextToSpeech::BoundaryHint) override {}
    void resume() override {}

    double rate() const override { return m_rate; }
    bool setRate(double rate) override
    {
        m_rate = rate;
        return true;
    }
    double pitch() const override { return 0.0; }
    bool setPitch(double) override { return true; }
    QLocale locale() const override { return m_locale; }
    bool setLocale(const QLocale &locale) override
    {
        m_locale = locale;
        return true;
    }
    double volume() const override { return 1.0; }
    bool setVolume(double) override { return true; }
    QVoice voice() const override { return m_voice; }
    bool setVoice(const QVoice &voice) override
    {
        m_voice = voice;
        m_locale = voice.locale();
        return true;
    }
    QTextToSpeech::State state() const override { return m_state; }
    QTextToSpeech::ErrorReason errorReason() const override { return QTextToSpeech::ErrorReason::NoError; }
    QString errorString() const override { return QString(); }

    void setTtsState(QTextToSpeech::State state)
    {
        if (m_state == state)
            return;
        m_state = state;
        emit stateChanged(state);
    }

    int sayCount = 0;
    int stopCount = 0;
    QString lastText;

private:
    static QLocale english() { return QLocale(QLocale::English, QLocale::UnitedStates); }
    static QLocale cantonese() { return QLocale(QLocale::Chinese, QLocale::HongKong); }

    QTextToSpeech::State m_state = QTextToSpeech::Ready;
    QLocale m_locale = english();
    QVoice m_voice;
    double m_rate = 0.0;
};

class FakeSapiPlugin : public QObject, public QTextToSpeechPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.qt.speech.tts.plugin/6.0" FILE "fake_tts_sapi.json")
    Q_INTERFACES(QTextToSpeechPlugin)

public:
    static inline bool enabled = true;
    QTextToSpeechEngine *createTextToSpeechEngine(const QVariantMap &, QObject *parent,
                                                  QString *errorString) const override
    {
        if (!enabled) {
            *errorString = QStringLiteral("disabled by the test");
            return nullptr;
        }
        return new FakeTtsEngine(parent);
    }
};

class FakeWinrtPlugin : public QObject, public QTextToSpeechPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.qt.speech.tts.plugin/6.0" FILE "fake_tts_winrt.json")
    Q_INTERFACES(QTextToSpeechPlugin)

public:
    static inline bool enabled = false;
    QTextToSpeechEngine *createTextToSpeechEngine(const QVariantMap &, QObject *parent,
                                                  QString *errorString) const override
    {
        if (!enabled) {
            *errorString = QStringLiteral("disabled by the test");
            return nullptr;
        }
        return new FakeTtsEngine(parent);
    }
};

class TstSystemSpeech : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
#ifdef Q_OS_WIN
        QSKIP("The fake backends share their names with the real Windows ones.");
#endif
    }

    void init()
    {
        FakeTtsEngine::autoStart = true;
        FakeSapiPlugin::enabled = true;
        FakeWinrtPlugin::enabled = false;
    }

    void speaksAndFinishes()
    {
        SystemSpeechEngine engine;
        QCOMPARE(engine.backendName(), QStringLiteral("sapi"));
        QVERIFY(engine.hasVoiceFor(Language::English));
        QVERIFY(engine.hasVoiceFor(Language::Cantonese));
        FakeTtsEngine *fake = FakeTtsEngine::current;
        QVERIFY(fake);
        QSignalSpy states(&engine, &SpeechEngine::stateChanged);
        QSignalSpy errors(&engine, &SpeechEngine::errorOccurred);

        engine.speak(QStringLiteral("Tom & Jerry <3"), Language::English);
        QCOMPARE(engine.state(), SpeechEngine::State::Loading);
        QCOMPARE(fake->lastText, QStringLiteral("Tom &amp; Jerry &lt;3"));  // SAPI parses XML
        QTRY_COMPARE(engine.state(), SpeechEngine::State::Speaking);
        fake->setTtsState(QTextToSpeech::Ready);  // finished
        QCOMPARE(engine.state(), SpeechEngine::State::Idle);
        QCOMPARE(states.size(), 3);
        QCOMPARE(errors.size(), 0);
    }

    void silentBackendDoesNotStayLoading()
    {
        // e.g. Qt 6.8's WinRT backend when its audio sink cannot start: no
        // error and no state change at all.
        FakeTtsEngine::autoStart = false;
        SystemSpeechEngine engine;
        engine.setNoResponseTimeout(100);
        FakeTtsEngine *fake = FakeTtsEngine::current;
        QVERIFY(fake);
        QSignalSpy errors(&engine, &SpeechEngine::errorOccurred);

        engine.speak(QStringLiteral("Hello"), Language::English);
        QCOMPARE(engine.state(), SpeechEngine::State::Loading);
        QTRY_COMPARE(engine.state(), SpeechEngine::State::Idle);
        QCOMPARE(errors.size(), 0);

        // It starts after all (a long text took long to synthesize): tracked
        // again, so it shows as speaking and can be stopped.
        fake->setTtsState(QTextToSpeech::Speaking);
        QCOMPARE(engine.state(), SpeechEngine::State::Speaking);
        const int stops = fake->stopCount;
        engine.stop();
        QCOMPARE(engine.state(), SpeechEngine::State::Idle);
        QVERIFY(fake->stopCount > stops);
    }

    void stopCancelsATextThatWasGivenUp()
    {
        FakeTtsEngine::autoStart = false;
        SystemSpeechEngine engine;
        engine.setNoResponseTimeout(100);
        FakeTtsEngine *fake = FakeTtsEngine::current;
        QVERIFY(fake);

        engine.speak(QStringLiteral("Hello"), Language::English);
        QTRY_COMPARE(engine.state(), SpeechEngine::State::Idle);
        const int stops = fake->stopCount;
        engine.stop();  // Idle, but the backend must still be told to stop
        QVERIFY(fake->stopCount > stops);
        fake->setTtsState(QTextToSpeech::Speaking);  // stale: was cancelled
        QCOMPARE(engine.state(), SpeechEngine::State::Idle);
    }

    void winrtWithoutAudioDeviceFailsAtOnce()
    {
        if (!QMediaDevices::audioOutputs().isEmpty())
            QSKIP("An audio output device is present.");
        FakeWinrtPlugin::enabled = true;
        SystemSpeechEngine engine;
        QCOMPARE(engine.backendName(), QStringLiteral("winrt"));
        FakeTtsEngine *fake = FakeTtsEngine::current;
        QVERIFY(fake);
        QSignalSpy errors(&engine, &SpeechEngine::errorOccurred);

        engine.speak(QStringLiteral("Hello"), Language::English);
        QCOMPARE(errors.size(), 1);
        QVERIFY(errors.first().first().toString().contains(QStringLiteral("audio output device")));
        QCOMPARE(engine.state(), SpeechEngine::State::Idle);
        QCOMPARE(fake->sayCount, 0);
        QVERIFY(engine.isAvailable());  // a device may be plugged in later
    }
};

Q_IMPORT_PLUGIN(FakeSapiPlugin)
Q_IMPORT_PLUGIN(FakeWinrtPlugin)

QTEST_GUILESS_MAIN(TstSystemSpeech)
#include "tst_systemspeech.moc"
