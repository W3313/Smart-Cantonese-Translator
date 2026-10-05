#include "ui/Surfaces.h"

#include "ui/Motion.h"
#include "ui/Theme.h"

#include <QAction>
#include <QApplication>
#include <QEnterEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QLayout>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

namespace sct::ui {

namespace {

constexpr qreal kCardRadius = 14.0;

} // namespace

bool isScreenChangeEvent(const QEvent *event)
{
    switch (event->type()) {
    case QEvent::ScreenChangeInternal:
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    case QEvent::DevicePixelRatioChange:
#endif
        return true;
    default:
        return false;
    }
}

QFrame *makeDivider(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setObjectName(QStringLiteral("divider"));
    line->setFrameShape(QFrame::NoFrame);
    line->setFixedHeight(1);
    return line;
}

// ---- Card -----------------------------------------------------------------------------

Card::Card(QWidget *parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::NoFrame);
    setAttribute(Qt::WA_Hover);
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        const bool within = now && (now == this || isAncestorOf(now));
        if (within != m_focusWithin) {
            m_focusWithin = within;
            updateState();
        }
    });
}

void Card::addRevealWidget(IconButton *button)
{
    if (!button)
        return;
    m_revealWidgets.append(button);
    button->setRevealOpacity(m_reveal);
}

void Card::updateState()
{
    const qreal revealTarget = (m_hovered || m_focusWithin) ? 1.0 : 0.0;
    motion::animate(this, QStringLiteral("reveal"), m_reveal, revealTarget, motion::kNormal, [this](const QVariant &v) {
        m_reveal = v.toReal();
        for (const QPointer<IconButton> &b : std::as_const(m_revealWidgets)) {
            if (b)
                b->setRevealOpacity(m_reveal);
        }
    });
    const qreal focusTarget = (m_focusHighlight && m_focusWithin) ? 1.0 : 0.0;
    motion::animate(this, QStringLiteral("focus"), m_focus, focusTarget, motion::kNormal, [this](const QVariant &v) {
        m_focus = v.toReal();
        update();
    });
}

void Card::enterEvent(QEnterEvent *event)
{
    QFrame::enterEvent(event);
    m_hovered = true;
    updateState();
}

void Card::leaveEvent(QEvent *event)
{
    QFrame::leaveEvent(event);
    m_hovered = false;
    updateState();
}

void Card::paintEvent(QPaintEvent *)
{
    const ThemeColors &c = Theme::colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).marginsRemoved(QMarginsF(shadowMargins())).adjusted(0.5, 0.5, -0.5, -0.5);
    paintSoftShadow(&p, r, kCardRadius, 1.0);
    QPainterPath path;
    path.addRoundedRect(r, kCardRadius, kCardRadius);
    p.fillPath(path, c.surface);
    const QColor border = mix(c.border, withAlpha(c.accent, 0.45), m_focus);
    p.setPen(QPen(border, 1.0));
    p.drawPath(path);
}

// ---- ClipBox ------------------------------------------------------------------------------

ClipBox::ClipBox(QWidget *body, QWidget *parent)
    : QWidget(parent)
    , m_body(body)
{
    m_body->setParent(this);
    m_body->installEventFilter(this);
    QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
}

int ClipBox::fullHeight(int w) const
{
    int h = m_body->hasHeightForWidth() ? m_body->heightForWidth(w) : -1;
    if (h < 0)
        h = m_body->sizeHint().height();
    return qMax(0, h);
}

void ClipBox::setVisibleHeight(int h)
{
    m_visible = h;
    updateGeometry();
}

void ClipBox::relayout()
{
    m_body->setGeometry(0, 0, width(), fullHeight(width()));
    updateGeometry();
}

QSize ClipBox::sizeHint() const
{
    const int w = width() > 0 ? width() : m_body->sizeHint().width();
    return QSize(m_body->sizeHint().width(), heightForWidth(w));
}

QSize ClipBox::minimumSizeHint() const { return QSize(0, 0); }

int ClipBox::heightForWidth(int w) const
{
    const int full = fullHeight(w);
    return m_visible < 0 ? full : qMin(full, m_visible);
}

void ClipBox::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_body->setGeometry(0, 0, width(), fullHeight(width()));
}

