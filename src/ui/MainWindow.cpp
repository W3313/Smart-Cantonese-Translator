#include "ui/MainWindow.h"

#include "core/AppSettings.h"
#include "core/HistoryStore.h"
#include "core/TranslationService.h"
#include "core/Version.h"
#include "tts/SpeechService.h"
#include "ui/AboutDialog.h"
#include "ui/Controls.h"
#include "ui/HistoryPanel.h"
#include "ui/InputPane.h"
#include "ui/Motion.h"
#include "ui/ResultView.h"
#include "ui/SpeechController.h"
#include "ui/Surfaces.h"
#include "ui/Theme.h"

#include <QAction>
#include <QBoxLayout>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace sct {

using ui::IconTone;

namespace {

const QString kClaudeKeysUrl = QStringLiteral("https://console.anthropic.com/settings/keys");
const QString kOpenAiKeysUrl = QStringLiteral("https://platform.openai.com/api-keys");
constexpr int kHistoryWidth = 356;  // includes the 16 px gap to the result card

QString shortProviderName(const QString &id)
{
    if (id == QLatin1String("claude"))
        return QStringLiteral("Claude");
    if (id == QLatin1String("openai"))
        return QStringLiteral("OpenAI");
    return id;
}

QKeySequence keyTranslate() { return QKeySequence(Qt::CTRL | Qt::Key_Return); }
QKeySequence keySwap() { return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S); }
QKeySequence keySpeak() { return QKeySequence(Qt::CTRL | Qt::Key_R); }
QKeySequence keyHistory() { return QKeySequence(Qt::CTRL | Qt::Key_H); }
QKeySequence keySettings() { return QKeySequence(Qt::CTRL | Qt::Key_Comma); }
QKeySequence keyCopy() { return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C); }

// "● Claude · claude-opus-5-5": provider status; opens Settings.
class StatusChip : public ui::ButtonBase
{
public:
    explicit StatusChip(QWidget *parent)
        : ui::ButtonBase(parent)
    {
        setFont(Theme::uiFont(9, QFont::Medium));
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    void setWarning(bool warning)
    {
        m_warning = warning;
        update();
    }

    QSize sizeHint() const override { return QSize(fontMetrics().horizontalAdvance(text()) + 12 + 8 + 8 + 12, 30); }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const ThemeColors &c = Theme::colors();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
        QPainterPath path;
        path.addRoundedRect(r, r.height() / 2, r.height() / 2);
        p.fillPath(path, m_warning ? c.warnBg : ui::withAlpha(c.hover, hoverProgress()));
        if (m_warning) {
            p.setPen(QPen(c.warnBorder, 1));
            p.drawPath(path);
        }
        const QColor dot = m_warning ? c.warnText : c.successText;
        p.setPen(Qt::NoPen);
        p.setBrush(dot);
        p.drawEllipse(QPointF(r.left() + 14, r.center().y()), 3.5, 3.5);
        p.setPen(m_warning ? c.warnText : ui::mix(c.textMuted, c.text, hoverProgress()));
        p.setFont(font());
        p.drawText(r.adjusted(24, 0, -10, 0), Qt::AlignLeft | Qt::AlignVCenter, text());
        paintFocusRing(&p, QRectF(rect()), r.height() / 2 + 1);
    }

private:
    bool m_warning = false;
};

