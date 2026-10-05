#include "ui/Widgets.h"

#include "ui/Theme.h"

#include <QAction>
#include <QButtonGroup>
#include <QEvent>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace sct::ui {

QString withShortcut(const QString &text, const QKeySequence &shortcut)
{
    if (shortcut.isEmpty())
        return text;
    return QStringLiteral("%1  (%2)").arg(text, shortcut.toString(QKeySequence::NativeText));
}

QToolButton *makeIconButton(const QString &iconName, const QString &toolTip, QWidget *parent, IconTone tone,
                            int iconSize)
{
    auto *b = new QToolButton(parent);
    b->setIcon(icon(iconName, tone));
    b->setIconSize(QSize(iconSize, iconSize));
    b->setToolTip(toolTip);
    b->setAccessibleName(toolTip);
    b->setAutoRaise(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::TabFocus);
    return b;
}

QFrame *makeDivider(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setObjectName(QStringLiteral("divider"));
    line->setFrameShape(QFrame::NoFrame);
    line->setFixedHeight(1);
    return line;
}

// ---- SegmentedControl ----------------------------------------------------------

SegmentedControl::SegmentedControl(QWidget *parent)
    : QWidget(parent)
    , m_group(new QButtonGroup(this))
    , m_layout(new QHBoxLayout(this))
{
    setObjectName(QStringLiteral("segmented"));
    setAttribute(Qt::WA_StyledBackground, true);
    m_layout->setContentsMargins(3, 3, 3, 3);
    m_layout->setSpacing(2);
    m_group->setExclusive(true);
    connect(m_group, &QButtonGroup::idClicked, this, &SegmentedControl::currentIndexChanged);
}

int SegmentedControl::addSegment(const QString &text, const QString &toolTip)
{
    auto *b = new QToolButton(this);
    b->setText(text);
    b->setToolTip(toolTip);
    b->setCheckable(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setToolButtonStyle(Qt::ToolButtonTextOnly);
    b->setFocusPolicy(Qt::TabFocus);
    const int index = int(m_buttons.size());
    m_group->addButton(b, index);
    m_layout->addWidget(b);
    m_buttons.append(b);
    if (index == 0)
        b->setChecked(true);
    return index;
}

int SegmentedControl::count() const { return int(m_buttons.size()); }

int SegmentedControl::currentIndex() const { return m_group->checkedId(); }

void SegmentedControl::setCurrentIndex(int index)
{
    if (index >= 0 && index < m_buttons.size())
        m_buttons.at(index)->setChecked(true);
}

QAbstractButton *SegmentedControl::button(int index) const
{
    return (index >= 0 && index < m_buttons.size()) ? m_buttons.at(index) : nullptr;
}

// ---- BusyIndicator -------------------------------------------------------------

BusyIndicator::BusyIndicator(int size, QWidget *parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
    , m_size(size)
{
    setFixedSize(size, size);
    m_timer->setInterval(16);
    connect(m_timer, &QTimer::timeout, this, [this] {
        m_angle = (m_angle + 6) % 360;
        update();
    });
}

QSize BusyIndicator::sizeHint() const { return QSize(m_size, m_size); }

void BusyIndicator::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    paintSpinner(&p, QRectF(rect()).adjusted(1, 1, -1, -1), m_angle, Theme::colors().accent, qMax(2.0, m_size / 9.0));
}

void BusyIndicator::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_timer->start();
}

void BusyIndicator::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_timer->stop();
}

// ---- CollapsibleSection -----------------------------------------------------------

CollapsibleSection::CollapsibleSection(const QString &title, const QString &iconName, QWidget *parent)
    : QWidget(parent)
    , m_header(new QToolButton(this))
    , m_content(new QWidget(this))
    , m_contentLayout(new QVBoxLayout(m_content))
    , m_title(title)
{
    Q_UNUSED(iconName);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_header->setObjectName(QStringLiteral("sectionHeader"));
    m_header->setCheckable(true);
    m_header->setChecked(true);
    m_header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_header->setIconSize(QSize(14, 14));
    m_header->setCursor(Qt::PointingHandCursor);
    m_header->setFont(Theme::uiFont(-1, QFont::DemiBold));
    m_header->setFocusPolicy(Qt::TabFocus);
    layout->addWidget(m_header, 0, Qt::AlignLeft);

    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(8);
    layout->addWidget(m_content);

    connect(m_header, &QToolButton::toggled, this, [this](bool on) {
        m_content->setVisible(on);
        updateHeader();
        emit expandedChanged(on);
    });
    updateHeader();
}

void CollapsibleSection::setTitle(const QString &title)
{
    m_title = title;
    updateHeader();
}

void CollapsibleSection::setCount(int count)
{
    m_count = count;
    updateHeader();
}

bool CollapsibleSection::isExpanded() const { return m_header->isChecked(); }

void CollapsibleSection::setExpanded(bool expanded) { m_header->setChecked(expanded); }

