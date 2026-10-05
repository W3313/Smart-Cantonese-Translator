// UI smoke tests for the main window, result view, history panel and the
// Settings dialog. Runs headless (QT_QPA_PLATFORM=offscreen); no network: the
// AI provider is replaced with a scriptable fake.
//
// Set SCT_SCREENSHOT_DIR=<dir> to also save PNGs of every major state
// (light and dark) for visual review.

#include "core/AppSettings.h"
#include "core/HistoryStore.h"
#include "core/TranslationProvider.h"
#include "core/TranslationService.h"
#include "tts/SpeechService.h"
#include "ui/Controls.h"
#include "ui/HistoryPanel.h"
#include "ui/InputPane.h"
#include "ui/MainWindow.h"
#include "ui/Motion.h"
#include "ui/ResultView.h"
#include "ui/SettingsDialog.h"
#include "ui/SpeechController.h"
#include "ui/Surfaces.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QLabel>
#include <QPointer>
#include <QDir>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScreen>
#include <QStyleFactory>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace sct;

// QCOMPARE pretty-printers for the enums (found by ADL).
namespace sct {
char *toString(ResultView::Page p) { return qstrdup(QByteArray::number(int(p)).constData()); }
char *toString(SettingsDialog::Tab t) { return qstrdup(QByteArray::number(int(t)).constData()); }
} // namespace sct

namespace {

// Scriptable provider: the test decides when and how a request completes.
class FakeProvider : public TranslationProvider
{
    Q_OBJECT

public:
    using TranslationProvider::TranslationProvider;

    QString id() const override { return QStringLiteral("claude"); }
    QString displayName() const override { return QStringLiteral("Claude (Anthropic)"); }
    void setApiKey(const QString &key) override { apiKey = key; }
    void setModel(const QString &m) override { modelId = m.isEmpty() ? QStringLiteral("claude-opus-5-5") : m; }
    QString model() const override { return modelId; }
    void setQuality(const QString &) override {}
    bool isConfigured() const override { return !apiKey.isEmpty(); }
    bool isBusy() const override { return busy; }
    void translate(const TranslationRequest &request) override
    {
        if (busy)
            cancel();
        busy = true;
        last = request;
        ++translateCalls;
    }
    void cancel() override
    {
        if (!busy)
            return;
        busy = false;
        TranslationError e;
        e.kind = ErrorKind::Cancelled;
        emit failed(e);
    }
    void listModels(const QString &) override
    {
        ++listCalls;
        QTimer::singleShot(0, this, [this] {
            emit modelsListed({QStringLiteral("claude-opus-5-5"), QStringLiteral("claude-sonnet-5-5")});
        });
    }
    QStringList suggestedModels() const override { return {QStringLiteral("claude-opus-5-5")}; }
    QString defaultModel() const override { return QStringLiteral("claude-opus-5-5"); }

    void complete(const TranslationResult &r)
    {
        busy = false;
        emit finished(r);
    }
    void fail(ErrorKind kind, const QString &message)
    {
        busy = false;
        TranslationError e;
        e.kind = kind;
        e.message = message;
        e.detail = QStringLiteral("HTTP/1.1 429 Too Many Requests\n{\"type\":\"rate_limit_error\"}");
        e.httpStatus = 429;
        emit failed(e);
    }

    QString apiKey;
    QString modelId = QStringLiteral("claude-opus-5-5");
    bool busy = false;
    int translateCalls = 0;
    int listCalls = 0;
    TranslationRequest last;
};

TranslationResult sampleResult(Direction d = Direction::EnglishToCantonese)
{
    TranslationResult r;
    if (d == Direction::EnglishToCantonese) {
        r.request.text = QStringLiteral("Long time no see! How have you been?");
        r.translation = QStringLiteral("好耐冇見！你最近點呀？");
        r.jyutping = QStringLiteral("hou2 noi6 mou5 gin3! nei5 zeoi3 gan6 dim2 aa3?");
        r.alternatives = {
            {QStringLiteral("咦，好耐冇見喎！近排點呀？"), QStringLiteral("ji2, hou2 noi6 mou5 gin3 wo3! gan6 paai2 dim2 aa3?"),
             QStringLiteral("Casual and warm - 近排 (lately) is very colloquial.")},
            {QStringLiteral("好耐冇見，你最近好嗎？"), QStringLiteral("hou2 noi6 mou5 gin3, nei5 zeoi3 gan6 hou2 maa3?"),
             QStringLiteral("A little more polite; fine with older relatives.")},
        };
        r.notes = {QStringLiteral("點呀 (dim2 aa3) is the everyday way to ask \"how are you\"."),
                   QStringLiteral("The particle 呀 softens the question.")};
    } else {
        r.request.text = QStringLiteral("你食咗飯未呀？");
        r.translation = QStringLiteral("Have you eaten yet?");
        r.jyutping = QStringLiteral("nei5 sik6 zo2 faan6 mei6 aa3?");
        r.literal = QStringLiteral("You eat-already rice not-yet?");
        r.notes = {QStringLiteral("A common friendly greeting, similar to \"How are you?\".")};
    }
    r.request.direction = d;
    r.providerId = QStringLiteral("claude");
    r.model = QStringLiteral("claude-opus-5-5");
    r.timestamp = QDateTime::currentDateTimeUtc();
    return r;
}

// One isolated app instance: settings in a temp dir, fake provider, window.
struct Env
{
    QTemporaryDir dir;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<TranslationService> service;
    std::unique_ptr<SpeechService> speech;
    std::unique_ptr<MainWindow> window;
    FakeProvider *fake = nullptr;

