#include "ui/MainWindow.h"

#include "core/AppSettings.h"
#include "core/HistoryStore.h"
#include "core/TranslationService.h"
#include "core/Version.h"
#include "tts/SpeechService.h"
#include "ui/AboutDialog.h"
#include "ui/HistoryPanel.h"
#include "ui/InputPane.h"
#include "ui/ResultView.h"
#include "ui/SpeechController.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include <QAction>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace sct {

using ui::IconTone;

namespace {

const QString kClaudeKeysUrl = QStringLiteral("https://console.anthropic.com/settings/keys");
const QString kOpenAiKeysUrl = QStringLiteral("https://platform.openai.com/api-keys");

QString shortProviderName(const QString &id)
{
    if (id == QLatin1String("claude"))
        return QStringLiteral("Claude");
    if (id == QLatin1String("openai"))
        return QStringLiteral("OpenAI");
    return id;
}

// Shortcut keys, shared by QShortcuts and tooltips.
QKeySequence keyTranslate() { return QKeySequence(Qt::CTRL | Qt::Key_Return); }
QKeySequence keySwap() { return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S); }
QKeySequence keySpeak() { return QKeySequence(Qt::CTRL | Qt::Key_R); }
QKeySequence keyHistory() { return QKeySequence(Qt::CTRL | Qt::Key_H); }
QKeySequence keySettings() { return QKeySequence(Qt::CTRL | Qt::Key_Comma); }
QKeySequence keyCopy() { return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C); }

} // namespace

MainWindow::MainWindow(AppSettings *settings, TranslationService *translation, SpeechService *speech,
                       QWidget *parent)
    : QMainWindow(parent)
    , m_settings(settings)
    , m_translation(translation)
    , m_speech(speech)
{
    setObjectName(QStringLiteral("mainWindow"));
    setWindowTitle(QStringLiteral(SCT_APP_NAME));
    setWindowIcon(ui::appIcon());
    setDockOptions(QMainWindow::AnimatedDocks);

    m_speechController = new SpeechController(m_speech, this);

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("centralArea"));
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(16, 10, 8, 4);
    layout->setSpacing(10);
    layout->addWidget(buildHeader());

    m_welcome = new ui::Banner(ui::Banner::Kind::Info, central);
    m_welcome->setObjectName(QStringLiteral("welcomeBanner"));
    m_welcome->setClosable(true);
    QPushButton *setup = m_welcome->addButton(tr("Open Settings"), true);
    connect(setup, &QPushButton::clicked, this, [this] { openSettings(SettingsDialog::Tab::AI); });
    connect(m_welcome, &ui::Banner::closed, this, [this] { m_welcomeDismissed = true; });
    m_welcome->hide();
    layout->addWidget(m_welcome);

    m_voiceHint = new ui::Banner(ui::Banner::Kind::Warning, central);
    m_voiceHint->setObjectName(QStringLiteral("voiceHintBanner"));
    m_voiceHint->setClosable(true);
    m_voiceHint->setTitle(tr("No Cantonese voice is available for read-aloud"));
    QPushButton *speechSettings = m_voiceHint->addButton(tr("Speech settings"));
    connect(speechSettings, &QPushButton::clicked, this, [this] { openSettings(SettingsDialog::Tab::Speech); });
    connect(m_voiceHint, &ui::Banner::closed, this, [this] { m_voiceHintDismissed = true; });
    m_voiceHint->hide();
    layout->addWidget(m_voiceHint);

    m_splitter = new QSplitter(Qt::Horizontal, central);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(12);
    m_input = new InputPane(m_splitter);
    m_result = new ResultView(m_splitter);
    m_input->setMinimumSize(300, 220);
    m_result->setMinimumSize(300, 220);
    m_splitter->addWidget(m_input);
    m_splitter->addWidget(m_result);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);
    layout->addWidget(m_splitter, 1);
    setCentralWidget(central);

    m_input->setSpeechController(m_speechController);
    m_result->setSpeechController(m_speechController);

    buildHistoryDock();
    buildShortcuts();

    statusBar()->setSizeGripEnabled(true);

    // Restore state.
    if (m_settings) {
        m_direction = m_settings->direction();
        m_tone = m_settings->tone();
        if (!restoreGeometry(m_settings->windowGeometry()))
            resize(1220, 760);
        restoreState(m_settings->windowState());
        m_historyDock->setVisible(m_settings->historyVisible());
    } else {
        resize(1220, 760);
    }
    m_toneControl->setCurrentIndex(int(m_tone));
    updateDirectionUi();
    applySettings();

    // Wiring between panes.
    connect(m_input, &InputPane::translateRequested, this, &MainWindow::translateNow);
    connect(m_input, &InputPane::switchDirectionRequested, this, [this] { setDirection(reversed(m_direction)); });
    connect(m_input, &InputPane::cleared, this, [this] {
        if (!m_translation || !m_translation->isBusy())
            m_result->showEmpty();
    });
    connect(m_result, &ResultView::cancelRequested, this, &MainWindow::cancelOrStop);
    connect(m_result, &ResultView::retryRequested, this, [this] {
        if (!m_lastRequest.text.trimmed().isEmpty() && m_translation)
            m_translation->translate(m_lastRequest);
        else
            translateNow();
    });
    connect(m_result, &ResultView::openSettingsRequested, this, [this] { openSettings(SettingsDialog::Tab::AI); });
    connect(m_result, &ResultView::starToggled, this, &MainWindow::onStarToggled);
    connect(m_result, &ResultView::statusMessage, this, [this](const QString &m) { showStatus(m); });
    connect(m_result, &ResultView::exampleChosen, this, [this](const QString &text) {
        setInputText(text);
        translateNow();
    });
    connect(m_speechController, &SpeechController::voiceUnavailable, this, &MainWindow::onVoiceUnavailable);
    connect(m_speechController, &SpeechController::message, this, [this](const QString &m) { showStatus(m, 6000); });

    connectServices();
    updateLayoutForWidth();
}