// Places the left/right groups at the edges and keeps the centre group
// centred in the window. When space runs out it hides the app title, then the
// provider chip, so nothing ever overlaps.
class HeaderBar : public QWidget
{
public:
    HeaderBar(QWidget *left, QWidget *centre, QWidget *right, QWidget *title, QWidget *chip, QWidget *parent)
        : QWidget(parent)
        , m_left(left)
        , m_centre(centre)
        , m_right(right)
        , m_title(title)
        , m_chip(chip)
    {
        for (QWidget *w : {left, centre, right})
            w->setParent(this);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    QSize sizeHint() const override
    {
        const int h = qMax(m_left->sizeHint().height(), qMax(m_centre->sizeHint().height(), m_right->sizeHint().height()));
        return QSize(m_left->sizeHint().width() + m_centre->sizeHint().width() + m_right->sizeHint().width() + 48, h);
    }
    QSize minimumSizeHint() const override { return QSize(m_centre->sizeHint().width(), sizeHint().height()); }

protected:
    bool event(QEvent *e) override
    {
        // A group's size hint changed (e.g. the provider chip text).
        if (e->type() == QEvent::LayoutRequest)
            relayout();
        return QWidget::event(e);
    }

    void resizeEvent(QResizeEvent *) override { relayout(); }

private:
    void relayout()
    {
        const int w = width();
        const int h = height();
        const int gap = 16;
        const int cw = m_centre->sizeHint().width();
        // Decide from the full widths so showing/hiding never oscillates.
        const int chipW = m_chip->sizeHint().width() + 8;
        int rightW = m_right->sizeHint().width() + (m_chip->isHidden() ? chipW : 0);
        const int logoW = 28;
        const int titleW = m_title->sizeHint().width() + 10;
        auto fits = [&](int leftW, int rw) { return leftW + gap + cw + gap + rw <= w; };
        const bool showTitle = fits(logoW + titleW, rightW);
        const bool showChip = fits(logoW, rightW);
        if (!showChip)
            rightW -= chipW;
        if (m_title->isHidden() == showTitle)
            m_title->setVisible(showTitle);
        if (m_chip->isHidden() == showChip)
            m_chip->setVisible(showChip);
        const int leftW = logoW + (showTitle ? titleW : 0);

        const int lh = m_left->sizeHint().height();
        const int ch = m_centre->sizeHint().height();
        const int rh = m_right->sizeHint().height();
        m_right->setGeometry(w - rightW, (h - rh) / 2, rightW, rh);
        int cx = (w - cw) / 2;
        cx = qMin(cx, w - rightW - gap - cw);
        cx = qMax(cx, leftW + gap);
        m_left->setGeometry(0, (h - lh) / 2, leftW, lh);
        m_centre->setGeometry(cx, (h - ch) / 2, cw, ch);
    }

    QWidget *m_left;
    QWidget *m_centre;
    QWidget *m_right;
    QWidget *m_title;
    QWidget *m_chip;
};

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
    setMinimumSize(780, 540);