    explicit Env(bool configured = true)
    {
        const QString ini = dir.filePath(QStringLiteral("settings.ini"));
        UiPrefs::useIniFile(ini);
        UiPrefs::instance()->setReduceMotion(true);  // deterministic: animations end immediately
        settings = std::make_unique<AppSettings>(ini);
        settings->setTheme(QStringLiteral("light"));
        if (configured)
            settings->setClaudeApiKey(QStringLiteral("test-key"));
        Theme::apply(QStringLiteral("light"));
        service = std::make_unique<TranslationService>(settings.get());
        fake = new FakeProvider;
        service->setProvider(fake);
        speech = std::make_unique<SpeechService>(settings.get(), service->networkManager());
        window = std::make_unique<MainWindow>(settings.get(), service.get(), speech.get());
        window->resize(1180, 720);
    }

    ~Env()
    {
        window.reset();
        speech.reset();
        service.reset();
        settings.reset();
    }

    bool show()
    {
        window->show();
        return QTest::qWaitForWindowExposed(window.get());
    }
};

QString screenshotDir() { return qEnvironmentVariable("SCT_SCREENSHOT_DIR"); }

void snap(QWidget *w, const QString &name)
{
    const QString dir = screenshotDir();
    if (dir.isEmpty())
        return;
    QDir().mkpath(dir);
    QTest::qWait(50);
    QVERIFY(w->grab().save(QDir(dir).filePath(QStringLiteral("ui-%1.png").arg(name))));
}

} // namespace

class TestMainWindow : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void startsEmptyAndUnconfigured();
    void directionAndTone();
    void translateFlowShowsResultAndHistory();
    void resultViewShowsJyutpingAlternativesNotes();
    void displayOptionsHideSections();
    void errorStatesOfferActions();
    void escapeCancelsTranslation();
    void historyRestoreDoesNotCallApi();
    void starTogglesHistoryEntry();
    void settingsDialogAppliesChanges();
    void settingsTestConnectionListsModels();
    void cantoneseVoiceHint();
    void changingDirectionOrToneWhileTranslatingRedoesIt();
    void enterActivatesFocusedButton();
    void copyShortcutCopiesOnlyTheShownResult();
    void firstRunWindowFitsOnScreen();
    void shortWindowKeepsCardsApart();
    void otherShortcutsAreWired();
    void animationsRunWithoutCrashing();
    void closingMidAnimationIsSafe();
    void screenshots();
};

void TestMainWindow::initTestCase()
{
    registerMetaTypes();
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QApplication::setFont(Theme::uiFont(10));
}

void TestMainWindow::startsEmptyAndUnconfigured()
{
    Env env(false);
    QVERIFY(env.show());
    QCOMPARE(env.window->windowTitle(), QStringLiteral("Smart Cantonese Translator"));
    QCOMPARE(env.window->resultView()->page(), ResultView::Page::Empty);
    QVERIFY(env.window->welcomeBanner()->isVisible());
    QVERIFY(env.window->providerChipText().contains(QStringLiteral("Add API key")));

    // Translating without a key shows the guidance error with "Open Settings".
    env.window->setInputText(QStringLiteral("Hello"));
    env.window->translateNow();
    QTRY_COMPARE(env.window->resultView()->page(), ResultView::Page::Error);
    const auto buttons = env.window->resultView()->errorBanner()->buttons();
    QCOMPARE(buttons.size(), 1);
    QCOMPARE(buttons.first()->text(), QStringLiteral("Open Settings"));
    QCOMPARE(env.fake->translateCalls, 0);

    // Adding a key hides the welcome banner and updates the chip.
    env.settings->setClaudeApiKey(QStringLiteral("k"));
    QVERIFY(env.window->welcomeBanner()->isHidden());
    QCOMPARE(env.window->providerChipText(), QStringLiteral("claude-opus-5-5"));
}

void TestMainWindow::directionAndTone()
{
    Env env;
    QVERIFY(env.show());
    QVERIFY(env.window->direction() == Direction::EnglishToCantonese);
    QVERIFY(env.window->inputPane()->editor()->placeholderText().contains(QStringLiteral("English")));

    QTest::mouseClick(env.window->directionPill(), Qt::LeftButton);
    QVERIFY(env.window->direction() == Direction::CantoneseToEnglish);
    QVERIFY(env.settings->direction() == Direction::CantoneseToEnglish);
    QVERIFY(env.window->inputPane()->editor()->placeholderText().contains(QStringLiteral("廣東話")));

    // Ctrl+Shift+S swaps back.
    QTest::keyClick(env.window->inputPane()->editor(), Qt::Key_S, Qt::ControlModifier | Qt::ShiftModifier);
    QVERIFY(env.window->direction() == Direction::EnglishToCantonese);

    // Clicking "Polite" in the tone control.
    ui::SegmentedControl *tone = env.window->toneControl();
    QTest::mouseClick(tone, Qt::LeftButton, Qt::NoModifier, tone->segmentRect(2).center());
    QVERIFY(env.window->tone() == Tone::Polite);
    QVERIFY(env.settings->tone() == Tone::Polite);
    // Keyboard: Left arrow moves to Neutral.
    tone->setFocus();
    QTest::keyClick(tone, Qt::Key_Left);
    QVERIFY(env.settings->tone() == Tone::Neutral);

    // Swapping with a result moves the translation into the input and translates back.
    env.window->showResult(sampleResult());
    QTest::mouseClick(env.window->directionPill(), Qt::LeftButton);
    QCOMPARE(env.window->inputPane()->text(), QStringLiteral("好耐冇見！你最近點呀？"));
    QCOMPARE(env.fake->translateCalls, 1);
    QVERIFY(env.fake->last.direction == Direction::CantoneseToEnglish);
}