MainWindow::~MainWindow() = default;

// ---- Construction ------------------------------------------------------------------

QWidget *MainWindow::buildHeader()
{
    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("headerBar"));
    auto *h = new QHBoxLayout(header);
    h->setContentsMargins(0, 0, 8, 0);
    h->setSpacing(8);

    m_logo = new QLabel(header);
    m_logo->setPixmap(ui::appLogo(30, devicePixelRatioF()));
    m_logo->setFixedSize(30, 30);
    h->addWidget(m_logo);
    m_appTitle = new QLabel(QStringLiteral(SCT_APP_NAME), header);
    m_appTitle->setObjectName(QStringLiteral("appTitle"));
    m_appTitle->setFont(Theme::uiFont(12, QFont::DemiBold));
    h->addWidget(m_appTitle);
    h->addStretch(1);

    // Direction: [English] ⇄ [廣東話 Cantonese]
    m_sourcePill = new QLabel(header);
    m_sourcePill->setProperty("role", QStringLiteral("langPill"));
    m_sourcePill->setAlignment(Qt::AlignCenter);
    m_targetPill = new QLabel(header);
    m_targetPill->setProperty("role", QStringLiteral("langPill"));
    m_targetPill->setAlignment(Qt::AlignCenter);
    for (QLabel *pill : {m_sourcePill, m_targetPill})
        pill->setFont(Theme::textFont(Language::Cantonese, 10.5));
    m_sourcePill->setToolTip(tr("Translate from"));
    m_targetPill->setToolTip(tr("Translate to"));
    m_swap = ui::makeIconButton(QStringLiteral("swap"), ui::withShortcut(tr("Swap languages"), keySwap()), header,
                                IconTone::Text, 20);
    m_swap->setObjectName(QStringLiteral("swapButton"));
    connect(m_swap, &QToolButton::clicked, this, &MainWindow::swapDirection);
    h->addWidget(m_sourcePill);
    h->addWidget(m_swap);
    h->addWidget(m_targetPill);
    h->addSpacing(18);

    m_toneLabel = new QLabel(tr("Tone"), header);
    m_toneLabel->setProperty("role", QStringLiteral("muted"));
    h->addWidget(m_toneLabel);
    m_toneControl = new ui::SegmentedControl(header);
    m_toneControl->setProperty("toneControl", true);
    m_toneControl->addSegment(tr("Casual"), tr("Casual: relaxed, everyday Cantonese - how friends and family talk "
                                                "(e.g. 咩呀, 得啦). For English, informal wording."));
    m_toneControl->addSegment(tr("Neutral"), tr("Neutral: natural, standard spoken Cantonese that suits most situations."));
    m_toneControl->addSegment(tr("Polite"), tr("Polite: courteous Cantonese for customers, elders and formal "
                                               "situations (e.g. 唔該, 請問). For English, formal wording."));
    connect(m_toneControl, &ui::SegmentedControl::currentIndexChanged, this, [this](int index) {
        setTone(static_cast<Tone>(index));
        // Re-translate the shown text in the new tone.
        if (m_result->page() == ResultView::Page::Result && m_result->hasResult()
            && m_result->result().request.text.trimmed() == m_input->text().trimmed()
            && m_result->result().request.tone != m_tone) {
            translateNow();
        }
    });
    h->addWidget(m_toneControl);
    h->addStretch(1);

    m_providerBadge = new QToolButton(header);
    m_providerBadge->setObjectName(QStringLiteral("providerBadge"));
    m_providerBadge->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_providerBadge->setIconSize(QSize(16, 16));
    m_providerBadge->setCursor(Qt::PointingHandCursor);
    connect(m_providerBadge, &QToolButton::clicked, this, [this] { openSettings(SettingsDialog::Tab::AI); });
    h->addWidget(m_providerBadge);
    h->addSpacing(4);

    m_historyButton = ui::makeIconButton(QStringLiteral("history"), ui::withShortcut(tr("History"), keyHistory()),
                                         header, IconTone::Text, 20);
    m_historyButton->setObjectName(QStringLiteral("historyButton"));
    m_historyButton->setCheckable(true);
    h->addWidget(m_historyButton);

    m_settingsButton = ui::makeIconButton(QStringLiteral("settings"), ui::withShortcut(tr("Settings"), keySettings()),
                                          header, IconTone::Text, 20);
    m_settingsButton->setObjectName(QStringLiteral("settingsButton"));
    connect(m_settingsButton, &QToolButton::clicked, this, [this] { openSettings(SettingsDialog::Tab::AI); });
    h->addWidget(m_settingsButton);

    m_helpButton = ui::makeIconButton(QStringLiteral("help"), tr("Help"), header, IconTone::Text, 20);
    m_helpButton->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(m_helpButton);
    connect(menu->addAction(ui::icon(QStringLiteral("keyboard")), tr("Keyboard shortcuts")), &QAction::triggered, this,
            &MainWindow::showShortcuts);
    menu->addSeparator();
    connect(menu->addAction(ui::icon(QStringLiteral("external")), tr("Get a Claude API key")), &QAction::triggered, this,
            [] { QDesktopServices::openUrl(QUrl(kClaudeKeysUrl)); });
    connect(menu->addAction(ui::icon(QStringLiteral("external")), tr("Get an OpenAI API key")), &QAction::triggered,
            this, [] { QDesktopServices::openUrl(QUrl(kOpenAiKeysUrl)); });
    menu->addSeparator();
    connect(menu->addAction(ui::icon(QStringLiteral("info")), tr("About %1").arg(QStringLiteral(SCT_APP_NAME))),
            &QAction::triggered, this, &MainWindow::showAbout);
    m_helpButton->setMenu(menu);
    h->addWidget(m_helpButton);
    return header;
}

