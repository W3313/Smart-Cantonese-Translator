#include "ui/ResultView.h"

#include "ui/Motion.h"
#include "ui/SpeechController.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace sct {

using ui::IconTone;

namespace {

constexpr int kRevealSlide = 14;  // px the result content rises while fading in
constexpr int kContentTop = 6;

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

// Big for short phrases, calmer for paragraphs.
qreal headlineScale(const QString &text, Language lang)
{
    const qsizetype n = text.size();
    if (lang == Language::Cantonese)
        return n <= 24 ? 1.9 : n <= 80 ? 1.6 : n <= 220 ? 1.3 : 1.12;
    return n <= 40 ? 1.7 : n <= 140 ? 1.42 : n <= 400 ? 1.18 : 1.06;
}

QString errorTitle(ErrorKind kind)
{
    switch (kind) {
    case ErrorKind::NotConfigured:
        return ResultView::tr("Add an API key to start translating");
    case ErrorKind::Network:
        return ResultView::tr("Can't reach the AI service");
    case ErrorKind::Timeout:
        return ResultView::tr("The request timed out");
    case ErrorKind::Auth:
        return ResultView::tr("The API key wasn't accepted");
    case ErrorKind::RateLimited:
        return ResultView::tr("Too many requests right now");
    case ErrorKind::Server:
        return ResultView::tr("The AI service is having trouble");
    case ErrorKind::Refused:
        return ResultView::tr("The model declined to translate this");
    case ErrorKind::BadResponse:
        return ResultView::tr("The answer came back garbled");
    case ErrorKind::InvalidRequest:
        return ResultView::tr("The request wasn't accepted");
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

QString shortProvider(const QString &id)
{
    if (id == QLatin1String("claude"))
        return QStringLiteral("Claude");
    if (id == QLatin1String("openai"))
        return QStringLiteral("OpenAI");
    return id;
}

// Rounded tinted card for one alternative phrasing.
class AltCard : public QFrame
{
public:
    explicit AltCard(QWidget *parent)
        : QFrame(parent)
    {
        setAttribute(Qt::WA_Hover);
    }

protected:
    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::HoverEnter || e->type() == QEvent::HoverLeave) {
            const bool on = e->type() == QEvent::HoverEnter;
            motion::animate(this, QStringLiteral("hover"), m_hover, on ? 1.0 : 0.0, motion::kFast,
                            [this](const QVariant &v) {
                                m_hover = v.toReal();
                                update();
                            });
        }
        return QFrame::event(e);
    }

    void paintEvent(QPaintEvent *) override
    {
        const ThemeColors &c = Theme::colors();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
        p.fillPath(path, c.surfaceAlt);
        p.setPen(QPen(ui::withAlpha(c.border, m_hover), 1));
        p.drawPath(path);
    }

private:
    qreal m_hover = 0.0;
};

} // namespace

ResultView::ResultView(QWidget *parent)
    : ui::Card(parent)
{
    setObjectName(QStringLiteral("resultCard"));
    auto *layout = new QVBoxLayout(this);
    const QMargins sm = shadowMargins();
    layout->setContentsMargins(sm.left() + 6, sm.top() + 12, sm.right() + 6, sm.bottom() + 10);
    layout->setSpacing(0);

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
    v->setSpacing(8);
    v->addStretch(3);

    auto *title = new QLabel(tr("Your translation will appear here"), page);
    title->setObjectName(QStringLiteral("emptyTitle"));
    title->setProperty("role", QStringLiteral("muted"));
    title->setAlignment(Qt::AlignCenter);
    title->setWordWrap(true);
    title->setFont(Theme::uiFont(13, QFont::DemiBold));
    v->addWidget(title);

    auto *hint = makeLabel(QStringLiteral("caption"), page);
    hint->setAlignment(Qt::AlignCenter);
    hint->setFont(Theme::uiFont(9.5));
    hint->setText(tr("Natural Cantonese with Jyutping, other ways to say it and usage notes."));
    v->addWidget(hint);
    v->addSpacing(14);

    m_examplesRow = new QHBoxLayout;
    m_examplesRow->setSpacing(8);
    v->addLayout(m_examplesRow);
    v->addStretch(4);
    return page;
}