void CollapsibleSection::updateHeader()
{
    // Muted tone for both states: a checked header should not turn accent.
    m_header->setIcon(icon(isExpanded() ? QStringLiteral("chevron-down") : QStringLiteral("chevron-right"),
                           IconTone::Muted));
    m_header->setText(m_count > 0 ? QStringLiteral("%1 · %2").arg(m_title).arg(m_count) : m_title);
    m_header->setToolTip(isExpanded() ? tr("Hide") : tr("Show"));
}

// ---- Banner ------------------------------------------------------------------------

Banner::Banner(Kind kind, QWidget *parent)
    : QFrame(parent)
    , m_kind(kind)
{
    setFrameShape(QFrame::NoFrame);
    setAttribute(Qt::WA_StyledBackground, true);

    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(14, 12, 10, 12);
    outer->setSpacing(12);

    m_icon = new QLabel(this);
    m_icon->setFixedSize(20, 20);
    outer->addWidget(m_icon, 0, Qt::AlignTop);

    auto *body = new QVBoxLayout;
    body->setContentsMargins(0, 1, 0, 0);
    body->setSpacing(6);
    outer->addLayout(body, 1);

    m_title = new QLabel(this);
    m_title->setFont(Theme::uiFont(-1, QFont::DemiBold));
    m_title->setWordWrap(true);
    m_title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_title->hide();
    body->addWidget(m_title);

    m_text = new QLabel(this);
    m_text->setWordWrap(true);
    m_text->setTextFormat(Qt::RichText);
    m_text->setOpenExternalLinks(true);
    m_text->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_text->hide();
    body->addWidget(m_text);

    m_details = new QLabel(this);
    m_details->setObjectName(QStringLiteral("bannerDetails"));
    m_details->setWordWrap(true);
    m_details->setTextFormat(Qt::PlainText);
    m_details->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    m_details->setFont(Theme::monoFont(8.5));
    m_details->hide();

    m_buttonRow = new QHBoxLayout;
    m_buttonRow->setContentsMargins(0, 4, 0, 0);
    m_buttonRow->setSpacing(8);
    m_detailsToggle = new QPushButton(tr("Show details"), this);
    m_detailsToggle->setProperty("link", true);
    m_detailsToggle->setCursor(Qt::PointingHandCursor);
    m_detailsToggle->setFlat(true);
    m_detailsToggle->hide();
    connect(m_detailsToggle, &QPushButton::clicked, this, [this] { setDetailsVisible(!detailsVisible()); });
    m_buttonRow->addStretch(1);
    m_buttonRow->addWidget(m_detailsToggle);
    body->addLayout(m_buttonRow);
    body->addWidget(m_details);

    m_close = makeIconButton(QStringLiteral("close"), tr("Dismiss"), this, IconTone::Muted, 16);
    m_close->hide();
    connect(m_close, &QToolButton::clicked, this, [this] {
        hide();
        emit closed();
    });
    outer->addWidget(m_close, 0, Qt::AlignTop);

    setKind(kind);
}

void Banner::setKind(Kind kind)
{
    m_kind = kind;
    const char *name = kind == Kind::Error ? "error" : kind == Kind::Warning ? "warning" : "info";
    setStyleProperty(this, "banner", QString::fromLatin1(name));
    // Children's QSS depends on the parent's property: repolish them too.
    for (QWidget *w : findChildren<QWidget *>()) {
        w->style()->unpolish(w);
        w->style()->polish(w);
    }
    refreshIcon();
}

void Banner::refreshIcon()
{
    const qreal dpr = devicePixelRatioF();
    switch (m_kind) {
    case Kind::Info:
        m_icon->setPixmap(iconPixmap(QStringLiteral("info"), IconTone::Info, 20, dpr));
        break;
    case Kind::Warning:
        m_icon->setPixmap(iconPixmap(QStringLiteral("warning"), IconTone::Warning, 20, dpr));
        break;
    case Kind::Error:
        m_icon->setPixmap(iconPixmap(QStringLiteral("error"), IconTone::Error, 20, dpr));
        break;
    }
}

void Banner::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        refreshIcon();
}

void Banner::setTitle(const QString &title)
{
    m_title->setText(title);
    m_title->setVisible(!title.isEmpty());
}

void Banner::setText(const QString &text)
{
    m_text->setText(text);
    m_text->setVisible(!text.isEmpty());
}

void Banner::setDetails(const QString &details)
{
    m_details->setText(details);
    m_detailsToggle->setVisible(!details.trimmed().isEmpty());
    setDetailsVisible(false);
}

void Banner::setClosable(bool closable) { m_close->setVisible(closable); }

QPushButton *Banner::addButton(const QString &text, bool primary)
{
    auto *b = new QPushButton(text, this);
    b->setCursor(Qt::PointingHandCursor);
    if (primary)
        b->setProperty("primary", true);
    // Buttons go before the stretch, left-aligned.
    m_buttonRow->insertWidget(int(m_buttons.size()), b);
    m_buttons.append(b);
    return b;
}