void MainWindow::buildHistoryDock()
{
    m_historyDock = new QDockWidget(tr("History"), this);
    m_historyDock->setObjectName(QStringLiteral("historyDock"));
    m_historyDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    m_historyDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    // Custom title bar: matches the pane headers instead of the Fusion strip.
    auto *title = new QWidget(m_historyDock);
    title->setObjectName(QStringLiteral("dockTitle"));
    auto *th = new QHBoxLayout(title);
    th->setContentsMargins(12, 12, 14, 4);
    auto *label = new QLabel(tr("History"), title);
    label->setProperty("role", QStringLiteral("paneTitle"));
    label->setFont(Theme::uiFont(-1, QFont::DemiBold));
    th->addWidget(label);
    th->addStretch(1);
    QToolButton *close = ui::makeIconButton(QStringLiteral("close"), ui::withShortcut(tr("Hide history"), keyHistory()),
                                            title, IconTone::Muted, 16);
    connect(close, &QToolButton::clicked, m_historyDock, &QDockWidget::hide);
    th->addWidget(close);
    m_historyDock->setTitleBarWidget(title);

    m_historyPanel = new HistoryPanel(m_translation ? m_translation->history() : nullptr, m_historyDock);
    m_historyPanel->setMinimumWidth(260);
    m_historyDock->setWidget(m_historyPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_historyDock);
    resizeDocks({m_historyDock}, {340}, Qt::Horizontal);

    QAction *toggle = m_historyDock->toggleViewAction();
    connect(m_historyButton, &QToolButton::clicked, this, [this] { setHistoryVisible(!isHistoryVisible()); });
    connect(toggle, &QAction::toggled, m_historyButton, &QToolButton::setChecked);
    m_historyButton->setChecked(!m_historyDock->isHidden());

    connect(m_historyPanel, &HistoryPanel::entryActivated, this, &MainWindow::restoreHistoryEntry);
    connect(m_historyPanel, &HistoryPanel::statusMessage, this, [this](const QString &m) { showStatus(m); });
}