void TestMainWindow::translateFlowShowsResultAndHistory()
{
    Env env;
    QVERIFY(env.show());
    QTest::keyClicks(env.window->inputPane()->editor(), QStringLiteral("Long time no see!"));
    QTest::keyClick(env.window->inputPane()->editor(), Qt::Key_Return, Qt::ControlModifier);
    QCOMPARE(env.fake->translateCalls, 1);
    QCOMPARE(env.fake->last.text, QStringLiteral("Long time no see!"));
    QVERIFY(env.fake->last.tone == Tone::Neutral);
    QCOMPARE(env.window->resultView()->page(), ResultView::Page::Loading);
    QVERIFY(env.window->inputPane()->isBusy());

    TranslationResult r = sampleResult();
    r.request = env.fake->last;
    env.fake->complete(r);
    QCOMPARE(env.window->resultView()->page(), ResultView::Page::Result);
    QVERIFY(!env.window->inputPane()->isBusy());
    QCOMPARE(env.window->resultView()->translationText(), QStringLiteral("好耐冇見！你最近點呀？"));
    QCOMPARE(env.service->history()->entries().size(), 1);

    // The history panel lists it once opened.
    env.window->setHistoryVisible(true);
    QCOMPARE(env.window->historyPanel()->visibleCount(), 1);

    // Same request again is served from cache (no provider call).
    env.window->translateNow();
    QTRY_VERIFY(env.window->resultView()->result().fromCache);
    QCOMPARE(env.fake->translateCalls, 1);
}

void TestMainWindow::resultViewShowsJyutpingAlternativesNotes()
{
    Env env;
    QVERIFY(env.show());
    ResultView *view = env.window->resultView();
    env.window->showResult(sampleResult());
    QCOMPARE(view->page(), ResultView::Page::Result);
    QCOMPARE(view->translationText(), QStringLiteral("好耐冇見！你最近點呀？"));
    QCOMPARE(view->jyutpingText(), QStringLiteral("hou2 noi6 mou5 gin3! nei5 zeoi3 gan6 dim2 aa3?"));
    QVERIFY(view->isJyutpingShown());
    QCOMPARE(view->alternativeCount(), 2);
    QCOMPARE(view->noteCount(), 2);
    QVERIFY(view->literalText().isEmpty());
    QCOMPARE(view->alternativesSection()->title(), QStringLiteral("2 other ways to say it"));
    QVERIFY(!view->alternativesSection()->isExpanded());  // collapsed by default
    view->alternativesSection()->setExpanded(true);
    QVERIFY(view->alternativesSection()->isExpanded());

    // Cantonese -> English shows the literal meaning.
    env.window->showResult(sampleResult(Direction::CantoneseToEnglish));
    QCOMPARE(view->translationText(), QStringLiteral("Have you eaten yet?"));
    QVERIFY(view->literalText().contains(QStringLiteral("You eat-already rice not-yet?")));
    QCOMPARE(view->alternativeCount(), 0);
    QVERIFY(view->alternativesSection()->isHidden());

    // Copy puts the translation on the clipboard.
    view->copyTranslation();
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("Have you eaten yet?"));
}

void TestMainWindow::displayOptionsHideSections()
{
    Env env;
    QVERIFY(env.show());
    env.window->showResult(sampleResult());
    env.settings->beginBatch();
    env.settings->setShowJyutping(false);
    env.settings->setShowNotes(false);
    env.settings->endBatch();
    QVERIFY(!env.window->resultView()->isJyutpingShown());
    QVERIFY(env.window->resultView()->notesSection()->isHidden());
    QVERIFY(!env.window->resultView()->alternativesSection()->isHidden());
}

void TestMainWindow::errorStatesOfferActions()
{
    Env env;
    QVERIFY(env.show());
    TranslationError auth;
    auth.kind = ErrorKind::Auth;
    auth.message = QStringLiteral("Claude rejected the API key.");
    env.window->showError(auth);
    ResultView *view = env.window->resultView();
    QCOMPARE(view->page(), ResultView::Page::Error);
    QCOMPARE(view->errorBanner()->text(), QStringLiteral("Claude rejected the API key."));
    QCOMPARE(view->errorBanner()->buttons().first()->text(), QStringLiteral("Open Settings"));

    // Rate limit: Retry re-sends the last request.
    env.window->setInputText(QStringLiteral("Hello"));
    env.window->translateNow();
    QCOMPARE(env.fake->translateCalls, 1);
    env.fake->fail(ErrorKind::RateLimited, QStringLiteral("Too many requests. Wait a moment and try again."));
    QCOMPARE(view->page(), ResultView::Page::Error);
    QVERIFY(view->errorBanner()->details().contains(QStringLiteral("429")));
    ui::Button *retry = view->errorBanner()->buttons().first();
    QCOMPARE(retry->text(), QStringLiteral("Retry"));
    retry->click();
    QCOMPARE(env.fake->translateCalls, 2);
    QCOMPARE(env.fake->last.text, QStringLiteral("Hello"));
}

