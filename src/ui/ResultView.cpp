#include "ui/ResultView.h"

#include "ui/SpeechController.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace sct {

using ui::IconTone;

namespace {

// Soft jade disc with 粵 - the empty-state illustration.
class EmptyGlyph : public QWidget
{
public:
    explicit EmptyGlyph(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(72, 72);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const ThemeColors &c = Theme::colors();
        p.setPen(Qt::NoPen);
        p.setBrush(c.accentSoft);
        p.drawEllipse(rect().adjusted(1, 1, -1, -1));
        QFont f = Theme::textFont(Language::Cantonese, 10);
        f.setPixelSize(32);
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.setPen(c.accent);
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("粵"));
    }
};

QLabel *makeLabel(const QString &role, QWidget *parent, bool selectable = false)
{
    auto *l = new QLabel(parent);
    if (!role.isEmpty())
        l->setProperty("role", role);
    l->setWordWrap(true);
    l->setTextFormat(Qt::PlainText);
    if (selectable) {
        l->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        l->setCursor(Qt::IBeamCursor);
    }
    return l;
}

QToolButton *makeTextButton(const QString &iconName, const QString &text, const QString &toolTip, QWidget *parent)
{
    QToolButton *b = ui::makeIconButton(iconName, toolTip, parent);
    b->setText(text);
    b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    return b;
}

// Scale the headline down for longer texts so short phrases look bold and
// paragraphs stay readable.
qreal headlineScale(const QString &text, Language lang)
{
    const qsizetype n = text.size();
    if (lang == Language::Cantonese)
        return n <= 30 ? 1.7 : n <= 90 ? 1.4 : n <= 240 ? 1.2 : 1.08;
    return n <= 50 ? 1.55 : n <= 160 ? 1.3 : n <= 400 ? 1.15 : 1.05;
}

QString errorTitle(ErrorKind kind)
{
    switch (kind) {
    case ErrorKind::NotConfigured:
        return ResultView::tr("Set up an AI provider to start translating");
    case ErrorKind::Network:
        return ResultView::tr("Can't reach the AI service");
    case ErrorKind::Timeout:
        return ResultView::tr("The request timed out");
    case ErrorKind::Auth:
        return ResultView::tr("The API key was not accepted");
    case ErrorKind::RateLimited:
        return ResultView::tr("Too many requests right now");
    case ErrorKind::Server:
        return ResultView::tr("The AI service is having trouble");
    case ErrorKind::Refused:
        return ResultView::tr("The model declined to translate this");
    case ErrorKind::BadResponse:
        return ResultView::tr("The answer came back garbled");
    case ErrorKind::InvalidRequest:
        return ResultView::tr("The request was not accepted");
    case ErrorKind::Cancelled:
        return ResultView::tr("Translation cancelled");
    }
    return ResultView::tr("Something went wrong");
}

QString defaultErrorMessage(ErrorKind kind)
{
    switch (kind) {
    case ErrorKind::NotConfigured:
        return ResultView::tr("Add an API key for Claude or OpenAI in Settings.");
    case ErrorKind::Network:
        return ResultView::tr("Check your internet connection and try again.");
    case ErrorKind::Timeout:
        return ResultView::tr("The service took too long to answer. Try again, or choose \"Fast\" quality in Settings.");
    case ErrorKind::Auth:
        return ResultView::tr("Check the API key in Settings.");
    case ErrorKind::RateLimited:
        return ResultView::tr("Wait a moment, then try again.");
    case ErrorKind::Server:
        return ResultView::tr("The service is busy or temporarily unavailable. Try again in a moment.");
    case ErrorKind::Refused:
        return ResultView::tr("Try rephrasing the text.");
    case ErrorKind::BadResponse:
        return ResultView::tr("Please try again.");
    case ErrorKind::InvalidRequest:
        return ResultView::tr("The selected model may not be available to your account. Check Settings.");
    case ErrorKind::Cancelled:
        return {};
    }
    return {};
}

} // namespace

