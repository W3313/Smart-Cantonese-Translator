#pragma once

#include "ui/Icons.h"

#include <QFrame>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QToolButton>
#include <QWidget>

class QAbstractButton;
class QButtonGroup;
class QHBoxLayout;
class QPushButton;
class QTimer;
class QVBoxLayout;

namespace sct::ui {

// "Copy translation (Ctrl+Shift+C)" - shortcut in the platform's native text.
QString withShortcut(const QString &text, const QKeySequence &shortcut);

// Flat icon-only tool button with a tinted icon and tooltip.
QToolButton *makeIconButton(const QString &iconName, const QString &toolTip, QWidget *parent,
                            IconTone tone = IconTone::Text, int iconSize = 18);

// A row of mutually exclusive, checkable buttons that looks like a pill
// ("Casual | Neutral | Polite").
class SegmentedControl : public QWidget
{
    Q_OBJECT

public:
    explicit SegmentedControl(QWidget *parent = nullptr);

    int addSegment(const QString &text, const QString &toolTip = QString());
    int count() const;
    int currentIndex() const;
    void setCurrentIndex(int index);  // does not emit currentIndexChanged
    QAbstractButton *button(int index) const;

signals:
    void currentIndexChanged(int index);  // user interaction only

private:
    QButtonGroup *m_group = nullptr;
    QHBoxLayout *m_layout = nullptr;
    QList<QToolButton *> m_buttons;
};

// Small indeterminate spinner.
class BusyIndicator : public QWidget
{
    Q_OBJECT

public:
    explicit BusyIndicator(int size = 28, QWidget *parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    QTimer *m_timer = nullptr;
    int m_angle = 0;
    int m_size = 28;
};

// Header button with a chevron that shows/hides a content area.
class CollapsibleSection : public QWidget
{
    Q_OBJECT

public:
    CollapsibleSection(const QString &title, const QString &iconName, QWidget *parent = nullptr);

    QVBoxLayout *contentLayout() const { return m_contentLayout; }
    QWidget *content() const { return m_content; }
    void setTitle(const QString &title);
    void setCount(int count);  // shown as a small "· 3" after the title; 0 hides it
    bool isExpanded() const;
    void setExpanded(bool expanded);

signals:
    void expandedChanged(bool expanded);

private:
    void updateHeader();

    QToolButton *m_header = nullptr;
    QWidget *m_content = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    QString m_title;
    int m_count = 0;
};

// Inline message strip (info / warning / error) with optional title,
// rich-text message, collapsible technical details and action buttons.
class Banner : public QFrame
{
    Q_OBJECT

public:
    enum class Kind { Info, Warning, Error };

    explicit Banner(Kind kind = Kind::Info, QWidget *parent = nullptr);

    void setKind(Kind kind);
    Kind kind() const { return m_kind; }
    void setTitle(const QString &title);
    void setText(const QString &text);  // rich text; links open externally unless handled
    void setDetails(const QString &details);  // empty hides the "Details" toggle
    void setClosable(bool closable);
    QPushButton *addButton(const QString &text, bool primary = false);
    void clearButtons();

    QString title() const;
    QString text() const;
    QString details() const;
    bool detailsVisible() const;
    void setDetailsVisible(bool visible);
    QList<QPushButton *> buttons() const { return m_buttons; }

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
    QPushButton *m_detailsToggle = nullptr;
    QLabel *m_details = nullptr;
    QHBoxLayout *m_buttonRow = nullptr;
    QToolButton *m_close = nullptr;
    QList<QPushButton *> m_buttons;
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

// Speaker button that turns into Stop while speaking and shows a spinner
// while (Azure) audio is loading. State is driven by SpeechController.
class SpeakButton : public QToolButton
{
    Q_OBJECT

public:
    enum class State { Idle, Loading, Speaking };

    explicit SpeakButton(QWidget *parent = nullptr, bool showText = false);

    void setSpeechState(State state);
    State speechState() const { return m_state; }
    // Tooltip shown while idle, e.g. "Listen (Ctrl+R)".
    void setIdleToolTip(const QString &toolTip);

protected:
    void changeEvent(QEvent *event) override;

private:
    void refresh();
    void updateSpinnerIcon();

    State m_state = State::Idle;
    bool m_showText = false;
    QString m_idleToolTip;
    QTimer *m_spinTimer = nullptr;
    int m_angle = 0;
};

// QLabel showing a tinted icon that follows theme changes.
class IconLabel : public QLabel
{
    Q_OBJECT

public:
    IconLabel(const QString &iconName, IconTone tone, int size, QWidget *parent = nullptr);
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