void TestMainWindow::escapeCancelsTranslation()
{
    Env env;
    QVERIFY(env.show());
    env.window->showResult(sampleResult());
    env.window->setInputText(QStringLiteral("Something else"));
    env.window->translateNow();
    QCOMPARE(env.window->resultView()->page(), ResultView::Page::Loading);
    QTest::keyClick(env.window->inputPane()->editor(), Qt::Key_Escape);
    QVERIFY(!env.service->isBusy());
    QVERIFY(!env.window->inputPane()->isBusy());
    // The previous result comes back.
    QCOMPARE(env.window->resultView()->page(), ResultView::Page::Result);
    QCOMPARE(env.window->resultView()->translationText(), QStringLiteral("好耐冇見！你最近點呀？"));
}

void TestMainWindow::historyRestoreDoesNotCallApi()
{
    Env env;
    QVERIFY(env.show());
    TranslationResult r = sampleResult(Direction::CantoneseToEnglish);
    r.request.tone = Tone::Casual;
    env.service->history()->add(r);
    env.window->setHistoryVisible(true);
    HistoryPanel *panel = env.window->historyPanel();
    QCOMPARE(panel->visibleCount(), 1);
    panel->activateRow(0);
    QCOMPARE(env.fake->translateCalls, 0);
    QVERIFY(env.window->direction() == Direction::CantoneseToEnglish);
    QVERIFY(env.window->tone() == Tone::Casual);
    QCOMPARE(env.window->inputPane()->text(), QStringLiteral("你食咗飯未呀？"));
    QCOMPARE(env.window->resultView()->translationText(), QStringLiteral("Have you eaten yet?"));

    // Search filters the list.
    panel->searchBox()->setText(QStringLiteral("zzz"));
    QCOMPARE(panel->visibleCount(), 0);
    panel->searchBox()->setText(QStringLiteral("eaten"));
    QCOMPARE(panel->visibleCount(), 1);
    panel->searchBox()->clear();

    // Ctrl+H hides the panel again.
    QTest::keyClick(env.window->inputPane()->editor(), Qt::Key_H, Qt::ControlModifier);
    QVERIFY(!env.window->isHistoryVisible());
}

void TestMainWindow::starTogglesHistoryEntry()
{
    Env env;
    QVERIFY(env.show());
    env.window->setInputText(QStringLiteral("Long time no see!"));
    env.window->translateNow();
    TranslationResult r = sampleResult();
    r.request = env.fake->last;
    env.fake->complete(r);
    QCOMPARE(env.service->history()->entries().size(), 1);
    QVERIFY(!env.window->resultView()->isStarred());

    auto *star = env.window->resultView()->findChild<ui::IconButton *>(QStringLiteral("starButton"));
    QVERIFY(star);
    star->click();
    QVERIFY(env.service->history()->entries().first().starred);
    QVERIFY(env.window->resultView()->isStarred());

    // Starred-only filter in the history panel.
    env.window->setHistoryVisible(true);
    env.window->historyPanel()->setStarredOnly(true);
    QCOMPARE(env.window->historyPanel()->visibleCount(), 1);
    star->click();
    QVERIFY(!env.service->history()->entries().first().starred);
    QCOMPARE(env.window->historyPanel()->visibleCount(), 0);
}