void MainWindow::buildShortcuts()
{
    auto add = [this](const QKeySequence &key, auto slot) {
        auto *s = new QShortcut(key, this);
        s->setContext(Qt::WindowShortcut);
        connect(s, &QShortcut::activated, this, slot);
        return s;
    };
    add(keyTranslate(), [this] { translateNow(); });
    add(QKeySequence(Qt::CTRL | Qt::Key_Enter), [this] { translateNow(); });
    add(QKeySequence(Qt::Key_Escape), [this] { cancelOrStop(); });
    add(keySwap(), [this] { swapDirection(); });
    add(keySpeak(), [this] {
        if (m_result->page() == ResultView::Page::Result && m_result->hasResult())
            m_result->speakButton()->click();
        else
            showStatus(tr("Translate something first, then press %1 to hear it.")
                           .arg(keySpeak().toString(QKeySequence::NativeText)));
    });
    add(keyHistory(), [this] { setHistoryVisible(!isHistoryVisible()); });
    add(keySettings(), [this] { openSettings(SettingsDialog::Tab::AI); });
    add(keyCopy(), [this] { m_result->copyTranslation(); });
    add(QKeySequence(Qt::CTRL | Qt::Key_F), [this] {
        setHistoryVisible(true);
        m_historyPanel->searchBox()->setFocus();
        m_historyPanel->searchBox()->selectAll();
    });
    add(QKeySequence(Qt::CTRL | Qt::Key_L), [this] { m_input->focusEditor(); });
    add(QKeySequence(Qt::Key_F1), [this] { showShortcuts(); });
}

void MainWindow::connectServices()
{
    if (m_translation) {
        connect(m_translation, &TranslationService::started, this, [this](const TranslationRequest &request) {
            m_lastRequest = request;
            m_result->showLoading();
        });
        connect(m_translation, &TranslationService::finished, this, &MainWindow::onFinished);
        connect(m_translation, &TranslationService::failed, this, &MainWindow::onFailed);
        if (HistoryStore *h = m_translation->history())
            connect(h, &HistoryStore::changed, this, &MainWindow::refreshStarFromHistory);
    }
    if (m_speech) {
        connect(m_speech, &SpeechService::notice, this, [this](const QString &m) { showStatus(m, 8000); });
        connect(m_speech, &SpeechService::voicesChanged, this, [this] {
            if (m_speech->canSpeak(Language::Cantonese))
                m_voiceHint->hide();
        });
    }
    if (m_settings)
        connect(m_settings, &AppSettings::changed, this, &MainWindow::applySettings);
}