bool ClipBox::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_body && event->type() == QEvent::LayoutRequest) {
        updateGeometry();
        if (width() > 0)
            m_body->resize(width(), fullHeight(width()));
    }
    return QWidget::eventFilter(watched, event);
}

// ---- Disclosure -----------------------------------------------------------------------------

class Disclosure::Header : public ButtonBase
{
public:
    explicit Header(QWidget *parent)
        : ButtonBase(parent)
    {
        setFont(Theme::uiFont(9.5, QFont::DemiBold));
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    void setProgress(qreal progress)
    {
        m_progress = progress;
        update();
    }

    QSize sizeHint() const override { return QSize(fontMetrics().horizontalAdvance(text()) + 6 + 8 + 14 + 10, 30); }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const ThemeColors &c = Theme::colors();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
        QPainterPath path;
        path.addRoundedRect(r, 8, 8);
        p.fillPath(path, withAlpha(c.hover, hoverProgress()));
        const QColor fg = mix(c.textMuted, c.text, qMax(hoverProgress(), m_progress * 0.5));
        p.setFont(font());
        p.setPen(fg);
        const qreal textW = fontMetrics().horizontalAdvance(text());
        p.drawText(QRectF(r.left() + 6, r.top(), textW + 2, r.height()), Qt::AlignLeft | Qt::AlignVCenter, text());
        const QPointF centre(r.left() + 6 + textW + 8 + 7, r.center().y());
        p.translate(centre);
        p.rotate(90.0 * m_progress);
        p.drawPixmap(QPointF(-7, -7), iconPixmap(QStringLiteral("chevron-right"), fg, 14, devicePixelRatioF()));
        p.resetTransform();
        paintFocusRing(&p, QRectF(rect()), 9);
    }

private:
    qreal m_progress = 0.0;
};

Disclosure::Disclosure(QWidget *parent)
    : QWidget(parent)
    , m_header(new Header(this))
    , m_body(new QWidget)
    , m_contentLayout(new QVBoxLayout(m_body))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(m_header, 0, Qt::AlignLeft);
    m_contentLayout->setContentsMargins(0, 2, 0, 4);
    m_contentLayout->setSpacing(8);
    m_clip = new ClipBox(m_body, this);
    m_clip->setVisibleHeight(0);
    m_body->hide();
    layout->addWidget(m_clip);
    connect(m_header, &QAbstractButton::clicked, this, [this] { setExpanded(!m_expanded); });
}

void Disclosure::setTitle(const QString &title)
{
    m_header->setText(title);
    m_header->setAccessibleName(title);
    m_header->updateGeometry();
}

QString Disclosure::title() const { return m_header->text(); }

void Disclosure::contentChanged()
{
    if (m_expanded)
        m_clip->relayout();
    else
        m_clip->updateGeometry();
}

void Disclosure::setExpanded(bool expanded, bool animated)
{
    if (expanded == m_expanded)
        return;
    m_expanded = expanded;
    m_header->setAccessibleDescription(expanded ? tr("Expanded") : tr("Collapsed"));
    const int duration = animated ? motion::kNormal : 0;
    motion::animate(m_header, QStringLiteral("chevron"), expanded ? 0.0 : 1.0, expanded ? 1.0 : 0.0, duration,
                    [this](const QVariant &v) { m_header->setProgress(v.toReal()); });
    if (expanded) {
        // Children fade individually below; never nest opacity effects.
        motion::finish(this, QStringLiteral("fade"));
        m_body->show();
        const int clipWidth = m_clip->width() > 0 ? m_clip->width() : width();
        const int full = m_clip->fullHeight(clipWidth);
        m_clip->relayout();
        motion::animate(
            m_clip, QStringLiteral("height"), 0, full, animated ? qBound(motion::kNormal, full, motion::kSlow) : 0,
            [this](const QVariant &v) { m_clip->setVisibleHeight(v.toInt()); },
            [this] { m_clip->setVisibleHeight(-1); }, motion::outCubic());
        if (animated) {
            int i = 0;
            for (int k = 0; k < m_contentLayout->count(); ++k) {
                if (QWidget *w = m_contentLayout->itemAt(k)->widget())
                    motion::fadeIn(w, motion::kNormal, 30 + 55 * i++);
            }
        }
    } else {
        const int from = m_clip->height();
        motion::animate(
            m_clip, QStringLiteral("height"), from, 0, animated ? motion::kNormal : 0,
            [this](const QVariant &v) { m_clip->setVisibleHeight(v.toInt()); },
            [this] {
                m_clip->setVisibleHeight(0);
                m_body->hide();
            },
            motion::inOutCubic());
    }
    emit expandedChanged(expanded);
}