void TestMainWindow::settingsDialogAppliesChanges()
{
    Env env;
    QVERIFY(env.show());
    SettingsDialog *dlg = env.window->openSettings(SettingsDialog::Tab::Appearance);
    QVERIFY(dlg);
    QVERIFY(QTest::qWaitForWindowExposed(dlg));
    QCOMPARE(dlg->currentTab(), SettingsDialog::Tab::Appearance);

    auto *theme = dlg->findChild<ui::SegmentedControl *>(QStringLiteral("themeControl"));
    QVERIFY(theme);
    QTest::mouseClick(theme, Qt::LeftButton, Qt::NoModifier, theme->segmentRect(2).center());
    QVERIFY(dlg->isDirty());

    auto *jyutping = dlg->findChild<ui::ToggleSwitch *>(QStringLiteral("showJyutping"));
    QVERIFY(jyutping);
    QVERIFY(jyutping->isChecked());
    jyutping->click();

    auto *key = dlg->findChild<QLineEdit *>(QStringLiteral("openaiKey"));
    QVERIFY(key);
    key->setText(QStringLiteral("sk-test"));
    QTest::keyClick(key, Qt::Key_A);  // textEdited -> dirty
    key->setText(QStringLiteral("sk-test"));

    auto *reduceMotion = dlg->findChild<ui::ToggleSwitch *>(QStringLiteral("reduceMotion"));
    QVERIFY(reduceMotion);
    QVERIFY(reduceMotion->isChecked());  // Env turns it on
    reduceMotion->click();

    auto *provider = dlg->findChild<ui::SegmentedControl *>(QStringLiteral("providerControl"));
    QVERIFY(provider);
    QTest::mouseClick(provider, Qt::LeftButton, Qt::NoModifier, provider->segmentRect(1).center());

    auto *ok = dlg->findChild<ui::Button *>(QStringLiteral("okButton"));
    QVERIFY(ok);
    QPointer<SettingsDialog> guard(dlg);
    ok->click();
    QTRY_VERIFY(guard.isNull());  // closes and deletes itself

    QCOMPARE(env.settings->theme(), QStringLiteral("dark"));
    QVERIFY(Theme::isDark());
    QVERIFY(!env.settings->showJyutping());
    QCOMPARE(env.settings->openAiApiKey(), QStringLiteral("sk-test"));
    QCOMPARE(env.settings->aiProvider(), QStringLiteral("openai"));
    QVERIFY(!UiPrefs::instance()->reduceMotion());
    UiPrefs::instance()->setReduceMotion(true);
    QVERIFY(env.window->providerChip()->toolTip().contains(QStringLiteral("OpenAI")));
    Theme::apply(QStringLiteral("light"));

    // Every page can be shown; Cancel discards.
    dlg = env.window->openSettings(SettingsDialog::Tab::Speech);
    QCOMPARE(dlg->currentTab(), SettingsDialog::Tab::Speech);
    QVERIFY(dlg->findChild<QWidget *>(QStringLiteral("windowsVoiceStatus")));
    dlg->setCurrentTab(SettingsDialog::Tab::Translation);
    dlg->setCurrentTab(SettingsDialog::Tab::AI);
    guard = dlg;
    dlg->reject();
    QTRY_VERIFY(guard.isNull());
}

void TestMainWindow::settingsTestConnectionListsModels()
{
    Env env;
    QVERIFY(env.show());
    SettingsDialog *dlg = env.window->openSettings(SettingsDialog::Tab::AI);
    auto *test = dlg->findChild<ui::Button *>(QStringLiteral("claudeTest"));
    auto *status = dlg->findChild<QLabel *>(QStringLiteral("claudeStatus"));
    QVERIFY(test && status);
    test->click();
    QTRY_VERIFY(status->text().contains(QStringLiteral("Connected")));
    auto *model = dlg->findChild<QComboBox *>(QStringLiteral("claudeModel"));
    QVERIFY(model);
    QCOMPARE(model->count(), 2);
    QCOMPARE(env.fake->listCalls, 1);

    // A second click while "Testing…" does not send another request.
    test->click();
    test->click();
    QCOMPARE(env.fake->listCalls, 2);
    QTRY_VERIFY(!test->isBusy());
    dlg->reject();
}

void TestMainWindow::cantoneseVoiceHint()
{
    Env env;
    QVERIFY(env.show());
    if (env.speech->canSpeak(Language::Cantonese))
        QSKIP("A Cantonese voice is installed on this machine");
    env.window->showResult(sampleResult());
    env.window->resultView()->speakButton()->click();
    QVERIFY(env.window->voiceHintBanner()->isVisible());
    QVERIFY(!env.window->voiceHintBanner()->text().isEmpty());
}

// A translation still running when the direction or tone changes is redone
// with the new setting: its late answer must not flip the direction back.
void TestMainWindow::changingDirectionOrToneWhileTranslatingRedoesIt()
{
    Env env;
    QVERIFY(env.show());
    ResultView *view = env.window->resultView();

    // Chinese typed with English -> Cantonese selected; "Switch languages" is
    // clicked while the first request is still running.
    env.window->setInputText(QStringLiteral("你食咗飯未呀？"));
    env.window->translateNow();
    QCOMPARE(env.fake->translateCalls, 1);
    QVERIFY(env.fake->last.direction == Direction::EnglishToCantonese);
    auto *hint = env.window->inputPane()->findChild<ui::Button *>(QStringLiteral("directionHintButton"));
    QVERIFY(hint);
    hint->click();
    QVERIFY(env.window->direction() == Direction::CantoneseToEnglish);
    QCOMPARE(env.fake->translateCalls, 2);
    QVERIFY(env.fake->last.direction == Direction::CantoneseToEnglish);
    QCOMPARE(view->page(), ResultView::Page::Loading);
    TranslationResult r = sampleResult(Direction::CantoneseToEnglish);
    r.request = env.fake->last;
    env.fake->complete(r);
    QVERIFY(env.window->direction() == Direction::CantoneseToEnglish);
    QCOMPARE(view->translationText(), QStringLiteral("Have you eaten yet?"));

    // Swap (Ctrl+Shift+S) while translating: redone in the new direction.
    env.window->setInputText(QStringLiteral("Hello there"));
    env.window->translateNow();
    QCOMPARE(env.fake->translateCalls, 3);
    QTest::keyClick(env.window->inputPane()->editor(), Qt::Key_S, Qt::ControlModifier | Qt::ShiftModifier);
    QVERIFY(env.window->direction() == Direction::EnglishToCantonese);
    QCOMPARE(env.fake->translateCalls, 4);
    QVERIFY(env.fake->last.direction == Direction::EnglishToCantonese);
    QCOMPARE(env.fake->last.text, QStringLiteral("Hello there"));
    QCOMPARE(env.window->inputPane()->text(), QStringLiteral("Hello there"));
    QCOMPARE(view->page(), ResultView::Page::Loading);

    // Tone change while translating: redone in the new tone.
    ui::SegmentedControl *tone = env.window->toneControl();
    QTest::mouseClick(tone, Qt::LeftButton, Qt::NoModifier, tone->segmentRect(2).center());
    QCOMPARE(env.fake->translateCalls, 5);
    QVERIFY(env.fake->last.tone == Tone::Polite);
    r = sampleResult();
    r.request = env.fake->last;
    env.fake->complete(r);
    QVERIFY(env.window->direction() == Direction::EnglishToCantonese);
    QVERIFY(view->result().request.tone == Tone::Polite);

    // Nothing to redo when the text box was emptied: just cancelled.
    env.window->setInputText(QStringLiteral("Good night"));
    env.window->translateNow();
    QCOMPARE(env.fake->translateCalls, 6);
    env.window->setInputText(QString());
    env.window->setDirection(Direction::CantoneseToEnglish);
    QCOMPARE(env.fake->translateCalls, 6);
    QVERIFY(!env.service->isBusy());
    QVERIFY(view->page() != ResultView::Page::Loading);
}