void Banner::clearButtons()
{
    qDeleteAll(m_buttons);
    m_buttons.clear();
}

QString Banner::title() const { return m_title->text(); }
QString Banner::text() const { return m_text->text(); }
QString Banner::details() const { return m_details->text(); }
bool Banner::detailsVisible() const { return !m_details->isHidden(); }

void Banner::setDetailsVisible(bool visible)
{
    m_details->setVisible(visible && !m_details->text().isEmpty());
    m_detailsToggle->setText(visible ? tr("Hide details") : tr("Show details"));
}

// ---- PasswordLineEdit -----------------------------------------------------------------

PasswordLineEdit::PasswordLineEdit(QWidget *parent)
    : QLineEdit(parent)
{
    setEchoMode(QLineEdit::Password);
    setInputMethodHints(Qt::ImhHiddenText | Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase);
    m_toggle = addAction(icon(QStringLiteral("eye"), IconTone::Muted), QLineEdit::TrailingPosition);
    m_toggle->setToolTip(tr("Show key"));
    connect(m_toggle, &QAction::triggered, this, [this] { setRevealed(!isRevealed()); });
}

void PasswordLineEdit::setRevealed(bool revealed)
{
    setEchoMode(revealed ? QLineEdit::Normal : QLineEdit::Password);
    m_toggle->setIcon(icon(revealed ? QStringLiteral("eye-off") : QStringLiteral("eye"), IconTone::Muted));
    m_toggle->setToolTip(revealed ? tr("Hide key") : tr("Show key"));
}

bool PasswordLineEdit::isRevealed() const { return echoMode() == QLineEdit::Normal; }

// ---- SpeakButton ------------------------------------------------------------------------

SpeakButton::SpeakButton(QWidget *parent, bool showText)
    : QToolButton(parent)
    , m_showText(showText)
    , m_spinTimer(new QTimer(this))
{
    setCursor(Qt::PointingHandCursor);
    setAutoRaise(true);
    setIconSize(QSize(18, 18));
    setFocusPolicy(Qt::TabFocus);
    setToolButtonStyle(showText ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly);
    m_idleToolTip = tr("Listen");
    m_spinTimer->setInterval(33);
    connect(m_spinTimer, &QTimer::timeout, this, [this] {
        m_angle = (m_angle + 12) % 360;
        updateSpinnerIcon();
    });
    refresh();
}

void SpeakButton::setSpeechState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    refresh();
}

void SpeakButton::setIdleToolTip(const QString &toolTip)
{
    m_idleToolTip = toolTip;
    refresh();
}

void SpeakButton::changeEvent(QEvent *event)
{
    QToolButton::changeEvent(event);
    if (event->type() == QEvent::PaletteChange && m_state == State::Loading)
        updateSpinnerIcon();
}

void SpeakButton::refresh()
{
    switch (m_state) {
    case State::Idle:
        m_spinTimer->stop();
        setIcon(ui::icon(QStringLiteral("speaker")));
        setText(tr("Listen"));
        setToolTip(m_idleToolTip);
        break;
    case State::Loading:
        setText(tr("Loading voice…"));
        setToolTip(tr("Loading voice… click to cancel"));
        updateSpinnerIcon();
        m_spinTimer->start();
        break;
    case State::Speaking:
        m_spinTimer->stop();
        setIcon(ui::icon(QStringLiteral("stop"), IconTone::Accent));
        setText(tr("Stop"));
        setToolTip(withShortcut(tr("Stop"), QKeySequence(Qt::Key_Escape)));
        break;
    }
    setAccessibleName(m_state == State::Idle ? m_idleToolTip : toolTip());
}

void SpeakButton::updateSpinnerIcon()
{
    const qreal dpr = devicePixelRatioF();
    const QSize s = iconSize();
    QPixmap pm(s * dpr);
    pm.fill(Qt::transparent);
    pm.setDevicePixelRatio(dpr);
    {
        QPainter p(&pm);
        paintSpinner(&p, QRectF(2, 2, s.width() - 4, s.height() - 4), m_angle, Theme::colors().accent, 2.0);
    }
    setIcon(QIcon(pm));
}

// ---- IconLabel ------------------------------------------------------------------------------

IconLabel::IconLabel(const QString &iconName, IconTone tone, int size, QWidget *parent)
    : QLabel(parent)
    , m_name(iconName)
    , m_tone(tone)
    , m_size(size)
{
    setFixedSize(size, size);
    refresh();
}

void IconLabel::setIcon(const QString &iconName, IconTone tone)
{
    m_name = iconName;
    m_tone = tone;
    refresh();
}

void IconLabel::changeEvent(QEvent *event)
{
    QLabel::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        refresh();
}

void IconLabel::refresh() { setPixmap(iconPixmap(m_name, m_tone, m_size, devicePixelRatioF())); }

} // namespace sct::ui