// ---- Direction & tone -------------------------------------------------------------------

void MainWindow::setDirection(Direction direction)
{
    if (direction == m_direction)
        return;
    m_direction = direction;
    if (m_settings)
        m_settings->setDirection(direction);
    updateDirectionUi();
    // A result for the other direction no longer matches the panes.
    if (m_result->page() == ResultView::Page::Result && m_result->result().request.direction != direction)
        m_result->showEmpty();
}

void MainWindow::swapDirection()
{
    const bool hasResult = m_result->page() == ResultView::Page::Result && m_result->hasResult()
                           && m_result->result().request.direction == m_direction;
    const QString moved = hasResult ? m_result->result().translation.trimmed() : QString();
    if (m_translation && m_translation->isBusy())
        m_translation->cancel();
    setDirection(reversed(m_direction));
    if (!moved.isEmpty()) {
        m_input->setText(moved);
        translateNow();
    }
    m_input->focusEditor();
}

void MainWindow::setTone(Tone tone)
{
    m_tone = tone;
    m_toneControl->setCurrentIndex(int(tone));
    if (m_settings && m_settings->tone() != tone)
        m_settings->setTone(tone);
}

void MainWindow::updateDirectionUi()
{
    const Language src = sourceLanguage(m_direction);
    const Language tgt = targetLanguage(m_direction);
    m_sourcePill->setText(Theme::languageLabel(src));
    m_targetPill->setText(Theme::languageLabel(tgt));
    // Same width for both pills keeps the swap button in place.
    const QFontMetrics fm(m_sourcePill->font());
    const int w = qMax(fm.horizontalAdvance(Theme::languageLabel(Language::English)),
                       fm.horizontalAdvance(Theme::languageLabel(Language::Cantonese)))
                  + 34;
    m_sourcePill->setFixedWidth(w);
    m_targetPill->setFixedWidth(w);
    m_input->setDirection(m_direction);
    m_result->setDirection(m_direction);
}

// ---- Translation flow -------------------------------------------------------------------

void MainWindow::setInputText(const QString &text) { m_input->setText(text); }

void MainWindow::translateNow()
{
    const QString text = m_input->text().trimmed();
    if (text.isEmpty()) {
        showStatus(tr("Type or paste something to translate first."));
        m_input->focusEditor();
        return;
    }
    if (!m_translation)
        return;
    TranslationRequest request;
    request.text = text;
    request.direction = m_direction;
    request.tone = m_tone;
    if (m_settings) {
        request.script = m_settings->script();
        request.wantAlternatives = m_settings->showAlternatives();
        request.wantNotes = m_settings->showNotes();
    }
    m_lastRequest = request;
    if (m_speechController->isSpeaking())
        m_speechController->stop();
    m_translation->translate(request);
}

void MainWindow::cancelOrStop()
{
    if (m_translation && (m_translation->isBusy() || m_result->page() == ResultView::Page::Loading)) {
        m_translation->cancel();
        if (m_result->hasResult())
            m_result->showResult(m_result->result());
        else
            m_result->showEmpty();
        refreshStarFromHistory();
        showStatus(tr("Translation cancelled"));
        return;
    }
    if (m_speechController->isSpeaking())
        m_speechController->stop();
}

void MainWindow::showResult(const TranslationResult &result, bool fromHistory)
{
    m_result->showResult(result, fromHistory);
    refreshStarFromHistory();
}

void MainWindow::showError(const TranslationError &error) { m_result->showError(error); }

void MainWindow::onFinished(const TranslationResult &result)
{
    showResult(result);
    if (result.fromCache)
        showStatus(tr("Served from cache - no API call needed"));
    if (m_settings && m_settings->autoSpeak() && result.isValid()) {
        m_speechController->speak({result.translation, targetLanguage(result.request.direction)},
                                  m_result->speakButton());
    }
}

