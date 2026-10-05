#pragma once

#include "ui/Controls.h"
#include "ui/Icons.h"

#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPointer>
#include <QWidget>

class QHBoxLayout;
class QTimer;
class QVBoxLayout;

namespace sct::ui {

// Rounded surface with a painted soft shadow. Secondary actions registered
// with addRevealWidget() fade in while the card is hovered or has focus.
class Card : public QFrame
{
    Q_OBJECT

public:
    explicit Card(QWidget *parent = nullptr);

    void addRevealWidget(IconButton *button);
    // Accent outline while a child has keyboard focus (the input card).
    void setFocusHighlight(bool on) { m_focusHighlight = on; }
    qreal revealProgress() const { return m_reveal; }
    static QMargins shadowMargins() { return QMargins(3, 2, 3, 5); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void updateState();

    QList<QPointer<IconButton>> m_revealWidgets;
    bool m_hovered = false;
    bool m_focusWithin = false;
    bool m_focusHighlight = false;
    qreal m_reveal = 0.0;
    qreal m_focus = 0.0;
};

// Shows the top part of a body widget: animating visibleHeight gives smooth
// expand/collapse without squeezing the body's layout.
class ClipBox : public QWidget
{
    Q_OBJECT

public:
    ClipBox(QWidget *body, QWidget *parent = nullptr);

    int fullHeight(int w) const;
    void setVisibleHeight(int h);  // -1 = everything
    int visibleHeight() const { return m_visible; }
    void relayout();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *m_body;
    int m_visible = -1;
};

// Quiet disclosure row ("3 other ways to say it  ›") whose content expands
// with an animated height, a rotating chevron and staggered children.
class Disclosure : public QWidget
{
    Q_OBJECT

public:
    explicit Disclosure(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    QString title() const;
    QVBoxLayout *contentLayout() const { return m_contentLayout; }
    QWidget *body() const { return m_body; }
    bool isExpanded() const { return m_expanded; }
    void setExpanded(bool expanded, bool animated = true);
    void contentChanged();

signals:
    void expandedChanged(bool expanded);

private:
    class Header;
    Header *m_header = nullptr;
    QWidget *m_body = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    ClipBox *m_clip = nullptr;
    bool m_expanded = false;
};

// Inline message strip (info / warning / error) with title, rich-text
// message, collapsible technical details and action buttons.
class Banner : public QFrame
{
    Q_OBJECT

public:
    enum class Kind { Info, Warning, Error };

    explicit Banner(Kind kind = Kind::Info, QWidget *parent = nullptr);

    void setKind(Kind kind);
    Kind kind() const { return m_kind; }
    void setTitle(const QString &title);
    void setText(const QString &text);
    void setDetails(const QString &details);
    void setClosable(bool closable);
    Button *addButton(const QString &text, bool primary = false);
    void clearButtons();
    // Fade in (and shake for errors).
    void animateIn(bool shake = false);

    QString title() const;
    QString text() const;
    QString details() const;
    bool detailsVisible() const;
    void setDetailsVisible(bool visible);
    QList<Button *> buttons() const { return m_buttons; }

signals:
    void closed();

protected:
    void changeEvent(QEvent *event) override;

private:
    void refreshIcon();

    Kind m_kind;
    QLabel *m_icon = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_text = nullptr;
    Button *m_detailsToggle = nullptr;
    QLabel *m_details = nullptr;
    QHBoxLayout *m_buttonRow = nullptr;
    IconButton *m_close = nullptr;
    QList<Button *> m_buttons;
};

// Shimmering placeholder lines shown while a translation loads.
class SkeletonView : public QWidget
{
    Q_OBJECT

public:
    explicit SkeletonView(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    QTimer *m_timer = nullptr;
    qreal m_phase = 0.0;
    qreal m_appear = 0.0;
};

// Transient notification that slides up from the bottom centre of a window.
class Toast : public QWidget
{
    Q_OBJECT

public:
    static Toast *showMessage(QWidget *window, const QString &text, const QString &iconName = QString(),
                              int msec = 2400);
    static Toast *current(QWidget *window);
    QString text() const { return m_text; }

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit Toast(QWidget *window);
    void popup(const QString &text, const QString &iconName, int msec);
    void dismiss();
    void reposition();

    QString m_text;
    QString m_icon;
    qreal m_progress = 0.0;
    QTimer *m_timer = nullptr;
};

// Side panel that slides open/closed by animating its width; the content
// stays anchored to the right edge so it appears to slide in.
class SidePanel : public QWidget
{
    Q_OBJECT

public:
    SidePanel(QWidget *content, int panelWidth, QWidget *parent = nullptr);

    void setOpen(bool open, bool animated = true);
    bool isOpen() const { return m_open; }
    QWidget *content() const { return m_content; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void openChanged(bool open);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void setCurrentWidth(int w);

    QWidget *m_content;
    int m_panelWidth;
    int m_width = 0;
    bool m_open = false;
};

// Password field with a show/hide "eye" toggle.
class PasswordLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit PasswordLineEdit(QWidget *parent = nullptr);
    void setRevealed(bool revealed);
    bool isRevealed() const;

private:
    QAction *m_toggle = nullptr;
};

// QLabel showing a tinted icon that follows theme changes.
class IconLabel : public QLabel
{
    Q_OBJECT

public:
    IconLabel(const QString &iconName, IconTone tone, int pixelSize, QWidget *parent = nullptr);
    void setIcon(const QString &iconName, IconTone tone);

protected:
    void changeEvent(QEvent *event) override;

private:
    void refresh();

    QString m_name;
    IconTone m_tone;
    int m_size;
};

// Thin horizontal rule.
QFrame *makeDivider(QWidget *parent);

} // namespace sct::ui
