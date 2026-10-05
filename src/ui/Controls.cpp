#include "ui/Controls.h"

#include "ui/Motion.h"
#include "ui/Theme.h"

#include <QEnterEvent>
#include <QFocusEvent>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QToolTip>
#include <QVariantAnimation>

#include <cmath>

namespace sct::ui {

namespace {

constexpr qreal kPi = 3.14159265358979;

QRectF lerp(const QRectF &a, const QRectF &b, qreal t)
{
    return QRectF(a.x() + (b.x() - a.x()) * t, a.y() + (b.y() - a.y()) * t, a.width() + (b.width() - a.width()) * t,
                  a.height() + (b.height() - a.height()) * t);
}

void fillRounded(QPainter *p, const QRectF &r, qreal radius, const QColor &color)
{
    if (color.alpha() == 0)
        return;
    QPainterPath path;
    path.addRoundedRect(r, radius, radius);
    p->fillPath(path, color);
}

} // namespace

QString withShortcut(const QString &text, const QKeySequence &shortcut)
{
    if (shortcut.isEmpty())
        return text;
    return QStringLiteral("%1  (%2)").arg(text, shortcut.toString(QKeySequence::NativeText));
}

// ---- ButtonBase ----------------------------------------------------------------------

ButtonBase::ButtonBase(QWidget *parent)
    : QAbstractButton(parent)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setAttribute(Qt::WA_Hover);
    connect(this, &QAbstractButton::pressed, this, [this] { animatePress(true); });
    connect(this, &QAbstractButton::released, this, [this] { animatePress(false); });
}

void ButtonBase::animateHover(bool on)
{
    motion::animate(this, QStringLiteral("hover"), m_hover, on ? 1.0 : 0.0, motion::kFast, [this](const QVariant &v) {
        m_hover = v.toReal();
        update();
    });
}

void ButtonBase::animatePress(bool on)
{
    motion::animate(this, QStringLiteral("press"), m_press, on ? 1.0 : 0.0, on ? 90 : motion::kFast,
                    [this](const QVariant &v) {
                        m_press = v.toReal();
                        update();
                    });
}

void ButtonBase::paintFocusRing(QPainter *p, const QRectF &rect, qreal radius) const
{
    if (!focusRingVisible())
        return;
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(Theme::colors().accent, 2));
    p->drawRoundedRect(rect.adjusted(1, 1, -1, -1), radius, radius);
    p->restore();
}

void ButtonBase::enterEvent(QEnterEvent *event)
{
    QAbstractButton::enterEvent(event);
    if (isEnabled())
        animateHover(true);
}

void ButtonBase::leaveEvent(QEvent *event)
{
    QAbstractButton::leaveEvent(event);
    animateHover(false);
}

void ButtonBase::focusInEvent(QFocusEvent *event)
{
    if (event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason
        || event->reason() == Qt::ShortcutFocusReason)
        m_keyboardFocus = true;
    else if (event->reason() != Qt::ActiveWindowFocusReason && event->reason() != Qt::PopupFocusReason)
        m_keyboardFocus = false;
    QAbstractButton::focusInEvent(event);
    update();
}

void ButtonBase::focusOutEvent(QFocusEvent *event)
{
    QAbstractButton::focusOutEvent(event);
    update();
}

void ButtonBase::mousePressEvent(QMouseEvent *event)
{
    m_keyboardFocus = false;
    QAbstractButton::mousePressEvent(event);
}

void ButtonBase::changeEvent(QEvent *event)
{
    QAbstractButton::changeEvent(event);
    if (event->type() == QEvent::EnabledChange) {
        if (!isEnabled())
            animateHover(false);
        update();
    } else if (event->type() == QEvent::PaletteChange || event->type() == QEvent::FontChange) {
        updateGeometry();
        update();
    }
}

// ---- IconButton ------------------------------------------------------------------------

IconButton::IconButton(const QString &iconName, const QString &toolTip, QWidget *parent, IconTone tone, int iconSize)
    : ButtonBase(parent)
    , m_iconName(iconName)
    , m_tone(tone)
    , m_iconSize(iconSize)
{
    setToolTip(toolTip);
    setAccessibleName(toolTip.section(QStringLiteral("  ("), 0, 0));
    setFont(Theme::uiFont(9.5, QFont::Medium));
}