// Enter on a focused button clicks it, like a QPushButton, instead of
// running the dialog's default action (Save).
void TestMainWindow::enterActivatesFocusedButton()
{
    Env env;
    QVERIFY(env.show());
    SettingsDialog *dlg = env.window->openSettings(SettingsDialog::Tab::Appearance);
    QVERIFY(QTest::qWaitForWindowExposed(dlg));
    auto *theme = dlg->findChild<ui::SegmentedControl *>(QStringLiteral("themeControl"));
    QVERIFY(theme);
    QTest::mouseClick(theme, Qt::LeftButton, Qt::NoModifier, theme->segmentRect(2).center());
    QVERIFY(dlg->isDirty());
    auto *cancel = dlg->findChild<ui::Button *>(QStringLiteral("cancelButton"));
    QVERIFY(cancel);
    cancel->setFocus(Qt::TabFocusReason);
    QPointer<SettingsDialog> guard(dlg);
    QTest::keyClick(cancel, Qt::Key_Return);
    QTRY_VERIFY(guard.isNull());
    QCOMPARE(env.settings->theme(), QStringLiteral("light"));  // cancelled, not saved

    // On a toggle switch Enter still saves the dialog (like a check box).
    dlg = env.window->openSettings(SettingsDialog::Tab::Translation);
    QVERIFY(QTest::qWaitForWindowExposed(dlg));
    auto *jyutping = dlg->findChild<ui::ToggleSwitch *>(QStringLiteral("showJyutping"));
    QVERIFY(jyutping);
    jyutping->click();
    jyutping->setFocus(Qt::TabFocusReason);
    guard = dlg;
    QTest::keyClick(jyutping, Qt::Key_Enter, Qt::KeypadModifier);
    QTRY_VERIFY(guard.isNull());
    QVERIFY(!env.settings->showJyutping());

    // Main window: Enter on the focused Translate button translates.
    env.window->setInputText(QStringLiteral("Hello"));
    ui::Button *translate = env.window->inputPane()->translateButton();
    QVERIFY(translate->toolTip().contains(QStringLiteral("Enter")));
    translate->setFocus(Qt::TabFocusReason);
    QTest::keyClick(translate, Qt::Key_Return);
    QCOMPARE(env.fake->translateCalls, 1);
}

void TestMainWindow::copyShortcutCopiesOnlyTheShownResult()
{
    Env env;
    QVERIFY(env.show());
    QPlainTextEdit *editor = env.window->inputPane()->editor();
    QApplication::clipboard()->setText(QStringLiteral("unchanged"));
    env.window->showResult(sampleResult());
    TranslationError e;
    e.kind = ErrorKind::Server;
    env.window->showError(e);  // the previous result is no longer on screen
    QTest::keyClick(editor, Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier);
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("unchanged"));
    QVERIFY(ui::Toast::current(env.window.get()));
    QCOMPARE(ui::Toast::current(env.window.get())->text(), QStringLiteral("Nothing to copy yet"));

    env.window->showResult(sampleResult());
    QTest::keyClick(editor, Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier);
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("好耐冇見！你最近點呀？"));
}

// Without a saved geometry the window must fit the screen (small laptops,
// 150 % scaling), with the Translate button visible.
void TestMainWindow::firstRunWindowFitsOnScreen()
{
    Env env;
    MainWindow fresh(env.settings.get(), env.service.get(), env.speech.get());
    const QRect area = fresh.screen()->availableGeometry();
    QVERIFY(area.isValid());
    QVERIFY2(fresh.width() <= qMax(area.width(), fresh.minimumWidth()),
             qPrintable(QStringLiteral("%1 > %2").arg(fresh.width()).arg(area.width())));
    QVERIFY2(fresh.height() <= qMax(area.height(), fresh.minimumHeight()),
             qPrintable(QStringLiteral("%1 > %2").arg(fresh.height()).arg(area.height())));
    if (area.width() >= 1400 && area.height() >= 900)
        QCOMPARE(fresh.size(), QSize(1180, 720));  // the designed size where it fits
}