ResultView::ResultView(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("pane"));
    setFrameShape(QFrame::NoFrame);
    setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 12, 4, 8);
    layout->setSpacing(4);

    auto *header = new QHBoxLayout;
    header->setContentsMargins(16, 0, 14, 0);
    m_paneTitle = new QLabel(this);
    m_paneTitle->setProperty("role", QStringLiteral("paneTitle"));
    m_paneTitle->setFont(Theme::uiFont(-1, QFont::DemiBold));
    header->addWidget(m_paneTitle);
    header->addStretch(1);
    m_badge = new QLabel(this);
    m_badge->setProperty("role", QStringLiteral("muted"));
    m_badge->hide();
    header->addWidget(m_badge);
    layout->addLayout(header);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildEmptyPage());
    m_stack->addWidget(buildLoadingPage());
    m_stack->addWidget(buildResultPage());
    m_stack->addWidget(buildErrorPage());
    layout->addWidget(m_stack, 1);

    setDirection(Direction::EnglishToCantonese);
    applyFonts();
    setPage(Page::Empty);
}

// ---- Pages -------------------------------------------------------------------------

QWidget *ResultView::buildEmptyPage()
{
    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("emptyPage"));
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(24, 16, 24, 24);
    v->setSpacing(10);
    v->addStretch(2);

    auto *glyph = new EmptyGlyph(page);
    v->addWidget(glyph, 0, Qt::AlignHCenter);
    v->addSpacing(6);

    m_emptyTitle = new QLabel(tr("Your translation will appear here"), page);
    m_emptyTitle->setAlignment(Qt::AlignCenter);
    m_emptyTitle->setWordWrap(true);
    m_emptyTitle->setFont(Theme::uiFont(12, QFont::DemiBold));
    v->addWidget(m_emptyTitle);

    auto *hint = makeLabel(QStringLiteral("muted"), page);
    hint->setAlignment(Qt::AlignCenter);
    hint->setText(tr("Type or paste text, then press %1. You'll get natural Cantonese with Jyutping, "
                     "other ways to say it and usage notes.")
                      .arg(QKeySequence(Qt::CTRL | Qt::Key_Return).toString(QKeySequence::NativeText)));
    v->addWidget(hint);
    v->addSpacing(8);

    auto *tryLabel = makeLabel(QStringLiteral("caption"), page);
    tryLabel->setAlignment(Qt::AlignCenter);
    tryLabel->setText(tr("Try an example"));
    v->addWidget(tryLabel);

    m_examplesRow = new QHBoxLayout;
    m_examplesRow->setSpacing(8);
    v->addLayout(m_examplesRow);
    v->addStretch(3);
    return page;
}

QWidget *ResultView::buildLoadingPage()
{
    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("loadingPage"));
    auto *v = new QVBoxLayout(page);
    v->addStretch(1);
    auto *spinner = new ui::BusyIndicator(36, page);
    v->addWidget(spinner, 0, Qt::AlignHCenter);
    v->addSpacing(8);
    auto *label = new QLabel(tr("Translating…"), page);
    label->setFont(Theme::uiFont(11.5, QFont::DemiBold));
    label->setAlignment(Qt::AlignCenter);
    v->addWidget(label);
    auto *sub = makeLabel(QStringLiteral("muted"), page);
    sub->setAlignment(Qt::AlignCenter);
    sub->setText(tr("Finding the most natural way to say it"));
    v->addWidget(sub);
    v->addSpacing(8);
    m_cancel = new QPushButton(tr("Cancel"), page);
    m_cancel->setToolTip(ui::withShortcut(tr("Cancel translation"), QKeySequence(Qt::Key_Escape)));
    m_cancel->setCursor(Qt::PointingHandCursor);
    connect(m_cancel, &QPushButton::clicked, this, &ResultView::cancelRequested);
    v->addWidget(m_cancel, 0, Qt::AlignHCenter);
    v->addStretch(2);
    return page;
}