QWidget *ResultView::buildLoadingPage()
{
    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("loadingPage"));
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(16, kContentTop + 6, 16, 12);
    auto *skeleton = new ui::SkeletonView(page);
    skeleton->setObjectName(QStringLiteral("skeleton"));
    v->addWidget(skeleton, 1);
    return page;
}

QWidget *ResultView::buildResultPage()
{
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->viewport()->setAutoFillBackground(false);

    auto *content = new QWidget(m_scroll);
    content->setObjectName(QStringLiteral("resultContent"));
    content->setAutoFillBackground(false);
    m_contentLayout = new QVBoxLayout(content);
    m_contentLayout->setContentsMargins(16, kContentTop, 16, 8);
    m_contentLayout->setSpacing(0);

    m_translation = makeLabel(QString(), content, true);
    m_translation->setObjectName(QStringLiteral("translationText"));
    m_translation->setAccessibleName(tr("Translation"));
    m_contentLayout->addWidget(m_translation);
    m_contentLayout->addSpacing(8);

    m_jyutpingCaption = makeLabel(QStringLiteral("caption"), content);
    m_jyutpingCaption->setText(tr("Pronunciation of your text"));
    m_contentLayout->addWidget(m_jyutpingCaption);

    m_jyutping = makeLabel(QStringLiteral("jyutping"), content, true);
    m_jyutping->setObjectName(QStringLiteral("jyutpingText"));
    m_jyutping->setAccessibleName(tr("Jyutping pronunciation"));
    m_contentLayout->addWidget(m_jyutping);

    m_literal = makeLabel(QStringLiteral("muted"), content, true);
    m_literal->setObjectName(QStringLiteral("literalText"));
    m_literal->setContentsMargins(0, 6, 0, 0);
    m_contentLayout->addWidget(m_literal);

    m_contentLayout->addSpacing(14);
    m_actions = new QWidget(content);
    auto *actions = new QHBoxLayout(m_actions);
    actions->setContentsMargins(0, 0, 0, 0);
    actions->setSpacing(4);
    m_speak = new ui::SpeakButton(m_actions, true);
    m_speak->setObjectName(QStringLiteral("speakResultButton"));
    m_speak->setIdleToolTip(ui::withShortcut(tr("Listen"), QKeySequence(Qt::CTRL | Qt::Key_R)));
    actions->addWidget(m_speak);
    m_copy = new ui::IconButton(QStringLiteral("copy"),
                                ui::withShortcut(tr("Copy translation"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C)),
                                m_actions);
    m_copy->setObjectName(QStringLiteral("copyButton"));
    m_copy->setText(tr("Copy"));
    connect(m_copy, &QAbstractButton::clicked, this, [this] {
        copyTranslation();
        flashCopied(m_copy);
    });
    actions->addWidget(m_copy);
    m_copyJyutping = new ui::IconButton(QStringLiteral("copy-jyutping"), tr("Copy with Jyutping"), m_actions,
                                        IconTone::Muted);
    m_copyJyutping->setObjectName(QStringLiteral("copyJyutpingButton"));
    connect(m_copyJyutping, &QAbstractButton::clicked, this, [this] {
        copyWithJyutping();
        flashCopied(m_copyJyutping);
    });
    actions->addWidget(m_copyJyutping);
    m_star = new ui::IconButton(QStringLiteral("star"), tr("Star"), m_actions, IconTone::Muted);
    m_star->setObjectName(QStringLiteral("starButton"));
    m_star->setCheckable(true);
    m_star->setCheckedBackground(false);
    connect(m_star, &QAbstractButton::toggled, this, [this](bool on) {
        updateStarButton();
        emit starToggled(on);
    });
    actions->addWidget(m_star);
    actions->addStretch(1);
    addRevealWidget(m_copyJyutping);
    addRevealWidget(m_star);
    m_contentLayout->addWidget(m_actions);

    m_contentLayout->addSpacing(14);
    m_altSection = new ui::Disclosure(content);
    m_altSection->setObjectName(QStringLiteral("alternativesSection"));
    m_contentLayout->addWidget(m_altSection);
    m_notesSection = new ui::Disclosure(content);
    m_notesSection->setObjectName(QStringLiteral("notesSection"));
    m_contentLayout->addWidget(m_notesSection);
    connect(m_altSection, &ui::Disclosure::expandedChanged, this, [this](bool on) { m_altExpanded = on; });
    connect(m_notesSection, &ui::Disclosure::expandedChanged, this, [this](bool on) { m_notesExpanded = on; });

    m_contentLayout->addStretch(1);
    m_footer = makeLabel(QStringLiteral("caption"), content);
    m_footer->setObjectName(QStringLiteral("resultFooter"));
    m_footer->setFont(Theme::uiFont(8.5));
    m_footer->setContentsMargins(0, 12, 0, 0);
    m_contentLayout->addWidget(m_footer);

    m_scroll->setWidget(content);
    content->setAutoFillBackground(false);  // setWidget() switches it on
    return m_scroll;
}