    m_speechController = new SpeechController(m_speech, this);

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("centralArea"));
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(20, 14, 20, 16);
    layout->setSpacing(12);
    layout->addWidget(buildHeader());

    m_welcome = new ui::Banner(ui::Banner::Kind::Info, central);
    m_welcome->setObjectName(QStringLiteral("welcomeBanner"));
    m_welcome->setClosable(true);
    ui::Button *setup = m_welcome->addButton(tr("Open Settings"), true);
    connect(setup, &QAbstractButton::clicked, this, [this] { openSettings(SettingsDialog::Tab::AI); });
    connect(m_welcome, &ui::Banner::closed, this, [this] { m_welcomeDismissed = true; });
    m_welcome->hide();
    layout->addWidget(m_welcome);

    m_voiceHint = new ui::Banner(ui::Banner::Kind::Warning, central);
    m_voiceHint->setObjectName(QStringLiteral("voiceHintBanner"));
    m_voiceHint->setClosable(true);
    m_voiceHint->setTitle(tr("No Cantonese voice is available for read-aloud"));
    ui::Button *speechSettings = m_voiceHint->addButton(tr("Speech settings"));
    connect(speechSettings, &QAbstractButton::clicked, this, [this] { openSettings(SettingsDialog::Tab::Speech); });
    connect(m_voiceHint, &ui::Banner::closed, this, [this] { m_voiceHintDismissed = true; });
    m_voiceHint->hide();
    layout->addWidget(m_voiceHint);

    // Cards (side by side, stacked when narrow) + the slide-in history panel.
    auto *bodyRow = new QHBoxLayout;
    bodyRow->setContentsMargins(0, 0, 0, 0);
    bodyRow->setSpacing(0);
    auto *panes = new QWidget(central);
    m_panesLayout = new QBoxLayout(QBoxLayout::LeftToRight, panes);
    m_panesLayout->setContentsMargins(0, 0, 0, 0);
    m_panesLayout->setSpacing(14);
    m_input = new InputPane(panes);
    m_result = new ResultView(panes);
    m_input->setMinimumSize(280, 200);
    m_result->setMinimumSize(280, 220);
    m_panesLayout->addWidget(m_input, 1);
    m_panesLayout->addWidget(m_result, 1);
    bodyRow->addWidget(panes, 1);

    auto *historyHost = new QWidget;
    auto *historyHostLayout = new QHBoxLayout(historyHost);
    historyHostLayout->setContentsMargins(14, 0, 0, 0);
    m_historyPanel = new HistoryPanel(m_translation ? m_translation->history() : nullptr, historyHost);
    historyHostLayout->addWidget(m_historyPanel);
    m_historySide = new ui::SidePanel(historyHost, kHistoryWidth, central);
    m_historySide->setObjectName(QStringLiteral("historySidePanel"));
    bodyRow->addWidget(m_historySide);
    layout->addLayout(bodyRow, 1);
    setCentralWidget(central);

    m_input->setSpeechController(m_speechController);
    m_result->setSpeechController(m_speechController);

    buildShortcuts();

    // Restore state.
    if (m_settings) {
        m_direction = m_settings->direction();
        m_tone = m_settings->tone();
        if (!restoreGeometry(m_settings->windowGeometry()))
            resize(1180, 720);
    } else {
        resize(1180, 720);
    }
    m_toneControl->setCurrentIndex(int(m_tone), false);
    applyDirection(m_direction, false);
    setHistoryVisible(UiPrefs::instance()->historyVisible(), false);
    applySettings();

    // Wiring between the panes.
    connect(m_input, &InputPane::translateRequested, this, &MainWindow::translateNow);
    connect(m_input, &InputPane::cancelRequested, this, &MainWindow::cancelOrStop);
    connect(m_input, &InputPane::switchDirectionRequested, this, [this] { setDirection(reversed(m_direction)); });
    connect(m_input, &InputPane::cleared, this, [this] {
        if (!m_translation || !m_translation->isBusy())
            m_result->showEmpty();
    });
    connect(m_result, &ResultView::retryRequested, this, [this] {
        if (!m_lastRequest.text.trimmed().isEmpty() && m_translation)
            m_translation->translate(m_lastRequest);
        else
            translateNow();
    });
    connect(m_result, &ResultView::openSettingsRequested, this, [this] { openSettings(SettingsDialog::Tab::AI); });
    connect(m_result, &ResultView::starToggled, this, &MainWindow::onStarToggled);
    connect(m_result, &ResultView::statusMessage, this, &MainWindow::showStatus);
    connect(m_result, &ResultView::exampleChosen, this, [this](const QString &text) {
        setInputText(text);
        translateNow();
    });
    connect(m_historyPanel, &HistoryPanel::entryActivated, this, &MainWindow::restoreHistoryEntry);
    connect(m_historyPanel, &HistoryPanel::statusMessage, this, &MainWindow::showStatus);
    connect(m_historyPanel, &HistoryPanel::closeRequested, this, [this] { setHistoryVisible(false); });
    connect(m_speechController, &SpeechController::voiceUnavailable, this, &MainWindow::onVoiceUnavailable);
    connect(m_speechController, &SpeechController::message, this, [this](const QString &m) { showStatus(m); });
    connect(UiPrefs::instance(), &UiPrefs::changed, this, [this] { m_historyButton->setChecked(isHistoryVisible()); });

    connectServices();
    updateLayoutForWidth();
}

MainWindow::~MainWindow() = default;

// ---- Construction ------------------------------------------------------------------