QWidget *ResultView::buildResultPage()
{
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *content = new QWidget(m_scroll);
    content->setObjectName(QStringLiteral("resultContent"));
    auto *v = new QVBoxLayout(content);
    v->setContentsMargins(16, 6, 16, 12);
    v->setSpacing(6);

    m_translation = makeLabel(QString(), content, true);
    m_translation->setObjectName(QStringLiteral("translationText"));
    m_translation->setAccessibleName(tr("Translation"));
    v->addWidget(m_translation);

    m_jyutpingCaption = makeLabel(QStringLiteral("caption"), content);
    m_jyutpingCaption->setText(tr("Jyutping of your text"));
    v->addWidget(m_jyutpingCaption);

    m_jyutping = makeLabel(QStringLiteral("jyutping"), content, true);
    m_jyutping->setObjectName(QStringLiteral("jyutpingText"));
    m_jyutping->setAccessibleName(tr("Jyutping pronunciation"));
    v->addWidget(m_jyutping);

    m_literal = makeLabel(QStringLiteral("muted"), content, true);
    m_literal->setObjectName(QStringLiteral("literalText"));
    v->addWidget(m_literal);

    v->addSpacing(4);
    auto *actions = new QHBoxLayout;
    actions->setSpacing(4);
    m_speak = new ui::SpeakButton(content, true);
    m_speak->setIdleToolTip(ui::withShortcut(tr("Listen"), QKeySequence(Qt::CTRL | Qt::Key_R)));
    actions->addWidget(m_speak);
    m_copy = makeTextButton(QStringLiteral("copy"), tr("Copy"),
                            ui::withShortcut(tr("Copy translation"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C)),
                            content);
    connect(m_copy, &QToolButton::clicked, this, &ResultView::copyTranslation);
    actions->addWidget(m_copy);
    m_copyJyutping = ui::makeIconButton(QStringLiteral("copy-jyutping"), tr("Copy with Jyutping"), content);
    connect(m_copyJyutping, &QToolButton::clicked, this, &ResultView::copyWithJyutping);
    actions->addWidget(m_copyJyutping);
    m_star = ui::makeIconButton(QStringLiteral("star"), tr("Star - keep in history"), content);
    m_star->setObjectName(QStringLiteral("starButton"));
    m_star->setCheckable(true);
    connect(m_star, &QToolButton::toggled, this, [this](bool on) {
        updateStarButton();
        emit starToggled(on);
    });
    actions->addWidget(m_star);
    actions->addStretch(1);
    v->addLayout(actions);

    m_altSection = new ui::CollapsibleSection(tr("Other ways to say it"), QStringLiteral("sparkles"), content);
    v->addSpacing(6);
    v->addWidget(m_altSection);

    m_notesSection = new ui::CollapsibleSection(tr("Notes"), QStringLiteral("bulb"), content);
    v->addSpacing(2);
    v->addWidget(m_notesSection);

    v->addStretch(1);
    m_footer = makeLabel(QStringLiteral("caption"), content);
    m_footer->setObjectName(QStringLiteral("resultFooter"));
    v->addSpacing(8);
    v->addWidget(m_footer);

    m_scroll->setWidget(content);
    return m_scroll;
}

QWidget *ResultView::buildErrorPage()
{
    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("errorPage"));
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(14, 8, 14, 14);
    m_errorBanner = new ui::Banner(ui::Banner::Kind::Error, page);
    m_errorBanner->setObjectName(QStringLiteral("errorBanner"));
    v->addWidget(m_errorBanner);
    v->addStretch(1);
    return page;
}

void ResultView::setPage(Page page)
{
    m_page = page;
    m_stack->setCurrentIndex(int(page));
    if (page != Page::Result)
        m_badge->hide();
}

// ---- Public API --------------------------------------------------------------------

void ResultView::setSpeechController(SpeechController *controller)
{
    m_speech = controller;
    if (m_speech) {
        m_speech->attach(m_speak, [this] {
            return SpeechController::Utterance{m_result.translation, targetLanguage(m_result.request.direction)};
        });
    }
    m_speak->setEnabled(m_speech != nullptr);
}

void ResultView::setDirection(Direction direction)
{
    m_direction = direction;
    m_paneTitle->setText(Theme::languageLabel(targetLanguage(direction)));
    rebuildExamples();
}

void ResultView::setDisplayOptions(bool showJyutping, bool showAlternatives, bool showNotes)
{
    m_showJyutping = showJyutping;
    m_showAlternatives = showAlternatives;
    m_showNotes = showNotes;
    applyVisibility();
}