void IconButton::setIconName(const QString &name, IconTone tone)
{
    m_iconName = name;
    m_tone = tone;
    update();
}

void IconButton::setRevealOpacity(qreal opacity)
{
    if (qFuzzyCompare(m_reveal, opacity))
        return;
    m_reveal = opacity;
    update();
}

void IconButton::setPinned(bool pinned)
{
    m_pinned = pinned;
    update();
}

QSize IconButton::sizeHint() const
{
    const int h = m_iconSize + 14;
    if (text().isEmpty())
        return QSize(h, h);
    return QSize(10 + m_iconSize + 6 + fontMetrics().horizontalAdvance(text()) + 12, h);
}

QColor IconButton::contentColor() const
{
    const ThemeColors &c = Theme::colors();
    if (!isEnabled())
        return c.textDisabled;
    if (isChecked() && m_tone == IconTone::Text)
        return c.accent;
    if (m_tone == IconTone::Muted)
        return mix(c.textMuted, c.text, hoverProgress());
    return toneColor(m_tone);
}

void IconButton::paintGlyph(QPainter *p, const QRectF &rect, const QColor &color)
{
    if (m_iconName.isEmpty())
        return;
    const qreal dpr = devicePixelRatioF();
    const QPixmap pm = iconPixmap(m_iconName, color, m_iconSize, dpr);
    p->drawPixmap(QPointF(rect.center().x() - m_iconSize / 2.0, rect.center().y() - m_iconSize / 2.0), pm);
}

void IconButton::paintEvent(QPaintEvent *)
{
    const qreal reveal = (m_pinned || focusRingVisible() || isChecked() || isDown()) ? 1.0 : m_reveal;
    if (reveal <= 0.001)
        return;
    const ThemeColors &c = Theme::colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setOpacity(reveal);

    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    const qreal radius = 8;
    p.save();
    const qreal scale = 1.0 - 0.07 * pressProgress();
    p.translate(r.center());
    p.scale(scale, scale);
    p.translate(-r.center());
    if (isChecked())
        fillRounded(&p, r, radius, c.accentSoft);
    fillRounded(&p, r, radius, withAlpha(c.hover, hoverProgress()));
    fillRounded(&p, r, radius, withAlpha(c.pressed, pressProgress() * 0.6));

    const QColor fg = contentColor();
    if (text().isEmpty()) {
        paintGlyph(&p, r, fg);
    } else {
        const QRectF glyph(r.left() + 9, r.center().y() - m_iconSize / 2.0, m_iconSize, m_iconSize);
        paintGlyph(&p, glyph, fg);
        p.setPen(fg);
        p.setFont(font());
        const QRectF textRect(glyph.right() + 6, r.top(), r.right() - glyph.right() - 6, r.height());
        p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text());
    }
    p.restore();
    paintFocusRing(&p, QRectF(rect()), radius + 1);
}

// ---- Button ---------------------------------------------------------------------------------

Button::Button(const QString &text, Variant variant, QWidget *parent)
    : ButtonBase(parent)
    , m_variant(variant)
{
    setText(text);
    setAccessibleName(text);
    setVariant(variant);
}

void Button::setVariant(Variant variant)
{
    m_variant = variant;
    setFont(Theme::uiFont(variant == Variant::Primary ? 10.5 : 10, variant == Variant::Primary ? QFont::DemiBold
                                                                                               : QFont::Medium));
    updateGeometry();
    update();
}

void Button::setTrailingIcon(const QString &iconName)
{
    m_trailingIcon = iconName;
    updateGeometry();
    update();
}

int Button::contentWidth(const QString &label, bool withIcon) const
{
    int w = fontMetrics().horizontalAdvance(label);
    if (withIcon)
        w += 8 + 16;
    return w;
}

QSize Button::sizeHint() const
{
    const int pad = m_variant == Variant::Primary ? 20 : m_variant == Variant::Secondary ? 16 : 10;
    int w = contentWidth(text(), !m_trailingIcon.isEmpty());
    if (!m_busyText.isEmpty())
        w = qMax(w, contentWidth(m_busyText, true));
    const int h = m_variant == Variant::Primary ? 38 : 34;
    return QSize(w + 2 * pad, h);
}