void MainWindow::onFailed(const TranslationError &error)
{
    if (error.kind == ErrorKind::Cancelled) {
        if (m_result->page() == ResultView::Page::Loading) {
            if (m_result->hasResult())
                m_result->showResult(m_result->result());
            else
                m_result->showEmpty();
        }
        return;
    }
    showError(error);
    if (error.kind == ErrorKind::NotConfigured)
        updateWelcomeBanner();
}

void MainWindow::restoreHistoryEntry(const HistoryEntry &entry)
{
    if (m_translation && m_translation->isBusy())
        m_translation->cancel();
    const TranslationResult &r = entry.result;
    m_direction = r.request.direction;
    if (m_settings)
        m_settings->setDirection(m_direction);
    updateDirectionUi();
    setTone(r.request.tone);
    m_input->setText(r.request.text);
    m_lastRequest = r.request;
    m_result->showResult(r, true);
    m_result->setStarred(entry.starred);
    showStatus(tr("Restored from history"));
}

// ---- Stars ----------------------------------------------------------------------------------

QUuid MainWindow::findHistoryId(const TranslationResult &result) const
{
    HistoryStore *h = m_translation ? m_translation->history() : nullptr;
    if (!h || !result.isValid())
        return {};
    for (const HistoryEntry &e : h->entries()) {
        if (e.result.request.direction == result.request.direction
            && e.result.request.text.trimmed() == result.request.text.trimmed()
            && e.result.translation.trimmed() == result.translation.trimmed())
            return e.id;
    }
    return {};
}

void MainWindow::refreshStarFromHistory()
{
    if (m_result->page() != ResultView::Page::Result || !m_result->hasResult())
        return;
    HistoryStore *h = m_translation ? m_translation->history() : nullptr;
    const QUuid id = findHistoryId(m_result->result());
    bool starred = false;
    if (h && !id.isNull()) {
        for (const HistoryEntry &e : h->entries()) {
            if (e.id == id) {
                starred = e.starred;
                break;
            }
        }
    }
    m_result->setStarred(starred);
}

void MainWindow::onStarToggled(bool starred)
{
    HistoryStore *h = m_translation ? m_translation->history() : nullptr;
    if (!h || !m_result->hasResult())
        return;
    QUuid id = findHistoryId(m_result->result());
    if (id.isNull())
        id = h->add(m_result->result());
    h->setStarred(id, starred);
    showStatus(starred ? tr("Starred - find it in History with \"Starred only\"") : tr("Star removed"));
}

// ---- Speech ----------------------------------------------------------------------------------

void MainWindow::onVoiceUnavailable(Language lang)
{
    if (lang == Language::Cantonese) {
        if (!m_voiceHintDismissed) {
            QString help = SpeechService::cantoneseVoiceHelpText().toHtmlEscaped();
            help.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
            m_voiceHint->setText(help);
            m_voiceHint->show();
        } else {
            showStatus(tr("No Cantonese voice is installed - see Settings ▸ Speech."), 6000);
        }
        return;
    }
    showStatus(tr("Read-aloud isn't available for English on this computer - see Settings ▸ Speech."), 6000);
}

// ---- Settings ----------------------------------------------------------------------------------

SettingsDialog *MainWindow::openSettings(SettingsDialog::Tab tab)
{
    if (!m_settingsDialog) {
        m_settingsDialog = new SettingsDialog(m_settings, m_translation, m_speech, this);
        m_settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
        m_settingsDialog->setWindowModality(Qt::WindowModal);
    }
    m_settingsDialog->setCurrentTab(tab);
    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
    return m_settingsDialog;
}

void MainWindow::applySettings()
{
    if (!m_settings)
        return;
    const QString theme = m_settings->theme();
    if (theme != m_appliedTheme) {
        m_appliedTheme = theme;
        if (theme != Theme::mode() || !Theme::isApplied())
            Theme::apply(theme);
    }
    const int pt = m_settings->fontPointSize();
    m_input->setTextPointSize(pt);
    m_input->setScript(m_settings->script());
    m_result->setTextPointSize(pt);
    m_result->setDisplayOptions(m_settings->showJyutping(), m_settings->showAlternatives(), m_settings->showNotes());
    updateProviderBadge();
    updateWelcomeBanner();
    if (m_translation && m_translation->isActiveProviderConfigured() && !m_settings->firstRunCompleted())
        m_settings->setFirstRunCompleted(true);
}