void ResultView::setTextPointSize(int pointSize)
{
    m_pointSize = qBound(8, pointSize, 40);
    applyFonts();
}

void ResultView::showEmpty()
{
    m_result = TranslationResult();
    setPage(Page::Empty);
}

void ResultView::showLoading() { setPage(Page::Loading); }

void ResultView::showResult(const TranslationResult &result, bool fromHistory)
{
    m_result = result;
    m_fromHistory = fromHistory;
    if (!result.isValid()) {
        showEmpty();
        return;
    }
    m_paneTitle->setText(Theme::languageLabel(targetLanguage(result.request.direction)));

    m_translation->setText(result.translation.trimmed());
    m_jyutping->setText(result.jyutping.trimmed());
    m_literal->setText(result.literal.trimmed().isEmpty()
                           ? QString()
                           : tr("Literally: %1").arg(result.literal.trimmed()));
    m_jyutpingCaption->setVisible(false);

    {
        const QSignalBlocker block(m_star);
        m_star->setChecked(false);
    }
    updateStarButton();
    rebuildAlternatives();
    rebuildNotes();
    updateFooter();
    applyFonts();
    applyVisibility();
    m_scroll->verticalScrollBar()->setValue(0);
    setPage(Page::Result);

    if (result.fromCache || fromHistory) {
        m_badge->setText(fromHistory ? tr("From history") : tr("From cache"));
        m_badge->show();
    }
}

void ResultView::showError(const TranslationError &error)
{
    m_errorBanner->setTitle(errorTitle(error.kind));
    m_errorBanner->setText(
        (error.message.trimmed().isEmpty() ? defaultErrorMessage(error.kind) : error.message.trimmed()).toHtmlEscaped());
    QString details = error.detail.trimmed();
    if (error.httpStatus > 0)
        details = tr("HTTP %1").arg(error.httpStatus) + (details.isEmpty() ? QString() : QStringLiteral("\n") + details);
    m_errorBanner->setDetails(details);
    m_errorBanner->clearButtons();
    switch (error.kind) {
    case ErrorKind::NotConfigured:
    case ErrorKind::Auth:
    case ErrorKind::InvalidRequest: {
        QPushButton *b = m_errorBanner->addButton(tr("Open Settings"), true);
        b->setObjectName(QStringLiteral("openSettingsButton"));
        connect(b, &QPushButton::clicked, this, &ResultView::openSettingsRequested);
        break;
    }
    case ErrorKind::RateLimited:
    case ErrorKind::Server:
    case ErrorKind::Network:
    case ErrorKind::Timeout:
    case ErrorKind::BadResponse: {
        QPushButton *b = m_errorBanner->addButton(tr("Retry"), true);
        b->setObjectName(QStringLiteral("retryButton"));
        b->setIcon(ui::icon(QStringLiteral("retry"), IconTone::OnAccent));
        connect(b, &QPushButton::clicked, this, &ResultView::retryRequested);
        break;
    }
    case ErrorKind::Refused:
    case ErrorKind::Cancelled:
        break;
    }
    m_errorBanner->show();
    setPage(Page::Error);
}

void ResultView::setStarred(bool starred)
{
    const QSignalBlocker block(m_star);
    m_star->setChecked(starred);
    updateStarButton();
}

QString ResultView::translationText() const { return m_translation->text(); }
QString ResultView::jyutpingText() const { return m_jyutping->text(); }
QString ResultView::literalText() const { return m_literal->text(); }
bool ResultView::isJyutpingShown() const { return !m_jyutping->isHidden(); }
int ResultView::alternativeCount() const { return int(m_altCards.size()); }
int ResultView::noteCount() const { return int(m_noteLabels.size()); }
bool ResultView::isStarred() const { return m_star->isChecked(); }

void ResultView::copyTranslation()
{
    if (!m_result.isValid())
        return;
    QApplication::clipboard()->setText(m_result.translation.trimmed());
    emit statusMessage(tr("Copied translation"));
}