// Narrow and short window, history open (cards stacked) and the "no
// Cantonese voice" banner shown: nothing may be squeezed into anything else.
void TestMainWindow::shortWindowKeepsCardsApart()
{
    Env env;
    env.window->resize(800, 560);
    QVERIFY(env.show());
    env.window->setHistoryVisible(true, false);
    env.window->showResult(sampleResult());
    emit env.window->speechController()->voiceUnavailable(Language::Cantonese);
    QVERIFY(env.window->voiceHintBanner()->isVisible());
    QTest::qWait(50);  // layouts settle

    QWidget *central = env.window->centralWidget();
    auto rectIn = [central](QWidget *w) { return QRect(w->mapTo(central, QPoint(0, 0)), w->size()); };
    const QRect banner = rectIn(env.window->voiceHintBanner());
    const QRect input = rectIn(env.window->inputPane());
    const QRect result = rectIn(env.window->resultView());
    QVERIFY2(input.bottom() < result.top(), "the stacked cards overlap");
    QVERIFY2(banner.bottom() < input.top(), "the banner overlaps the cards");
    QVERIFY(result.bottom() <= central->height());
    QVERIFY(input.height() >= env.window->inputPane()->minimumHeight());
}

void TestMainWindow::otherShortcutsAreWired()
{
    Env env;
    QVERIFY(env.show());
    env.window->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(env.window.get()));
    QPlainTextEdit *editor = env.window->inputPane()->editor();

    // Ctrl+R without a result explains what to do.
    QTest::keyClick(editor, Qt::Key_R, Qt::ControlModifier);
    QVERIFY(ui::Toast::current(env.window.get()));
    QVERIFY(ui::Toast::current(env.window.get())->text().startsWith(QStringLiteral("Translate something first")));

    // Ctrl+F opens history with the search box focused; Esc there closes it.
    QTest::keyClick(editor, Qt::Key_F, Qt::ControlModifier);
    QVERIFY(env.window->isHistoryVisible());
    QLineEdit *search = env.window->historyPanel()->searchBox();
    QTRY_VERIFY(search->hasFocus());
    QTest::keyClick(search, Qt::Key_Escape);
    QVERIFY(!env.window->isHistoryVisible());
    QTRY_VERIFY(editor->hasFocus());

    // Ctrl+, opens Settings on the AI page.
    QTest::keyClick(editor, Qt::Key_Comma, Qt::ControlModifier);
    auto *dlg = env.window->findChild<SettingsDialog *>();
    QVERIFY(dlg);
    QVERIFY(dlg->isVisible());
    QCOMPARE(dlg->currentTab(), SettingsDialog::Tab::AI);
    QPointer<SettingsDialog> guard(dlg);
    dlg->reject();
    QTRY_VERIFY(guard.isNull());
}

// Same flows with animations on: exercises the motion code paths (deferred
// deletes, overlapping animations) for crashes.
void TestMainWindow::animationsRunWithoutCrashing()
{
    Env env;
    UiPrefs::instance()->setReduceMotion(false);
    QVERIFY(env.show());
    QTest::mouseClick(env.window->directionPill(), Qt::LeftButton);
    QTest::mouseClick(env.window->directionPill(), Qt::LeftButton);
    env.window->setInputText(QStringLiteral("Long time no see!"));
    env.window->translateNow();
    QTest::qWait(120);
    TranslationResult r = sampleResult();
    r.request = env.fake->last;
    env.fake->complete(r);
    env.window->resultView()->alternativesSection()->setExpanded(true);  // during the reveal
    env.window->setHistoryVisible(true);
    QTest::qWait(150);
    env.window->setHistoryVisible(false);
    env.window->setHistoryVisible(true);
    env.service->history()->clear();
    env.service->history()->add(sampleResult(Direction::CantoneseToEnglish));
    env.service->history()->add(sampleResult());
    QTest::qWait(100);
    env.service->history()->remove(env.service->history()->entries().first().id);
    TranslationError e;
    e.kind = ErrorKind::Server;
    env.window->showError(e);
    env.window->showResult(sampleResult());
    env.window->showStatus(QStringLiteral("Copied"), QStringLiteral("check"));
    env.settings->setTheme(QStringLiteral("dark"));
    QTest::qWait(500);
    QCOMPARE(env.window->resultView()->page(), ResultView::Page::Result);
    QCOMPARE(env.window->historyPanel()->visibleCount(), 1);
    env.settings->setTheme(QStringLiteral("light"));
    QTest::qWait(400);
    UiPrefs::instance()->setReduceMotion(true);
}