QWidget *MainWindow::buildHeader()
{
    // Left: logo + name.
    auto *left = new QWidget;
    auto *lh = new QHBoxLayout(left);
    lh->setContentsMargins(0, 0, 0, 0);
    lh->setSpacing(10);
    m_logo = new QLabel(left);
    m_logo->setFixedSize(28, 28);
    m_logo->setPixmap(ui::appLogo(28, devicePixelRatioF()));
    lh->addWidget(m_logo);
    m_appTitle = new QLabel(QStringLiteral(SCT_APP_NAME), left);
    m_appTitle->setObjectName(QStringLiteral("appTitle"));
    m_appTitle->setFont(Theme::uiFont(11.5, QFont::DemiBold));
    m_appTitle->setMinimumWidth(0);
    lh->addWidget(m_appTitle);
    lh->addStretch(1);

    // Centre: direction pill + tone.
    auto *centre = new QWidget;
    auto *ch = new QHBoxLayout(centre);
    ch->setContentsMargins(0, 0, 0, 0);
    ch->setSpacing(12);
    m_directionPill = new ui::DirectionPill(centre);
    m_directionPill->setObjectName(QStringLiteral("directionPill"));
    m_directionPill->setToolTip(ui::withShortcut(tr("Swap languages"), keySwap()));
    connect(m_directionPill, &QAbstractButton::clicked, this, &MainWindow::swapDirection);
    ch->addWidget(m_directionPill);
    m_toneControl = new ui::SegmentedControl(centre);
    m_toneControl->setObjectName(QStringLiteral("toneControl"));
    m_toneControl->addSegment(tr("Casual"), tr("Casual - relaxed, everyday Cantonese, how friends and family talk "
                                                "(咩呀, 得啦). Informal English the other way."));
    m_toneControl->addSegment(tr("Neutral"), tr("Neutral - natural spoken Cantonese that suits most situations."));
    m_toneControl->addSegment(tr("Polite"), tr("Polite - courteous Cantonese for customers, elders and formal "
                                               "situations (唔該, 請問). Formal English the other way."));
    connect(m_toneControl, &ui::SegmentedControl::currentIndexChanged, this, [this](int index) {
        setTone(static_cast<Tone>(index));
        // Re-translate the shown text in the new tone.
        if (m_result->page() == ResultView::Page::Result && m_result->hasResult()
            && m_result->result().request.text.trimmed() == m_input->text().trimmed()
            && m_result->result().request.tone != m_tone)
            translateNow();
    });
    ch->addWidget(m_toneControl);

    // Right: provider status, history, settings, more.
    auto *right = new QWidget;
    auto *rh = new QHBoxLayout(right);
    rh->setContentsMargins(0, 0, 0, 0);
    rh->setSpacing(2);
    auto *chip = new StatusChip(right);
    chip->setObjectName(QStringLiteral("providerChip"));
    m_providerChip = chip;
    connect(m_providerChip, &QAbstractButton::clicked, this, [this] { openSettings(SettingsDialog::Tab::AI); });
    rh->addWidget(m_providerChip);
    rh->addSpacing(6);
    m_historyButton = new ui::IconButton(QStringLiteral("history"), ui::withShortcut(tr("History"), keyHistory()), right,
                                         IconTone::Text, 20);
    m_historyButton->setObjectName(QStringLiteral("historyButton"));
    m_historyButton->setCheckable(true);
    connect(m_historyButton, &QAbstractButton::clicked, this, [this] { setHistoryVisible(!isHistoryVisible()); });
    rh->addWidget(m_historyButton);
    m_settingsButton = new ui::IconButton(QStringLiteral("settings"), ui::withShortcut(tr("Settings"), keySettings()),
                                          right, IconTone::Text, 20);
    m_settingsButton->setObjectName(QStringLiteral("settingsButton"));
    connect(m_settingsButton, &QAbstractButton::clicked, this, [this] { openSettings(SettingsDialog::Tab::AI); });
    rh->addWidget(m_settingsButton);
    m_moreButton = new ui::IconButton(QStringLiteral("more"), tr("More"), right, IconTone::Text, 20);
    m_moreButton->setObjectName(QStringLiteral("moreButton"));
    connect(m_moreButton, &QAbstractButton::clicked, this, [this] {
        QMenu menu(this);
        connect(menu.addAction(ui::icon(QStringLiteral("keyboard")), tr("Keyboard shortcuts")), &QAction::triggered,
                this, &MainWindow::showShortcuts);
        menu.addSeparator();
        connect(menu.addAction(ui::icon(QStringLiteral("external")), tr("Get a Claude API key")), &QAction::triggered,
                this, [] { QDesktopServices::openUrl(QUrl(kClaudeKeysUrl)); });
        connect(menu.addAction(ui::icon(QStringLiteral("external")), tr("Get an OpenAI API key")), &QAction::triggered,
                this, [] { QDesktopServices::openUrl(QUrl(kOpenAiKeysUrl)); });
        menu.addSeparator();
        connect(menu.addAction(ui::icon(QStringLiteral("info")), tr("About %1").arg(QStringLiteral(SCT_APP_NAME))),
                &QAction::triggered, this, &MainWindow::showAbout);
        menu.exec(m_moreButton->mapToGlobal(QPoint(m_moreButton->width() - menu.sizeHint().width(),
                                                   m_moreButton->height() + 4)));
    });
    rh->addWidget(m_moreButton);

    auto *header = new HeaderBar(left, centre, right, m_appTitle, m_providerChip, this);
    header->setObjectName(QStringLiteral("headerBar"));
    return header;
}

