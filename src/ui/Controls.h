#pragma once

#include "core/TranslationTypes.h"
#include "ui/Icons.h"

#include <QAbstractButton>
#include <QKeySequence>
#include <QList>
#include <QPointer>

class QTimer;
class QVariantAnimation;

namespace sct::ui {

// "Copy translation (Ctrl+Shift+C)" - shortcut in the platform's native text.
QString withShortcut(const QString &text, const QKeySequence &shortcut);

// Base for the custom-painted buttons: animated hover/press progress and a
// keyboard-only focus ring.
class ButtonBase : public QAbstractButton
{
    Q_OBJECT

public:
    explicit ButtonBase(QWidget *parent = nullptr);

protected:
    qreal hoverProgress() const { return m_hover; }
    qreal pressProgress() const { return m_press; }
    bool focusRingVisible() const { return hasFocus() && m_keyboardFocus; }
    void paintFocusRing(QPainter *p, const QRectF &rect, qreal radius) const;

    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;
    // Enter/Return clicks the focused button, like a QPushButton in a dialog
    // (otherwise the dialog's default action would run instead).
    void setClickOnEnter(bool on) { m_clickOnEnter = on; }

private:
    void animateHover(bool on);
    void animatePress(bool on);

    qreal m_hover = 0.0;
    qreal m_press = 0.0;
    bool m_keyboardFocus = false;
    bool m_clickOnEnter = true;
};

// Small ghost button with a tinted icon and optional text ("Copy").
// revealOpacity lets a card fade secondary actions in on hover.
class IconButton : public ButtonBase
{
    Q_OBJECT

public:
    IconButton(const QString &iconName, const QString &toolTip, QWidget *parent = nullptr,
               IconTone tone = IconTone::Text, int iconSize = 18);

    void setIconName(const QString &name, IconTone tone);
    QString iconName() const { return m_iconName; }
    void setRevealOpacity(qreal opacity);
    qreal revealOpacity() const { return m_reveal; }
    // Keep fully visible regardless of the card's hover reveal (e.g. a starred star).
    void setPinned(bool pinned);
    bool isPinned() const { return m_pinned; }
    // Tinted background while checked (off for e.g. a star toggle).
    void setCheckedBackground(bool on) { m_checkedBackground = on; update(); }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *event) override;
    // Content painter for subclasses (SpeakButton draws bars/dots instead).
    virtual void paintGlyph(QPainter *p, const QRectF &rect, const QColor &color);
    QColor contentColor() const;
    int glyphSize() const { return m_iconSize; }

private:
    QString m_iconName;
    IconTone m_tone;
    int m_iconSize;
    qreal m_reveal = 1.0;
    bool m_pinned = false;
    bool m_checkedBackground = true;
};

// Text button. Primary (accent fill), Secondary (outlined), Ghost (text only).
// A primary button can morph into a busy state ("Cancel" with a sweeping
// highlight) without changing size.
class Button : public ButtonBase
{
    Q_OBJECT

public:
    enum class Variant { Primary, Secondary, Ghost };

    Button(const QString &text, Variant variant = Variant::Secondary, QWidget *parent = nullptr);

    void setVariant(Variant variant);
    Variant variant() const { return m_variant; }
    // Trailing icon (e.g. the arrow on Translate).
    void setTrailingIcon(const QString &iconName);
    void setBusy(bool busy, const QString &busyText = QString());
    bool isBusy() const { return m_busy; }
    QString busyText() const { return m_busyText; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int contentWidth(const QString &text, bool withIcon) const;

    Variant m_variant;
    QString m_trailingIcon;
    QString m_busyText;
    bool m_busy = false;
    qreal m_busyProgress = 0.0;  // 0 = normal, 1 = busy
    qreal m_phase = 0.0;         // sweep/spinner phase while busy
    QVariantAnimation *m_loop = nullptr;
};

// Pill-shaped segmented control with a highlight that slides between segments.
class SegmentedControl : public ButtonBase
{
    Q_OBJECT

public:
    explicit SegmentedControl(QWidget *parent = nullptr);

    int addSegment(const QString &text, const QString &toolTip = QString());
    int count() const { return int(m_segments.size()); }
    int currentIndex() const { return m_current; }
    // Programmatic change: animates, but does not emit currentIndexChanged.
    void setCurrentIndex(int index, bool animated = true);
    QString segmentText(int index) const;
    QRect segmentRect(int index) const;
    void setSegmentFont(const QFont &font);
    // false: each segment is as wide as its text (default: all equal).
    void setEqualWidths(bool equal);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

signals:
    void currentIndexChanged(int index);  // user interaction only

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool event(QEvent *event) override;
    bool hitButton(const QPoint &pos) const override;

private:
    struct Segment
    {
        QString text;
        QString toolTip;
    };
    int segmentAt(const QPoint &pos) const;
    int segmentWidth(int index) const;
    int totalWidth() const;
    void selectByUser(int index);
    void updateAccessibleName();

    QList<Segment> m_segments;
    int m_current = 0;
    int m_hovered = -1;
    bool m_equalWidths = true;
    QRectF m_pill;      // animated highlight rect
    QRectF m_pillFrom;
};

// iOS/Windows-style on/off switch with an animated knob.
class ToggleSwitch : public ButtonBase
{
    Q_OBJECT

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);
    QSize sizeHint() const override { return QSize(42, 24); }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal m_pos = 0.0;  // 0 off .. 1 on
};

// Speaker button: speaker icon -> pulsing dots while Azure audio loads ->
// animated sound-wave bars while speaking (click again to stop).
class SpeakButton : public IconButton
{
    Q_OBJECT

public:
    enum class State { Idle, Loading, Speaking };

    explicit SpeakButton(QWidget *parent = nullptr, bool showText = false);

    void setSpeechState(State state);
    State speechState() const { return m_state; }
    void setIdleToolTip(const QString &toolTip);
    QSize sizeHint() const override;

protected:
    void paintGlyph(QPainter *p, const QRectF &rect, const QColor &color) override;

private:
    void refresh();

    State m_state = State::Idle;
    bool m_showText = false;
    QString m_idleToolTip;
    QTimer *m_ticker = nullptr;
    qreal m_phase = 0.0;
};

// "English  ⇄  廣東話 Cantonese": the whole pill swaps the direction; the
// arrow rotates and the labels slide/cross-fade on change.
class DirectionPill : public ButtonBase
{
    Q_OBJECT

public:
    explicit DirectionPill(QWidget *parent = nullptr);

    void setDirection(Direction direction, bool animated);
    Direction direction() const { return m_direction; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int labelWidth() const;

    Direction m_direction = Direction::EnglishToCantonese;
    Direction m_previous = Direction::EnglishToCantonese;
    qreal m_swapProgress = 1.0;  // 0 -> 1 while labels change
    qreal m_angle = 0.0;
};

} // namespace sct::ui