QWidget *ResultView::buildErrorPage()
{
    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("errorPage"));
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(10, 10, 10, 10);
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
    if (m_page != Page::Empty) {
        motion::crossFade(m_stack, motion::kNormal);
        setPage(Page::Empty);
    }
}

void ResultView::showLoading() { setPage(Page::Loading); }

void ResultView::showResult(const TranslationResult &result, bool fromHistory, bool animated)
{
    m_result = result;
    m_fromHistory = fromHistory;
    if (!result.isValid()) {
        showEmpty();
        return;
    }
    m_translation->setText(result.translation.trimmed());
    m_jyutping->setText(result.jyutping.trimmed());
    m_literal->setText(result.literal.trimmed().isEmpty() ? QString()
                                                          : tr("Literally: %1").arg(result.literal.trimmed()));
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
    if (animated)
        reveal();
}

void ResultView::reveal()
{
    if (motion::reduced())
        return;
    // Content rises a little while the pieces fade in one after another.
    QPointer<QVBoxLayout> layout(m_contentLayout);
    motion::animate(
        m_scroll, QStringLiteral("rise"), kContentTop + kRevealSlide, kContentTop, motion::kSlow,
        [layout](const QVariant &v) {
            if (layout) {
                const QMargins m = layout->contentsMargins();
                layout->setContentsMargins(m.left(), v.toInt(), m.right(), m.bottom());
            }
        });
    const QList<QWidget *> pieces = {m_translation, m_jyutpingCaption, m_jyutping, m_literal,
                                     m_actions,     m_altSection,      m_notesSection, m_footer};
    int delay = 0;
    for (QWidget *w : pieces) {
        if (w->isHidden())
            continue;
        motion::fadeIn(w, motion::kSlow, delay);
        delay += (w == m_translation) ? 80 : 45;
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
        ui::Button *b = m_errorBanner->addButton(tr("Open Settings"), true);
        b->setObjectName(QStringLiteral("openSettingsButton"));
        connect(b, &QAbstractButton::clicked, this, &ResultView::openSettingsRequested);
        break;
    }
    case ErrorKind::RateLimited:
    case ErrorKind::Server:
    case ErrorKind::Network:
    case ErrorKind::Timeout:
    case ErrorKind::BadResponse: {
        ui::Button *b = m_errorBanner->addButton(tr("Retry"), true);
        b->setObjectName(QStringLiteral("retryButton"));
        connect(b, &QAbstractButton::clicked, this, &ResultView::retryRequested);
        break;
    }
    case ErrorKind::Refused:
    case ErrorKind::Cancelled:
        break;
    }
    setPage(Page::Error);
    // Only real failures shake; "set up a key" is guidance, not an error.
    m_errorBanner->animateIn(error.kind != ErrorKind::NotConfigured);
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
    emit statusMessage(tr("Copied translation"), QStringLiteral("check"));
}