void MainWindow::buildShortcuts()
{
    auto add = [this](const QKeySequence &key, auto slot) {
        auto *s = new QShortcut(key, this);
        s->setContext(Qt::WindowShortcut);
        connect(s, &QShortcut::activated, this, slot);
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
        m_historyPanel->focusSearch();
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
        connect(m_translation, &TranslationService::busyChanged, m_input, &InputPane::setBusy);
        if (HistoryStore *h = m_translation->history())
            connect(h, &HistoryStore::changed, this, &MainWindow::refreshStarFromHistory);
    }
    if (m_speech) {
        connect(m_speech, &SpeechService::notice, this, [this](const QString &m) { showStatus(m, QStringLiteral("info")); });
        connect(m_speech, &SpeechService::voicesChanged, this, [this] {
            if (m_speech->canSpeak(Language::Cantonese))
                m_voiceHint->hide();
        });
    }
    if (m_settings)
        connect(m_settings, &AppSettings::changed, this, &MainWindow::applySettings);
}

// ---- Direction & tone -------------------------------------------------------------------

void MainWindow::applyDirection(Direction direction, bool animated)
{
    m_direction = direction;
    m_directionPill->setDirection(direction, animated);
    m_input->setDirection(direction);
    m_result->setDirection(direction);
}

void MainWindow::setDirection(Direction direction)
{
    if (direction == m_direction)
        return;
    if (m_settings)
        m_settings->setDirection(direction);
    motion::crossFade(m_input, motion::kNormal);
    applyDirection(direction, true);
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
    const Direction next = reversed(m_direction);
    if (m_settings)
        m_settings->setDirection(next);
    // Text trades places with a quick cross-fade.
    motion::crossFade(m_input, motion::kNormal);
    if (!moved.isEmpty())
        motion::crossFade(m_result, motion::kNormal);
    applyDirection(next, true);
    if (!moved.isEmpty()) {
        m_input->setText(moved);
        translateNow();
    } else if (m_result->page() == ResultView::Page::Result || m_result->page() == ResultView::Page::Error) {
        m_result->showEmpty();
    }
    m_input->focusEditor();
}

void MainWindow::setTone(Tone tone)
{
    m_tone = tone;
    if (m_toneControl->currentIndex() != int(tone))
        m_toneControl->setCurrentIndex(int(tone), true);
    if (m_settings && m_settings->tone() != tone)
        m_settings->setTone(tone);
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
        m_input->setBusy(false);
        if (m_result->hasResult())
            m_result->showResult(m_result->result(), false, false);
        else
            m_result->showEmpty();
        refreshStarFromHistory();
        showStatus(tr("Translation cancelled"));
        return;
    }
    if (m_speechController->isSpeaking()) {
        m_speechController->stop();
        return;
    }
    if (isHistoryVisible() && m_historyPanel->isAncestorOf(focusWidget())) {
        setHistoryVisible(false);
        m_input->focusEditor();
    }
}