void ResultView::copyWithJyutping()
{
    if (!m_result.isValid())
        return;
    QStringList parts;
    if (m_result.request.direction == Direction::CantoneseToEnglish) {
        parts << m_result.request.text.trimmed() << m_result.jyutping.trimmed() << m_result.translation.trimmed();
    } else {
        parts << m_result.translation.trimmed() << m_result.jyutping.trimmed();
    }
    parts.removeAll(QString());
    QApplication::clipboard()->setText(parts.join(QLatin1Char('\n')));
    emit statusMessage(tr("Copied with Jyutping"));
}

// ---- Internals ---------------------------------------------------------------------

void ResultView::rebuildExamples()
{
    qDeleteAll(m_exampleButtons);
    m_exampleButtons.clear();
    while (m_examplesRow->count() > 0)
        delete m_examplesRow->takeAt(0);

    const QStringList examples = m_direction == Direction::EnglishToCantonese
                                     ? QStringList{tr("Long time no see!"), tr("How much is this?"),
                                                   tr("Could you speak a bit slower?")}
                                     : QStringList{QStringLiteral("唔該晒！"), QStringLiteral("你食咗飯未呀？"),
                                                   QStringLiteral("今日好熱呀")};
    const Language src = sourceLanguage(m_direction);
    m_examplesRow->addStretch(1);
    for (const QString &text : examples) {
        auto *b = new QPushButton(text, m_examplesRow->parentWidget());
        b->setProperty("chip", true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFont(Theme::textFont(src, 10.5));
        b->setFocusPolicy(Qt::TabFocus);
        connect(b, &QPushButton::clicked, this, [this, text] { emit exampleChosen(text); });
        m_examplesRow->addWidget(b);
        m_exampleButtons.append(b);
    }
    m_examplesRow->addStretch(1);
}

void ResultView::rebuildAlternatives()
{
    QVBoxLayout *layout = m_altSection->contentLayout();
    qDeleteAll(m_altCards);
    m_altCards.clear();
    m_altTextLabels.clear();
    m_altJyutpingLabels.clear();

    const Language target = targetLanguage(m_result.request.direction);
    for (const Alternative &alt : m_result.alternatives) {
        if (alt.text.trimmed().isEmpty())
            continue;
        auto *card = new QFrame(m_altSection->content());
        card->setObjectName(QStringLiteral("altCard"));
        card->setAttribute(Qt::WA_StyledBackground, true);
        auto *h = new QHBoxLayout(card);
        h->setContentsMargins(14, 10, 8, 10);
        h->setSpacing(8);

        auto *texts = new QVBoxLayout;
        texts->setSpacing(2);
        QLabel *text = makeLabel(QString(), card, true);
        text->setText(alt.text.trimmed());
        texts->addWidget(text);
        m_altTextLabels.append(text);
        QLabel *jp = makeLabel(QStringLiteral("jyutping"), card, true);
        jp->setText(alt.jyutping.trimmed());
        jp->setVisible(!alt.jyutping.trimmed().isEmpty());
        texts->addWidget(jp);
        m_altJyutpingLabels.append(jp);
        if (!alt.note.trimmed().isEmpty()) {
            QLabel *note = makeLabel(QStringLiteral("muted"), card, true);
            note->setText(alt.note.trimmed());
            texts->addSpacing(2);
            texts->addWidget(note);
        }
        h->addLayout(texts, 1);

        auto *speak = new ui::SpeakButton(card);
        speak->setIdleToolTip(tr("Listen to this one"));
        if (m_speech) {
            const QString altText = alt.text.trimmed();
            m_speech->attach(speak, [altText, target] { return SpeechController::Utterance{altText, target}; });
        } else {
            speak->setEnabled(false);
        }
        h->addWidget(speak, 0, Qt::AlignTop);
        QToolButton *copy = ui::makeIconButton(QStringLiteral("copy"), tr("Copy"), card);
        const QString altText = alt.text.trimmed();
        connect(copy, &QToolButton::clicked, this, [this, altText] {
            QApplication::clipboard()->setText(altText);
            emit statusMessage(tr("Copied"));
        });
        h->addWidget(copy, 0, Qt::AlignTop);

        layout->addWidget(card);
        m_altCards.append(card);
    }
    m_altSection->setCount(int(m_altCards.size()));
}

void ResultView::rebuildNotes()
{
    QVBoxLayout *layout = m_notesSection->contentLayout();
    m_noteLabels.clear();
    while (layout->count() > 0) {
        QLayoutItem *item = layout->takeAt(0);
        delete item->widget();
        delete item;
    }

    for (const QString &note : m_result.notes) {
        if (note.trimmed().isEmpty())
            continue;
        auto *row = new QWidget(m_notesSection->content());
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(4, 0, 0, 0);
        h->setSpacing(8);
        auto *bullet = new QLabel(QStringLiteral("•"), row);
        bullet->setProperty("role", QStringLiteral("jyutping"));
        h->addWidget(bullet, 0, Qt::AlignTop);
        QLabel *text = makeLabel(QString(), row, true);
        text->setText(note.trimmed());
        h->addWidget(text, 1);
        layout->addWidget(row);
        m_noteLabels.append(text);
    }
    m_notesSection->setCount(int(m_noteLabels.size()));
}

void ResultView::applyFonts()
{
    const Language target = targetLanguage(m_result.isValid() ? m_result.request.direction : m_direction);
    const ChineseScript script = m_result.request.script;
    const qreal base = m_pointSize;
    m_translation->setFont(
        Theme::textFont(target, base * headlineScale(m_translation->text(), target), script));
    m_jyutping->setFont(Theme::jyutpingFont(base * 0.95));
    m_literal->setFont(Theme::uiFont(base * 0.88));
    m_jyutpingCaption->setFont(Theme::uiFont(base * 0.75));
    for (QLabel *l : std::as_const(m_altTextLabels))
        l->setFont(Theme::textFont(target, base * 1.12, script));
    for (QLabel *l : std::as_const(m_altJyutpingLabels))
        l->setFont(Theme::jyutpingFont(base * 0.85));
    for (QLabel *l : std::as_const(m_noteLabels))
        l->setFont(Theme::textFont(Language::English, base * 0.9, script));
    for (QWidget *card : std::as_const(m_altCards)) {
        for (QLabel *l : card->findChildren<QLabel *>()) {
            if (l->property("role").toString() == QLatin1String("muted"))
                l->setFont(Theme::uiFont(base * 0.82));
        }
    }
}

void ResultView::applyVisibility()
{
    const bool hasJyutping = !m_result.jyutping.trimmed().isEmpty();
    const bool yueSource = m_result.request.direction == Direction::CantoneseToEnglish;
    m_jyutping->setVisible(m_showJyutping && hasJyutping);
    m_jyutpingCaption->setVisible(m_showJyutping && hasJyutping && yueSource);
    m_copyJyutping->setVisible(m_showJyutping && hasJyutping);
    m_literal->setVisible(!m_literal->text().isEmpty());
    m_altSection->setVisible(m_showAlternatives && !m_altCards.isEmpty());
    m_notesSection->setVisible(m_showNotes && !m_noteLabels.isEmpty());
}

void ResultView::updateStarButton()
{
    const bool on = m_star->isChecked();
    m_star->setIcon(ui::icon(on ? QStringLiteral("star-filled") : QStringLiteral("star"),
                             on ? IconTone::Star : IconTone::Text));
    m_star->setToolTip(on ? tr("Starred - click to unstar") : tr("Star - keep in history"));
}

void ResultView::updateFooter()
{
    QString provider = m_result.providerId;
    if (provider == QLatin1String("claude"))
        provider = QStringLiteral("Claude");
    else if (provider == QLatin1String("openai"))
        provider = QStringLiteral("OpenAI");
    QStringList parts;
    if (!provider.isEmpty())
        parts << (m_result.model.isEmpty() ? provider : QStringLiteral("%1 · %2").arg(provider, m_result.model));
    if (m_result.timestamp.isValid() && m_fromHistory)
        parts << QLocale().toString(m_result.timestamp.toLocalTime(), QLocale::ShortFormat);
    m_footer->setText(parts.isEmpty() ? QString() : tr("Translated by %1").arg(parts.join(QStringLiteral(" · "))));
    m_footer->setVisible(!parts.isEmpty());
}

void ResultView::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
        updateStarButton();
}

} // namespace sct