// Everything torn down while animations, toasts, overlays and a request are
// still in flight (run under ASan to catch lifetime bugs).
void TestMainWindow::closingMidAnimationIsSafe()
{
    {
        Env env;
        UiPrefs::instance()->setReduceMotion(false);
        QVERIFY(env.show());
        env.service->history()->add(sampleResult(Direction::CantoneseToEnglish));
        env.service->history()->add(sampleResult());
        env.window->setHistoryVisible(true);
        env.window->setInputText(QStringLiteral("Long time no see!"));
        for (int i = 0; i < 3; ++i)
            env.window->translateNow();  // rapid re-translate
        TranslationResult r = sampleResult();
        r.request = env.fake->last;
        env.fake->complete(r);
        env.window->resultView()->alternativesSection()->setExpanded(true);
        env.window->showStatus(QStringLiteral("Copied"), QStringLiteral("check"));
        env.service->history()->remove(env.service->history()->entries().last().id);  // row collapses
        SettingsDialog *dlg = env.window->openSettings(SettingsDialog::Tab::Appearance);
        QVERIFY(QTest::qWaitForWindowExposed(dlg));
        env.settings->setTheme(QStringLiteral("dark"));  // cross-fades the window and the dialog
        dlg->reject();
        env.window->setInputText(QStringLiteral("Another one"));
        env.window->translateNow();
        TranslationError e;
        e.kind = ErrorKind::Network;
        env.fake->fail(e.kind, QStringLiteral("offline"));  // error banner slides and shakes
        QTest::mouseClick(env.window->directionPill(), Qt::LeftButton);
        QTest::qWait(60);
        env.window->close();  // mid-animation, translation running
    }
    Theme::apply(QStringLiteral("light"));
    UiPrefs::instance()->setReduceMotion(true);
}

void TestMainWindow::screenshots()
{
    if (screenshotDir().isEmpty())
        QSKIP("Set SCT_SCREENSHOT_DIR to save screenshots");
    for (const QString &theme : QStringList{QStringLiteral("light"), QStringLiteral("dark")}) {
        Env env(false);
        env.settings->setTheme(theme);
        Theme::apply(theme);
        QVERIFY(env.show());
        snap(env.window.get(), QStringLiteral("empty-%1").arg(theme));

        env.settings->setClaudeApiKey(QStringLiteral("k"));
        env.window->setInputText(QStringLiteral("Long time no see! How have you been?"));
        env.window->translateNow();
        snap(env.window.get(), QStringLiteral("loading-%1").arg(theme));

        TranslationResult r = sampleResult();
        r.request = env.fake->last;
        env.fake->complete(r);
        env.window->resultView()->alternativesSection()->setExpanded(true, false);
        env.window->resultView()->notesSection()->setExpanded(true, false);
        snap(env.window.get(), QStringLiteral("result-%1").arg(theme));

        env.service->history()->add(sampleResult(Direction::CantoneseToEnglish));
        env.window->setHistoryVisible(true, false);
        env.service->history()->setStarred(env.service->history()->entries().last().id, true);
        snap(env.window.get(), QStringLiteral("history-%1").arg(theme));
        env.window->setHistoryVisible(false, false);

        env.window->showResult(sampleResult(Direction::CantoneseToEnglish));
        env.window->resultView()->notesSection()->setExpanded(true, false);
        snap(env.window.get(), QStringLiteral("yue2en-%1").arg(theme));

        env.window->setInputText(QStringLiteral("Hello"));
        env.window->translateNow();
        env.fake->fail(ErrorKind::RateLimited, QStringLiteral("Claude is receiving too many requests. Wait a moment, then try again."));
        env.window->resultView()->errorBanner()->setDetailsVisible(true);
        snap(env.window.get(), QStringLiteral("error-%1").arg(theme));

        env.window->resize(800, 760);
        QTest::qWait(50);
        snap(env.window.get(), QStringLiteral("narrow-%1").arg(theme));
        env.window->resize(1180, 720);

        SettingsDialog *dlg = env.window->openSettings(SettingsDialog::Tab::AI);
        QVERIFY(QTest::qWaitForWindowExposed(dlg));
        snap(dlg, QStringLiteral("settings-ai-%1").arg(theme));
        dlg->setCurrentTab(SettingsDialog::Tab::Translation);
        snap(dlg, QStringLiteral("settings-translation-%1").arg(theme));
        dlg->setCurrentTab(SettingsDialog::Tab::Speech);
        snap(dlg, QStringLiteral("settings-speech-%1").arg(theme));
        dlg->setCurrentTab(SettingsDialog::Tab::Appearance);
        snap(dlg, QStringLiteral("settings-appearance-%1").arg(theme));
        dlg->reject();
    }
    Theme::apply(QStringLiteral("light"));

    // Mid-animation frames (motion on).
    Env env;
    QVERIFY(env.show());
    UiPrefs::instance()->setReduceMotion(false);
    env.window->showResult(sampleResult());
    QTest::qWait(400);
    env.window->resultView()->speakButton()->setSpeechState(ui::SpeakButton::State::Speaking);
    env.window->showStatus(QStringLiteral("Copied translation"), QStringLiteral("check"));
    QTest::qWait(300);
    snap(env.window.get(), QStringLiteral("motion-speaking-toast"));
    env.window->resultView()->speakButton()->setSpeechState(ui::SpeakButton::State::Idle);
    ui::SegmentedControl *tone = env.window->toneControl();
    QTest::mouseClick(tone, Qt::LeftButton, Qt::NoModifier, tone->segmentRect(0).center());
    env.window->setHistoryVisible(true);
    QTest::mouseClick(env.window->directionPill(), Qt::LeftButton);
    QTest::qWait(110);
    snap(env.window.get(), QStringLiteral("motion-midswap"));
    QTest::qWait(600);
    UiPrefs::instance()->setReduceMotion(true);
}

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