void MainWindow::showResult(const TranslationResult &result, bool fromHistory)
{
    if (result.isValid() && result.request.direction != m_direction) {
        if (m_settings)
            m_settings->setDirection(result.request.direction);
        applyDirection(result.request.direction, true);
    }
    m_result->showResult(result, fromHistory);
    refreshStarFromHistory();
}

void MainWindow::showError(const TranslationError &error) { m_result->showError(error); }

void MainWindow::onFinished(const TranslationResult &result)
{
    showResult(result);
    if (result.fromCache)
        showStatus(tr("Served from cache - no API call needed"), QStringLiteral("check"));
    if (m_settings && m_settings->autoSpeak() && result.isValid())
        m_speechController->speak({result.translation, targetLanguage(result.request.direction)},
                                  m_result->speakButton());
}

void MainWindow::onFailed(const TranslationError &error)
{
    if (error.kind == ErrorKind::Cancelled) {
        if (m_result->page() == ResultView::Page::Loading) {
            if (m_result->hasResult())
                m_result->showResult(m_result->result(), false, false);
            else
                m_result->showEmpty();
        }
        return;
    }
    showError(error);
    if (error.kind == ErrorKind::NotConfigured)
        updateWelcomeBanner(true);
}

void MainWindow::restoreHistoryEntry(const HistoryEntry &entry)
{
    if (m_translation && m_translation->isBusy())
        m_translation->cancel();
    const TranslationResult &r = entry.result;
    if (r.request.direction != m_direction) {
        if (m_settings)
            m_settings->setDirection(r.request.direction);
        applyDirection(r.request.direction, true);
    }
    setTone(r.request.tone);
    motion::crossFade(m_input, motion::kFast);
    m_input->setText(r.request.text);
    m_lastRequest = r.request;
    m_result->showResult(r, true);
    m_result->setStarred(entry.starred);
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
    showStatus(starred ? tr("Starred - find it in History") : tr("Star removed"),
               starred ? QStringLiteral("star-filled") : QString());
}

// ---- Speech ----------------------------------------------------------------------------------

void MainWindow::onVoiceUnavailable(Language lang)
{
    if (lang == Language::Cantonese) {
        if (!m_voiceHintDismissed && m_voiceHint->isHidden()) {
            QString help = SpeechService::cantoneseVoiceHelpText().toHtmlEscaped();
            help.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
            m_voiceHint->setText(help);
            m_voiceHint->animateIn(false);
        } else {
            showStatus(tr("No Cantonese voice is installed - see Settings › Speech"), QStringLiteral("warning"));
        }
        return;
    }
    showStatus(tr("Read-aloud isn't available for English - see Settings › Speech"), QStringLiteral("warning"));
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
        const bool runtimeChange = !m_appliedTheme.isEmpty();
        m_appliedTheme = theme;
        if (theme != Theme::mode() || !Theme::isApplied()) {
            if (runtimeChange && isVisible()) {
                // Cross-fade from a snapshot of the old theme.
                motion::crossFade(this, motion::kSlow);
                if (m_settingsDialog && m_settingsDialog->isVisible())
                    motion::crossFade(m_settingsDialog, motion::kSlow);
            }
            Theme::apply(theme);
        }
    }
    // Only touch what changed: this also runs when tone/direction are saved.
    const int pt = m_settings->fontPointSize();
    const ChineseScript script = m_settings->script();
    const int display = (m_settings->showJyutping() ? 1 : 0) | (m_settings->showAlternatives() ? 2 : 0)
                        | (m_settings->showNotes() ? 4 : 0);
    if (pt != m_appliedFontSize || script != m_appliedScript) {
        m_appliedFontSize = pt;
        m_appliedScript = script;
        m_input->setTextPointSize(pt);
        m_input->setScript(script);
        m_result->setTextPointSize(pt);
    }
    if (display != m_appliedDisplay) {
        m_appliedDisplay = display;
        m_result->setDisplayOptions((display & 1) != 0, (display & 2) != 0, (display & 4) != 0);
    }
    updateProviderChip();
    updateWelcomeBanner(false);
    if (m_translation && m_translation->isActiveProviderConfigured() && !m_settings->firstRunCompleted())
        m_settings->setFirstRunCompleted(true);
}