void ResultView::copyWithJyutping()
{
    if (!m_result.isValid())
        return;
    QStringList parts;
    if (m_result.request.direction == Direction::CantoneseToEnglish)
        parts << m_result.request.text.trimmed() << m_result.jyutping.trimmed() << m_result.translation.trimmed();
    else
        parts << m_result.translation.trimmed() << m_result.jyutping.trimmed();
    parts.removeAll(QString());
    QApplication::clipboard()->setText(parts.join(QLatin1Char('\n')));
    emit statusMessage(tr("Copied with Jyutping"), QStringLiteral("check"));
}

// Briefly swap a copy button's icon for a check mark.
void ResultView::flashCopied(ui::IconButton *button)
{
    const QString original = button->iconName();
    if (original == QLatin1String("check"))
        return;
    const IconTone tone = button == m_copy ? IconTone::Text : IconTone::Muted;
    button->setIconName(QStringLiteral("check"), IconTone::Success);
    QPointer<ui::IconButton> guard(button);
    QTimer::singleShot(1300, button, [guard, original, tone] {
        if (guard)
            guard->setIconName(original, tone);
    });
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
        auto *b = new ui::Button(text, ui::Button::Variant::Secondary, m_stack->widget(int(Page::Empty)));
        QFont f = Theme::textFont(src, 9.5);
        f.setWeight(QFont::Normal);
        b->setFont(f);
        b->setToolTip(tr("Translate this example"));
        connect(b, &QAbstractButton::clicked, this, [this, text] { emit exampleChosen(text); });
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
    m_altNoteLabels.clear();

    const Language target = targetLanguage(m_result.request.direction);
    for (const Alternative &alt : m_result.alternatives) {
        const QString altText = alt.text.trimmed();
        if (altText.isEmpty())
            continue;
        auto *card = new AltCard(m_altSection->body());
        card->setObjectName(QStringLiteral("altCard"));
        auto *h = new QHBoxLayout(card);
        h->setContentsMargins(14, 10, 8, 10);
        h->setSpacing(6);

        auto *texts = new QVBoxLayout;
        texts->setSpacing(2);
        QLabel *text = makeLabel(QString(), card, true);
        text->setText(altText);
        texts->addWidget(text);
        m_altTextLabels.append(text);
        QLabel *jp = makeLabel(QStringLiteral("jyutping"), card, true);
        jp->setText(alt.jyutping.trimmed());
        jp->setVisible(!alt.jyutping.trimmed().isEmpty() && m_showJyutping);
        texts->addWidget(jp);
        m_altJyutpingLabels.append(jp);
        if (!alt.note.trimmed().isEmpty()) {
            QLabel *note = makeLabel(QStringLiteral("muted"), card, true);
            note->setText(alt.note.trimmed());
            note->setContentsMargins(0, 4, 0, 0);
            texts->addWidget(note);
            m_altNoteLabels.append(note);
        }
        h->addLayout(texts, 1);

        auto *speak = new ui::SpeakButton(card);
        speak->setIdleToolTip(tr("Listen to this one"));
        if (m_speech)
            m_speech->attach(speak, [altText, target] { return SpeechController::Utterance{altText, target}; });
        else
            speak->setEnabled(false);
        h->addWidget(speak, 0, Qt::AlignTop);
        auto *copy = new ui::IconButton(QStringLiteral("copy"), tr("Copy"), card, IconTone::Muted);
        connect(copy, &QAbstractButton::clicked, this, [this, altText, copy] {
            QApplication::clipboard()->setText(altText);
            emit statusMessage(tr("Copied"), QStringLiteral("check"));
            flashCopied(copy);
        });
        h->addWidget(copy, 0, Qt::AlignTop);

        layout->addWidget(card);
        m_altCards.append(card);
    }
    const int n = int(m_altCards.size());
    m_altSection->setTitle(n == 1 ? tr("1 other way to say it") : tr("%1 other ways to say it").arg(n));
    {
        const bool keep = m_altExpanded;  // setExpanded() reports back through expandedChanged
        m_altSection->setExpanded(keep, false);
        m_altExpanded = keep;
    }
    m_altSection->contentChanged();
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
        auto *row = new QWidget(m_notesSection->body());
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(8, 0, 0, 0);
        h->setSpacing(10);
        auto *bullet = new QLabel(QStringLiteral("•"), row);
        bullet->setProperty("role", QStringLiteral("jyutping"));
        h->addWidget(bullet, 0, Qt::AlignTop);
        QLabel *text = makeLabel(QString(), row, true);
        text->setText(note.trimmed());
        h->addWidget(text, 1);
        layout->addWidget(row);
        m_noteLabels.append(text);
    }
    const int n = int(m_noteLabels.size());
    m_notesSection->setTitle(n == 1 ? tr("1 usage note") : tr("%1 usage notes").arg(n));
    {
        const bool keep = m_notesExpanded;
        m_notesSection->setExpanded(keep, false);
        m_notesExpanded = keep;
    }
    m_notesSection->contentChanged();
}