void MainWindow::updateProviderBadge()
{
    const QString id = m_translation ? m_translation->activeProviderId() : QStringLiteral("claude");
    const bool configured = m_translation && m_translation->isActiveProviderConfigured();
    const QString model = m_translation ? m_translation->activeModel() : QString();
    if (configured) {
        m_providerBadge->setText(model.isEmpty() ? shortProviderName(id)
                                                 : QStringLiteral("%1 · %2").arg(shortProviderName(id), model));
        m_providerBadge->setIcon(ui::icon(QStringLiteral("sparkles"), IconTone::Accent));
        m_providerBadge->setToolTip(ui::withShortcut(tr("AI provider and model - click to change"), keySettings()));
    } else {
        m_providerBadge->setText(tr("%1 · add API key").arg(shortProviderName(id)));
        m_providerBadge->setIcon(ui::icon(QStringLiteral("key"), IconTone::Warning));
        m_providerBadge->setToolTip(tr("No API key yet - click to set one up"));
    }
    ui::setStyleProperty(m_providerBadge, "warning", !configured);
}

void MainWindow::updateWelcomeBanner()
{
    const bool configured = m_translation && m_translation->isActiveProviderConfigured();
    if (configured || m_welcomeDismissed) {
        m_welcome->hide();
        return;
    }
    const bool firstRun = m_settings && !m_settings->firstRunCompleted();
    m_welcome->setTitle(firstRun ? tr("Welcome! Let's connect an AI provider")
                                 : tr("Add an API key to start translating"));
    m_welcome->setText(
        tr("Translations are written by Claude or OpenAI using your own API key. Get one in a minute - "
           "<a href=\"%1\">Claude key</a> or <a href=\"%2\">OpenAI key</a> - then paste it in Settings. "
           "Your key is stored encrypted on this PC.")
            .arg(kClaudeKeysUrl, kOpenAiKeysUrl));
    m_welcome->show();
}

// ---- Window ------------------------------------------------------------------------------------

void MainWindow::setHistoryVisible(bool visible)
{
    m_historyDock->setVisible(visible);
    if (m_settings)
        m_settings->setHistoryVisible(visible);
}

bool MainWindow::isHistoryVisible() const { return !m_historyDock->isHidden(); }

void MainWindow::showAbout() { ui::showAboutDialog(this); }

void MainWindow::showShortcuts() { ui::showShortcutsDialog(this); }

void MainWindow::showStatus(const QString &message, int timeoutMs)
{
    statusBar()->showMessage(message, timeoutMs);
}

void MainWindow::updateLayoutForWidth()
{
    const int w = centralWidget() ? centralWidget()->width() : width();
    m_splitter->setOrientation(w < 760 ? Qt::Vertical : Qt::Horizontal);
    m_appTitle->setVisible(width() >= 1180 || (!isHistoryVisible() && width() >= 980));
    m_toneLabel->setVisible(w >= 900);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateLayoutForWidth();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_firstShow) {
        m_firstShow = false;
        m_logo->setPixmap(ui::appLogo(30, devicePixelRatioF()));
        m_input->focusEditor();
        QTimer::singleShot(0, this, &MainWindow::updateLayoutForWidth);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_settings) {
        m_settings->beginBatch();
        m_settings->setWindowGeometry(saveGeometry());
        m_settings->setWindowState(saveState());
        m_settings->setHistoryVisible(isHistoryVisible());
        m_settings->endBatch();
        m_settings->sync();
    }
    m_speechController->stop();
    if (m_translation && m_translation->isBusy())
        m_translation->cancel();
    QMainWindow::closeEvent(event);
}

} // namespace sct