QString MainWindow::providerChipText() const { return m_providerChip->text(); }

void MainWindow::updateProviderChip()
{
    auto *chip = static_cast<StatusChip *>(m_providerChip);
    const QString id = m_translation ? m_translation->activeProviderId() : QStringLiteral("claude");
    const bool configured = m_translation && m_translation->isActiveProviderConfigured();
    const QString model = m_translation ? m_translation->activeModel() : QString();
    // The model id already says which provider it is; keep the chip short.
    if (configured) {
        chip->setText(model.isEmpty() ? shortProviderName(id) : model);
        chip->setToolTip(ui::withShortcut(tr("Translating with %1 · %2 - click to change")
                                              .arg(shortProviderName(id), model.isEmpty() ? tr("default model") : model),
                                          keySettings()));
    } else {
        chip->setText(tr("Add API key"));
        chip->setToolTip(tr("No %1 API key yet - click to set one up").arg(shortProviderName(id)));
    }
    chip->setAccessibleName(chip->toolTip());
    chip->setWarning(!configured);
    chip->updateGeometry();
    if (QWidget *group = chip->parentWidget())
        group->updateGeometry();  // HeaderBar re-lays out on LayoutRequest
}

void MainWindow::updateWelcomeBanner(bool animated)
{
    const bool configured = m_translation && m_translation->isActiveProviderConfigured();
    if (configured || m_welcomeDismissed) {
        m_welcome->hide();
        return;
    }
    const bool firstRun = m_settings && !m_settings->firstRunCompleted();
    m_welcome->setTitle(firstRun ? tr("Welcome! Connect an AI provider to start") : tr("Add an API key to start translating"));
    m_welcome->setText(tr("Translations are written by Claude or OpenAI using your own API key. Get one in a minute - "
                          "<a href=\"%1\">Claude key</a> or <a href=\"%2\">OpenAI key</a> - then paste it in Settings. "
                          "It's stored encrypted on this PC.")
                           .arg(kClaudeKeysUrl, kOpenAiKeysUrl));
    if (m_welcome->isHidden()) {
        if (animated || !m_firstShow)
            m_welcome->animateIn(false);
        else
            m_welcome->show();
    }
}

// ---- Window ------------------------------------------------------------------------------------

void MainWindow::setHistoryVisible(bool visible, bool animated)
{
    const bool wasOpen = m_historySide->isOpen();
    m_historySide->setOpen(visible, animated && isVisible());
    m_historyButton->setChecked(visible);
    if (visible && !wasOpen) {
        m_historyPanel->refresh(false);
        if (animated && isVisible())
            m_historyPanel->list()->staggerIn();
    }
    UiPrefs::instance()->setHistoryVisible(visible);
    if (m_settings)
        m_settings->setHistoryVisible(visible);
    QTimer::singleShot(motion::ms(motion::kSlow) + 10, this, &MainWindow::updateLayoutForWidth);
}

bool MainWindow::isHistoryVisible() const { return m_historySide->isOpen(); }

void MainWindow::showAbout() { ui::showAboutDialog(this); }

void MainWindow::showShortcuts() { ui::showShortcutsDialog(this); }

void MainWindow::showStatus(const QString &message, const QString &iconName)
{
    ui::Toast::showMessage(this, message, iconName);
}

void MainWindow::updateLayoutForWidth()
{
    const int panesWidth = width() - 40 - (isHistoryVisible() ? kHistoryWidth : 0);
    m_panesLayout->setDirection(panesWidth < 700 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
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
        m_logo->setPixmap(ui::appLogo(28, devicePixelRatioF()));
        m_input->focusEditor();
        QTimer::singleShot(0, this, &MainWindow::updateLayoutForWidth);
        if (!m_welcome->isHidden()) {
            m_welcome->hide();
            QTimer::singleShot(motion::ms(250), this, [this] { m_welcome->animateIn(false); });
        }
        m_firstShow = false;
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