void ResultView::applyFonts()
{
    const Language target = targetLanguage(m_result.isValid() ? m_result.request.direction : m_direction);
    const ChineseScript script = m_result.request.script;
    const qreal base = m_pointSize;
    QFont headline = Theme::textFont(target, base * headlineScale(m_translation->text(), target), script);
    headline.setWeight(target == Language::Cantonese ? QFont::Medium : QFont::Normal);
    m_translation->setFont(headline);
    m_jyutping->setFont(Theme::jyutpingFont(base * 0.95));
    m_literal->setFont(Theme::uiFont(base * 0.8));
    m_jyutpingCaption->setFont(Theme::uiFont(base * 0.68));
    for (QLabel *l : std::as_const(m_altTextLabels))
        l->setFont(Theme::textFont(target, base * 1.12, script));
    for (QLabel *l : std::as_const(m_altJyutpingLabels))
        l->setFont(Theme::jyutpingFont(base * 0.82));
    for (QLabel *l : std::as_const(m_altNoteLabels))
        l->setFont(Theme::uiFont(base * 0.74));
    for (QLabel *l : std::as_const(m_noteLabels))
        l->setFont(Theme::uiFont(base * 0.8));
    m_altSection->contentChanged();
    m_notesSection->contentChanged();
}

void ResultView::applyVisibility()
{
    const bool hasJyutping = !m_result.jyutping.trimmed().isEmpty();
    const bool yueSource = m_result.request.direction == Direction::CantoneseToEnglish;
    m_jyutping->setVisible(m_showJyutping && hasJyutping);
    m_jyutpingCaption->setVisible(m_showJyutping && hasJyutping && yueSource);
    m_copyJyutping->setVisible(m_showJyutping && hasJyutping);
    m_literal->setVisible(!m_literal->text().isEmpty());
    for (QLabel *jp : std::as_const(m_altJyutpingLabels))
        jp->setVisible(m_showJyutping && !jp->text().isEmpty());
    m_altSection->setVisible(m_showAlternatives && !m_altCards.isEmpty());
    m_notesSection->setVisible(m_showNotes && !m_noteLabels.isEmpty());
    m_altSection->contentChanged();
}

void ResultView::updateStarButton()
{
    const bool on = m_star->isChecked();
    m_star->setIconName(on ? QStringLiteral("star-filled") : QStringLiteral("star"),
                        on ? IconTone::Star : IconTone::Muted);
    m_star->setPinned(on);
    m_star->setToolTip(on ? tr("Starred - click to unstar") : tr("Star - keep it at the top of your history"));
    m_star->setAccessibleName(on ? tr("Unstar") : tr("Star"));
}

void ResultView::updateFooter()
{
    QStringList parts;
    const QString provider = shortProvider(m_result.providerId);
    if (!provider.isEmpty())
        parts << (m_result.model.isEmpty() ? provider : QStringLiteral("%1 · %2").arg(provider, m_result.model));
    if (m_fromHistory && m_result.timestamp.isValid())
        parts << QLocale().toString(m_result.timestamp.toLocalTime(), QLocale::ShortFormat);
    if (m_fromHistory)
        parts << tr("from history");
    else if (m_result.fromCache)
        parts << tr("from cache");
    m_footer->setText(parts.join(QStringLiteral("  ·  ")));
    m_footer->setVisible(!parts.isEmpty());
}

} // namespace sct