// ---- Banner ----------------------------------------------------------------------------------

Banner::Banner(Kind kind, QWidget *parent)
    : QFrame(parent)
    , m_kind(kind)
{
    setFrameShape(QFrame::NoFrame);
    setAttribute(Qt::WA_StyledBackground, true);

    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(16, 14, 10, 14);
    outer->setSpacing(12);

    m_icon = new QLabel(this);
    m_icon->setFixedSize(20, 20);
    outer->addWidget(m_icon, 0, Qt::AlignTop);

    auto *body = new QVBoxLayout;
    body->setContentsMargins(0, 1, 0, 0);
    body->setSpacing(4);
    outer->addLayout(body, 1);

    m_title = new QLabel(this);
    m_title->setFont(Theme::uiFont(10.5, QFont::DemiBold));
    m_title->setWordWrap(true);
    m_title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_title->hide();
    body->addWidget(m_title);

    m_text = new QLabel(this);
    m_text->setObjectName(QStringLiteral("bannerText"));
    m_text->setWordWrap(true);
    m_text->setTextFormat(Qt::RichText);
    m_text->setOpenExternalLinks(true);
    m_text->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_text->hide();
    body->addWidget(m_text);

    m_buttonRow = new QHBoxLayout;
    m_buttonRow->setContentsMargins(0, 8, 0, 0);
    m_buttonRow->setSpacing(8);
    m_detailsToggle = new Button(tr("Show details"), Button::Variant::Ghost, this);
    m_detailsToggle->setObjectName(QStringLiteral("detailsToggle"));
    m_detailsToggle->hide();
    connect(m_detailsToggle, &QAbstractButton::clicked, this, [this] { setDetailsVisible(!detailsVisible()); });
    m_buttonRow->addWidget(m_detailsToggle);
    m_buttonRow->addStretch(1);
    body->addLayout(m_buttonRow);

    m_details = new QLabel(this);
    m_details->setObjectName(QStringLiteral("bannerDetails"));
    m_details->setWordWrap(true);
    m_details->setTextFormat(Qt::PlainText);
    m_details->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    m_details->setFont(Theme::monoFont(8.5));
    m_details->hide();
    body->addWidget(m_details);

    m_close = new IconButton(QStringLiteral("close"), tr("Dismiss"), this, IconTone::Muted, 16);
    m_close->hide();
    connect(m_close, &QAbstractButton::clicked, this, [this] {
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
        m_icon->setPixmap(iconPixmap(QStringLiteral("info"), IconTone::Accent, 20, dpr));
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

bool Banner::event(QEvent *event)
{
    if (isScreenChangeEvent(event))
        refreshIcon();
    return QFrame::event(event);
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

Button *Banner::addButton(const QString &text, bool primary)
{
    auto *b = new Button(text, primary ? Button::Variant::Primary : Button::Variant::Secondary, this);
    m_buttonRow->insertWidget(int(m_buttons.size()), b);
    m_buttons.append(b);
    return b;
}

void Banner::clearButtons()
{
    qDeleteAll(m_buttons);
    m_buttons.clear();
}

void Banner::animateIn(bool shake)
{
    show();
    motion::fadeIn(this, motion::kNormal);
    if (!shake || motion::reduced())
        return;
    // Slide down a few pixels, then a gentle shake.
    if (QWidget *pw = parentWidget(); pw && pw->layout()) {
        QLayout *layout = pw->layout();
        // Resting margins, remembered so an interrupted slide can't drift them.
        const QVariant stored = layout->property("sct-slide-margins");
        const QMargins cur = layout->contentsMargins();
        const QRect packed = stored.isValid() ? stored.toRect() : QRect(cur.left(), cur.top(), cur.right(), cur.bottom());
        layout->setProperty("sct-slide-margins", packed);
        const QMargins base(packed.x(), packed.y(), packed.width(), packed.height());
        QPointer<QLayout> guard(layout);
        motion::animate(
            this, QStringLiteral("slide"), 0.0, 1.0, 180,
            [guard, base](const QVariant &v) {
                if (guard)
                    guard->setContentsMargins(base.left(), qRound(base.top() * v.toReal()), base.right(),
                                              base.bottom());
            },
            [this, guard, base] {
                if (guard)
                    guard->setContentsMargins(base);
                motion::shake(this);
            });
    }
}

QString Banner::title() const { return m_title->text(); }
QString Banner::text() const { return m_text->text(); }
QString Banner::details() const { return m_details->text(); }
bool Banner::detailsVisible() const { return !m_details->isHidden(); }

void Banner::setDetailsVisible(bool visible)
{
    m_details->setVisible(visible && !m_details->text().isEmpty());
    m_detailsToggle->setText(visible ? tr("Hide details") : tr("Show details"));
    m_detailsToggle->updateGeometry();
    if (visible)
        motion::fadeIn(m_details, motion::kFast);
}

// ---- SkeletonView ---------------------------------------------------------------------------

SkeletonView::SkeletonView(QWidget *parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
{
    setAccessibleName(tr("Translating"));
    m_timer->setInterval(16);
    connect(m_timer, &QTimer::timeout, this, [this] {
        m_phase = std::fmod(m_phase + 16.0 / 1500.0, 1.0);
        update();
    });
}

void SkeletonView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_phase = 0.0;
    if (!motion::reduced())
        m_timer->start();
    motion::animate(this, QStringLiteral("appear"), 0.0, 1.0, motion::kNormal, [this](const QVariant &v) {
        m_appear = v.toReal();
        update();
    });
}

void SkeletonView::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_timer->stop();
}

void SkeletonView::paintEvent(QPaintEvent *)
{
    const ThemeColors &c = Theme::colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(m_appear);
    p.setPen(Qt::NoPen);

    const qreal w = width();
    QBrush brush(c.skeleton);
    if (!motion::reduced()) {
        const qreal band = qMax(160.0, w * 0.45);
        const qreal cx = -band + (w + 2 * band) * m_phase;
        QLinearGradient g(cx - band / 2, 0, cx + band / 2, 0);
        g.setColorAt(0.0, c.skeleton);
        g.setColorAt(0.5, c.skeletonShine);
        g.setColorAt(1.0, c.skeleton);
        brush = QBrush(g);
    }
    p.setBrush(brush);
    struct Bar
    {
        qreal widthFraction;
        qreal height;
        qreal gapAfter;
    };
    // Headline (two lines), Jyutping, then the action row and a disclosure.
    const Bar bars[] = {{0.86, 28, 12}, {0.58, 28, 18}, {0.46, 14, 28}};
    qreal y = 6;
    for (const Bar &b : bars) {
        p.drawRoundedRect(QRectF(0, y, w * b.widthFraction, b.height), 8, 8);
        y += b.height + b.gapAfter;
    }
    p.drawRoundedRect(QRectF(0, y, 92, 30), 10, 10);
    p.drawRoundedRect(QRectF(102, y, 80, 30), 10, 10);
    y += 30 + 26;
    p.drawRoundedRect(QRectF(0, y, qMin(200.0, w * 0.4), 12), 6, 6);
}

// ---- Toast ---------------------------------------------------------------------------------------

Toast::Toast(QWidget *window)
    : QWidget(window)
    , m_timer(new QTimer(this))
{
    setObjectName(QStringLiteral("sctToast"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFont(Theme::uiFont(9.5, QFont::Medium));
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &Toast::dismiss);
    window->installEventFilter(this);
    hide();
}

Toast *Toast::current(QWidget *window)
{
    return window ? window->findChild<Toast *>(QStringLiteral("sctToast"), Qt::FindDirectChildrenOnly) : nullptr;
}

Toast *Toast::showMessage(QWidget *window, const QString &text, const QString &iconName, int msec)
{
    if (!window || text.isEmpty())
        return nullptr;
    Toast *t = current(window);
    if (!t)
        t = new Toast(window);
    t->popup(text, iconName, msec);
    return t;
}

void Toast::popup(const QString &text, const QString &iconName, int msec)
{
    m_text = text;
    m_icon = iconName;
    const QFontMetrics fm(font());
    const int iconW = m_icon.isEmpty() ? 0 : 16 + 8;
    const int maxW = qMax(200, parentWidget()->width() - 48);
    resize(qMin(maxW, fm.horizontalAdvance(m_text) + iconW + 36), 38);
    reposition();
    QWidget::show();
    raise();
    motion::animate(this, QStringLiteral("toast"), m_progress, 1.0, motion::kNormal, [this](const QVariant &v) {
        m_progress = v.toReal();
        reposition();
        update();
    });
    m_timer->start(msec);
}

void Toast::dismiss()
{
    motion::animate(
        this, QStringLiteral("toast"), m_progress, 0.0, motion::kNormal,
        [this](const QVariant &v) {
            m_progress = v.toReal();
            reposition();
            update();
        },
        [this] { hide(); }, motion::inOutCubic());
}

void Toast::reposition()
{
    QWidget *w = parentWidget();
    if (!w)
        return;
    const int y = w->height() - height() - 22 + qRound((1.0 - m_progress) * 16);
    move((w->width() - width()) / 2, y);
}

bool Toast::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize)
        reposition();
    return QWidget::eventFilter(watched, event);
}

void Toast::paintEvent(QPaintEvent *)
{
    const ThemeColors &c = Theme::colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(m_progress);
    const QRectF r = QRectF(rect()).adjusted(2, 1, -2, -4);
    const qreal radius = r.height() / 2.0;
    paintSoftShadow(&p, r, radius, 1.6);
    QPainterPath path;
    path.addRoundedRect(r, radius, radius);
    p.fillPath(path, c.tooltipBg);
    qreal x = r.left() + 16;
    if (!m_icon.isEmpty()) {
        p.drawPixmap(QPointF(x, r.center().y() - 8), iconPixmap(m_icon, c.tooltipText, 16, devicePixelRatioF()));
        x += 16 + 8;
    }
    p.setPen(c.tooltipText);
    p.setFont(font());
    const QRectF textRect(x, r.top(), r.right() - 16 - x, r.height());
    p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
               fontMetrics().elidedText(m_text, Qt::ElideRight, int(textRect.width())));
}

// ---- SidePanel --------------------------------------------------------------------------------------

SidePanel::SidePanel(QWidget *content, int panelWidth, QWidget *parent)
    : QWidget(parent)
    , m_content(content)
    , m_panelWidth(panelWidth)
{
    m_content->setParent(this);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setFixedWidth(0);
    hide();
}

QSize SidePanel::sizeHint() const { return QSize(m_width, m_content->sizeHint().height()); }

QSize SidePanel::minimumSizeHint() const { return QSize(m_width, 0); }

void SidePanel::setCurrentWidth(int w)
{
    m_width = w;
    setFixedWidth(w);
    m_content->setGeometry(w - m_panelWidth, 0, m_panelWidth, height());
}

void SidePanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_content->setGeometry(m_width - m_panelWidth, 0, m_panelWidth, height());
}