void Button::setBusy(bool busy, const QString &busyText)
{
    if (!busyText.isEmpty() && busyText != m_busyText) {
        m_busyText = busyText;
        updateGeometry();
    }
    if (busy == m_busy)
        return;
    m_busy = busy;
    motion::animate(this, QStringLiteral("busy"), m_busyProgress, busy ? 1.0 : 0.0, motion::kNormal,
                    [this](const QVariant &v) {
                        m_busyProgress = v.toReal();
                        update();
                    });
    if (busy && !motion::reduced()) {
        if (!m_loop) {
            m_loop = new QVariantAnimation(this);
            m_loop->setStartValue(0.0);
            m_loop->setEndValue(1.0);
            m_loop->setDuration(1300);
            m_loop->setLoopCount(-1);
            connect(m_loop, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
                m_phase = v.toReal();
                update();
            });
        }
        m_loop->start();
    } else if (m_loop) {
        m_loop->stop();
    }
    update();
}

void Button::paintEvent(QPaintEvent *)
{
    const ThemeColors &c = Theme::colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    if (!isEnabled())
        p.setOpacity(0.5);

    const QRectF outer = QRectF(rect()).adjusted(1, 1, -1, -1);
    const qreal radius = 10;
    p.save();
    const qreal scale = 1.0 - (m_variant == Variant::Primary ? 0.03 : 0.025) * pressProgress();
    p.translate(outer.center());
    p.scale(scale, scale);
    p.translate(-outer.center());

    QColor fg;
    QPainterPath shape;
    shape.addRoundedRect(outer, radius, radius);
    switch (m_variant) {
    case Variant::Primary: {
        paintSoftShadow(&p, outer, radius, isEnabled() ? 0.9 : 0.0);
        QColor bg = mix(c.accent, c.accentHover, hoverProgress());
        bg = mix(bg, c.accentPressed, pressProgress());
        p.fillPath(shape, bg);
        if (m_busyProgress > 0.001 && !motion::reduced()) {
            // Sweeping highlight band while busy.
            p.save();
            p.setClipPath(shape);
            const qreal bandW = outer.width() * 0.45;
            const qreal x = outer.left() - bandW + (outer.width() + bandW) * m_phase;
            QLinearGradient g(x, 0, x + bandW, 0);
            QColor shine(255, 255, 255, 0);
            g.setColorAt(0, shine);
            shine.setAlphaF(0.22 * m_busyProgress);
            g.setColorAt(0.5, shine);
            shine.setAlpha(0);
            g.setColorAt(1, shine);
            p.fillRect(QRectF(x, outer.top(), bandW, outer.height()), g);
            p.restore();
        }
        fg = c.onAccent;
        break;
    }
    case Variant::Secondary: {
        p.fillPath(shape, mix(c.surface, c.surfaceAlt, hoverProgress()));
        if (pressProgress() > 0)
            p.fillPath(shape, withAlpha(c.pressed, pressProgress()));
        p.setPen(QPen(mix(c.border, c.borderStrong, hoverProgress()), 1));
        p.drawPath(shape);
        fg = c.text;
        break;
    }
    case Variant::Ghost: {
        p.fillPath(shape, withAlpha(c.hover, hoverProgress()));
        if (pressProgress() > 0)
            p.fillPath(shape, withAlpha(c.pressed, pressProgress()));
        fg = c.accent;
        break;
    }
    }

    p.setFont(font());
    const QFontMetricsF fm(font());
    auto drawLabel = [&](const QString &label, const QString &iconName, bool iconLeading, qreal alpha, qreal dy) {
        if (alpha <= 0.001)
            return;
        p.save();
        p.setOpacity(p.opacity() * alpha);
        const qreal textW = fm.horizontalAdvance(label);
        const qreal iconW = iconName.isEmpty() ? 0 : 16 + 8;
        const qreal total = textW + iconW;
        qreal x = outer.center().x() - total / 2.0;
        const qreal cy = outer.center().y() + dy;
        if (!iconName.isEmpty() && iconLeading) {
            if (iconName == QLatin1String("spinner")) {
                paintSpinner(&p, QRectF(x + 1, cy - 7, 14, 14), int(m_phase * 360 * 2) % 360, fg, 2.0);
            } else {
                p.drawPixmap(QPointF(x, cy - 8), iconPixmap(iconName, fg, 16, devicePixelRatioF()));
            }
            x += iconW;
        }
        p.setPen(fg);
        p.drawText(QRectF(x, cy - fm.height() / 2.0, textW + 2, fm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                   label);
        if (!iconName.isEmpty() && !iconLeading)
            p.drawPixmap(QPointF(x + textW + 8, cy - 8), iconPixmap(iconName, fg, 16, devicePixelRatioF()));
        p.restore();
    };
    const qreal t = m_busyProgress;
    drawLabel(text(), m_trailingIcon, false, 1.0 - t, -6.0 * t);
    if (!m_busyText.isEmpty())
        drawLabel(m_busyText, QStringLiteral("spinner"), true, t, 6.0 * (1.0 - t));
    p.restore();
    paintFocusRing(&p, QRectF(rect()), radius + 1);
}

// ---- SegmentedControl --------------------------------------------------------------------

SegmentedControl::SegmentedControl(QWidget *parent)
    : ButtonBase(parent)
{
    setMouseTracking(true);
    setFont(Theme::uiFont(9.5, QFont::Medium));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

int SegmentedControl::addSegment(const QString &text, const QString &toolTip)
{
    m_segments.append({text, toolTip});
    updateGeometry();
    updateAccessibleName();
    update();
    return int(m_segments.size()) - 1;
}

QString SegmentedControl::segmentText(int index) const { return m_segments.value(index).text; }

void SegmentedControl::setSegmentFont(const QFont &font)
{
    setFont(font);
    updateGeometry();
}

int SegmentedControl::segmentWidth() const
{
    int w = 0;
    for (const Segment &s : m_segments)
        w = qMax(w, fontMetrics().horizontalAdvance(s.text));
    return w + 28;
}

QSize SegmentedControl::sizeHint() const
{
    return QSize(int(m_segments.size()) * segmentWidth() + 8, qMax(34, fontMetrics().height() + 16));
}

QRect SegmentedControl::segmentRect(int index) const
{
    const int segW = segmentWidth();
    const int x0 = (width() - segW * int(m_segments.size())) / 2;
    return QRect(x0 + index * segW, 4, segW, height() - 8);
}

int SegmentedControl::segmentAt(const QPoint &pos) const
{
    for (int i = 0; i < m_segments.size(); ++i) {
        if (segmentRect(i).adjusted(0, -4, 0, 4).contains(pos))
            return i;
    }
    return -1;
}

bool SegmentedControl::hitButton(const QPoint &pos) const { return segmentAt(pos) >= 0; }

void SegmentedControl::setCurrentIndex(int index, bool animated)
{
    if (index < 0 || index >= m_segments.size())
        return;
    const QRectF from = motion::isRunning(this, QStringLiteral("pill")) ? m_pill : QRectF(segmentRect(m_current));
    m_current = index;
    updateAccessibleName();
    const QRectF to(segmentRect(index));
    if (!animated || !isVisible()) {
        motion::stop(this, QStringLiteral("pill"));
        m_pill = to;
        update();
        return;
    }
    m_pillFrom = from;
    motion::animate(this, QStringLiteral("pill"), 0.0, 1.0, motion::kNormal, [this](const QVariant &v) {
        m_pill = lerp(m_pillFrom, QRectF(segmentRect(m_current)), v.toReal());
        update();
    });
}

void SegmentedControl::selectByUser(int index)
{
    if (index < 0 || index >= m_segments.size() || index == m_current)
        return;
    setCurrentIndex(index, true);
    emit currentIndexChanged(index);
}

void SegmentedControl::updateAccessibleName()
{
    setAccessibleName(segmentText(m_current));
    setAccessibleDescription(m_segments.value(m_current).toolTip);
}

void SegmentedControl::paintEvent(QPaintEvent *)
{
    const ThemeColors &c = Theme::colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (!isEnabled())
        p.setOpacity(0.5);
    const QRectF outer = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
    const qreal radius = outer.height() / 2.0;
    QPainterPath shape;
    shape.addRoundedRect(outer, radius, radius);
    p.fillPath(shape, c.surfaceAlt);
    p.setPen(QPen(c.border, 1));
    p.drawPath(shape);

    // Sliding highlight.
    const QRectF pill = motion::isRunning(this, QStringLiteral("pill")) ? m_pill : QRectF(segmentRect(m_current));
    if (!m_segments.isEmpty()) {
        const qreal pr = pill.height() / 2.0;
        if (!c.dark)
            paintSoftShadow(&p, pill, pr, 1.0);
        QPainterPath pillPath;
        pillPath.addRoundedRect(pill, pr, pr);
        p.fillPath(pillPath, c.segmentPill);
        if (!c.dark) {
            p.setPen(QPen(withAlpha(c.border, 0.9), 1));
            p.drawPath(pillPath);
        }
    }

    p.setFont(font());
    for (int i = 0; i < m_segments.size(); ++i) {
        const QRect r = segmentRect(i);
        // Selected text colour follows the pill as it slides.
        const qreal overlap = qMax(0.0, qMin(pill.right(), qreal(r.right())) - qMax(pill.left(), qreal(r.left())))
                              / qMax(1, r.width());
        QColor col = mix(c.textMuted, c.text, qMax(overlap, i == m_hovered ? 0.7 : 0.0));
        p.setPen(col);
        p.drawText(r, Qt::AlignCenter, m_segments.at(i).text);
    }
    paintFocusRing(&p, QRectF(rect()), radius + 1);
}

void SegmentedControl::mouseMoveEvent(QMouseEvent *event)
{
    const int h = segmentAt(event->position().toPoint());
    if (h != m_hovered) {
        m_hovered = h;
        update();
    }
    ButtonBase::mouseMoveEvent(event);
}

void SegmentedControl::mouseReleaseEvent(QMouseEvent *event)
{
    const int i = segmentAt(event->position().toPoint());
    const bool wasDown = isDown();
    ButtonBase::mouseReleaseEvent(event);
    if (wasDown && event->button() == Qt::LeftButton && i >= 0)
        selectByUser(i);
}

void SegmentedControl::leaveEvent(QEvent *event)
{
    m_hovered = -1;
    update();
    ButtonBase::leaveEvent(event);
}

void SegmentedControl::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Left:
        selectByUser(m_current - 1);
        return;
    case Qt::Key_Right:
        selectByUser(m_current + 1);
        return;
    case Qt::Key_Home:
        selectByUser(0);
        return;
    case Qt::Key_End:
        selectByUser(count() - 1);
        return;
    default:
        break;
    }
    ButtonBase::keyPressEvent(event);
}