void SidePanel::setOpen(bool open, bool animated)
{
    if (open == m_open)
        return;
    m_open = open;
    if (open)
        show();
    motion::animate(
        this, QStringLiteral("width"), m_width, open ? m_panelWidth : 0, animated ? motion::kSlow : 0,
        [this](const QVariant &v) { setCurrentWidth(v.toInt()); },
        [this, open] {
            setCurrentWidth(open ? m_panelWidth : 0);
            if (!open)
                hide();
        },
        open ? motion::outCubic() : motion::inOutCubic());
    emit openChanged(open);
}

// ---- PasswordLineEdit -------------------------------------------------------------------------------

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

// ---- IconLabel ----------------------------------------------------------------------------------------

IconLabel::IconLabel(const QString &iconName, IconTone tone, int pixelSize, QWidget *parent)
    : QLabel(parent)
    , m_name(iconName)
    , m_tone(tone)
    , m_size(pixelSize)
{
    setFixedSize(pixelSize, pixelSize);
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

bool IconLabel::event(QEvent *event)
{
    if (isScreenChangeEvent(event))
        refresh();
    return QLabel::event(event);
}

void IconLabel::refresh() { setPixmap(iconPixmap(m_name, m_tone, m_size, devicePixelRatioF())); }

} // namespace sct::ui