bool SegmentedControl::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        auto *he = static_cast<QHelpEvent *>(event);
        const int i = segmentAt(he->pos());
        if (i >= 0 && !m_segments.at(i).toolTip.isEmpty())
            QToolTip::showText(he->globalPos(), m_segments.at(i).toolTip, this, segmentRect(i));
        else
            QToolTip::hideText();
        return true;
    }
    return ButtonBase::event(event);
}

// ---- ToggleSwitch -------------------------------------------------------------------------

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : ButtonBase(parent)
{
    setCheckable(true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(this, &QAbstractButton::toggled, this, [this](bool on) {
        motion::animate(this, QStringLiteral("knob"), m_pos, on ? 1.0 : 0.0, motion::kNormal,
                        [this](const QVariant &v) {
                            m_pos = v.toReal();
                            update();
                        });
    });
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    const ThemeColors &c = Theme::colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (!isEnabled())
        p.setOpacity(0.45);
    // Snap when idle (e.g. setChecked before the widget was shown).
    if (!motion::isRunning(this, QStringLiteral("knob")))
        m_pos = isChecked() ? 1.0 : 0.0;

    const QRectF track = QRectF(rect()).adjusted(2, 2, -2, -2);
    const qreal r = track.height() / 2.0;
    QColor off = mix(c.switchOff, c.borderStrong, hoverProgress() * 0.6);
    QColor on = mix(c.accent, c.accentHover, hoverProgress());
    fillRounded(&p, track, r, mix(off, on, m_pos));

    const qreal d = track.height() - 6;
    const qreal stretch = 4.0 * pressProgress();
    const qreal travel = track.width() - d - 6 - stretch;
    const QRectF knob(track.left() + 3 + travel * m_pos, track.top() + 3, d + stretch, d);
    QColor shadow = c.shadow;
    shadow.setAlphaF(0.18);
    fillRounded(&p, knob.translated(0, 1), d / 2.0, shadow);
    fillRounded(&p, knob, d / 2.0, c.knob);
    paintFocusRing(&p, QRectF(rect()), r + 2);
}

// ---- SpeakButton -----------------------------------------------------------------------------

SpeakButton::SpeakButton(QWidget *parent, bool showText)
    : IconButton(QStringLiteral("speaker"), QString(), parent, IconTone::Text, 18)
    , m_showText(showText)
    , m_ticker(new QTimer(this))
{
    m_idleToolTip = tr("Listen");
    m_ticker->setInterval(16);
    connect(m_ticker, &QTimer::timeout, this, [this] {
        m_phase += 0.016;
        if (m_phase > 1000)
            m_phase = 0;
        update();
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

void SpeakButton::refresh()
{
    switch (m_state) {
    case State::Idle:
        setText(m_showText ? tr("Listen") : QString());
        setToolTip(m_idleToolTip);
        break;
    case State::Loading:
        setText(m_showText ? tr("Loading…") : QString());
        setToolTip(tr("Loading the voice… click to cancel"));
        break;
    case State::Speaking:
        setText(m_showText ? tr("Stop") : QString());
        setToolTip(withShortcut(tr("Stop"), QKeySequence(Qt::Key_Escape)));
        break;
    }
    setAccessibleName(m_state == State::Idle ? m_idleToolTip.section(QStringLiteral("  ("), 0, 0) : toolTip());
    if (m_state != State::Idle && !motion::reduced())
        m_ticker->start();
    else
        m_ticker->stop();
    m_phase = 0;
    updateGeometry();
    update();
}

QSize SpeakButton::sizeHint() const
{
    const QSize base = IconButton::sizeHint();
    if (!m_showText)
        return base;
    // Reserve the widest label so the row never jumps between states.
    int w = 0;
    for (const QString &label : {tr("Listen"), tr("Loading…"), tr("Stop")})
        w = qMax(w, fontMetrics().horizontalAdvance(label));
    return QSize(10 + glyphSize() + 6 + w + 12, base.height());
}

void SpeakButton::paintGlyph(QPainter *p, const QRectF &rect, const QColor &color)
{
    const QColor accent = isEnabled() ? Theme::colors().accent : color;
    const QPointF c = rect.center();
    const qreal s = glyphSize();
    switch (m_state) {
    case State::Idle:
        IconButton::paintGlyph(p, rect, color);
        return;
    case State::Loading: {
        // Three pulsing dots.
        p->save();
        p->setPen(Qt::NoPen);
        for (int i = 0; i < 3; ++i) {
            const qreal wave = 0.5 + 0.5 * std::sin(2 * kPi * (m_phase * 1.5) - i * 0.9);
            QColor dot = accent;
            dot.setAlphaF(0.35 + 0.65 * wave);
            const qreal radius = s * (0.09 + 0.035 * wave);
            p->setBrush(dot);
            p->drawEllipse(QPointF(c.x() + (i - 1) * s * 0.32, c.y()), radius, radius);
        }
        p->restore();
        return;
    }
    case State::Speaking: {
        // Animated sound-wave bars.
        p->save();
        const qreal barW = s * 0.13;
        const qreal gap = s * 0.11;
        const int bars = 4;
        const qreal total = bars * barW + (bars - 1) * gap;
        static const qreal speeds[bars] = {1.7, 2.3, 1.9, 2.6};
        static const qreal offsets[bars] = {0.0, 1.1, 2.3, 0.6};
        p->setPen(Qt::NoPen);
        p->setBrush(accent);
        for (int i = 0; i < bars; ++i) {
            const qreal wave = motion::reduced() ? 0.6 : 0.5 + 0.5 * std::sin(2 * kPi * m_phase * speeds[i] + offsets[i]);
            const qreal h = s * (0.25 + 0.6 * wave);
            const QRectF bar(c.x() - total / 2 + i * (barW + gap), c.y() - h / 2, barW, h);
            p->drawRoundedRect(bar, barW / 2, barW / 2);
        }
        p->restore();
        return;
    }
    }
}

// ---- DirectionPill ------------------------------------------------------------------------------

DirectionPill::DirectionPill(QWidget *parent)
    : ButtonBase(parent)
{
    QFont f = Theme::textFont(Language::Cantonese, 10.5);
    f.setWeight(QFont::Medium);
    setFont(f);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

int DirectionPill::labelWidth() const
{
    const QFontMetrics fm(font());
    return qMax(fm.horizontalAdvance(Theme::languageLabel(Language::English)),
                fm.horizontalAdvance(Theme::languageLabel(Language::Cantonese)))
           + 28;
}

QSize DirectionPill::sizeHint() const { return QSize(2 * labelWidth() + 40, qMax(38, fontMetrics().height() + 18)); }

void DirectionPill::setDirection(Direction direction, bool animated)
{
    if (direction == m_direction && !animated)
        return;
    m_previous = m_direction;
    m_direction = direction;
    setAccessibleName(tr("%1 to %2 - swap languages")
                          .arg(Theme::languageLabel(sourceLanguage(direction)),
                               Theme::languageLabel(targetLanguage(direction))));
    if (!animated || !isVisible()) {
        motion::stop(this, QStringLiteral("swap"));
        m_swapProgress = 1.0;
        update();
        return;
    }
    motion::animate(this, QStringLiteral("swap"), 0.0, 1.0, motion::kNormal, [this](const QVariant &v) {
        m_swapProgress = v.toReal();
        update();
    });
    const qreal startAngle = m_angle;
    motion::animate(
        this, QStringLiteral("rotate"), startAngle, startAngle + 180.0, motion::kSlow,
        [this](const QVariant &v) {
            m_angle = v.toReal();
            update();
        },
        [this] {
            m_angle = std::fmod(m_angle, 360.0);
            update();
        },
        motion::inOutCubic());
}

void DirectionPill::paintEvent(QPaintEvent *)
{
    const ThemeColors &c = Theme::colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF outer = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
    const qreal radius = outer.height() / 2.0;
    QPainterPath shape;
    shape.addRoundedRect(outer, radius, radius);
    if (!c.dark)
        paintSoftShadow(&p, outer, radius, 0.8);
    p.fillPath(shape, c.surface);
    p.setPen(QPen(mix(c.border, c.borderStrong, hoverProgress()), 1));
    p.drawPath(shape);

    // Swap disc in the middle.
    const qreal d = outer.height() - 8;
    const QRectF disc(outer.center().x() - d / 2, outer.center().y() - d / 2, d, d);
    const qreal scale = 1.0 - 0.08 * pressProgress();
    p.save();
    p.translate(disc.center());
    p.scale(scale, scale);
    p.setPen(Qt::NoPen);
    p.setBrush(mix(c.surfaceAlt, c.accentSoft, hoverProgress()));
    p.drawEllipse(QRectF(-d / 2, -d / 2, d, d));
    p.rotate(m_angle);
    const int iconSize = 18;
    const QPixmap icon = iconPixmap(QStringLiteral("swap"), mix(c.text, c.accent, hoverProgress()), iconSize,
                                    devicePixelRatioF());
    p.drawPixmap(QPointF(-iconSize / 2.0, -iconSize / 2.0), icon);
    p.restore();

    // Labels slide toward the centre and cross-fade when swapping.
    const int lw = labelWidth();
    const QRectF left(outer.left() + 4, outer.top(), lw, outer.height());
    const QRectF right(outer.right() - 4 - lw, outer.top(), lw, outer.height());
    const qreal t = m_swapProgress;
    const qreal dx = 16.0;
    p.setFont(font());
    auto draw = [&](const QRectF &r, const QString &text, qreal offset, qreal alpha) {
        if (alpha <= 0.001)
            return;
        p.save();
        p.setOpacity(alpha);
        p.setPen(c.text);
        p.drawText(r.translated(offset, 0), Qt::AlignCenter, text);
        p.restore();
    };
    const QString newSrc = Theme::languageLabel(sourceLanguage(m_direction));
    const QString newTgt = Theme::languageLabel(targetLanguage(m_direction));
    if (t < 1.0) {
        draw(left, Theme::languageLabel(sourceLanguage(m_previous)), dx * t, 1.0 - t);
        draw(right, Theme::languageLabel(targetLanguage(m_previous)), -dx * t, 1.0 - t);
    }
    draw(left, newSrc, -dx * (1.0 - t), t);
    draw(right, newTgt, dx * (1.0 - t), t);
    paintFocusRing(&p, QRectF(rect()), radius + 1);
}

} // namespace sct::ui
