#pragma once

#include "core/HistoryStore.h"
#include "ui/Surfaces.h"

#include <QHash>
#include <QScrollArea>
#include <QUuid>

class QLabel;
class QLineEdit;
class QStackedWidget;
class QVBoxLayout;

namespace sct {

// One custom-painted history entry: source, translation, direction + time,
// with star / delete affordances that fade in on hover. Rows fade in when
// added and collapse smoothly when deleted.
class HistoryRow : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryRow(const HistoryEntry &entry, QWidget *parent = nullptr);

    void setEntry(const HistoryEntry &entry);
    const HistoryEntry &entry() const { return m_entry; }
    void setCurrent(bool current, bool listFocused);
    void appear(int delayMs);
    void collapseAndDelete();
    bool isCollapsing() const { return m_collapsing; }

    static QRect starRect(const QRect &r);
    static QRect deleteRect(const QRect &r);

    QSize sizeHint() const override;

signals:
    void clicked(const QUuid &id);
    void starClicked(const QUuid &id);
    void deleteClicked(const QUuid &id);
    void contextMenuRequested(const QUuid &id, const QPoint &globalPos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    enum class Hit { None, Row, Star, Delete };
    Hit hitTest(const QPoint &pos) const;

    HistoryEntry m_entry;
    qreal m_opacity = 1.0;
    qreal m_hover = 0.0;
    int m_height;
    bool m_current = false;
    bool m_listFocused = false;
    bool m_collapsing = false;
    Hit m_pressed = Hit::None;
    Hit m_hoverHit = Hit::None;
};

// Scrollable list of HistoryRows with keyboard navigation (Up/Down, Enter,
// Delete) and diff-based, animated updates.
class HistoryList : public QScrollArea
{
    Q_OBJECT

public:
    explicit HistoryList(QWidget *parent = nullptr);

    // animated: fade new rows in / collapse removed rows (store changes);
    // false for filter changes.
    void setEntries(const QList<HistoryEntry> &entries, bool animated);
    int count() const { return int(m_rows.size()); }
    HistoryRow *rowAt(int index) const { return m_rows.value(index); }
    int currentIndex() const { return m_current; }
    void setCurrentIndex(int index);
    // Fade the first rows in one after another (panel opening).
    void staggerIn();

signals:
    void activated(const sct::HistoryEntry &entry);
    void starClicked(const QUuid &id);
    void deleteClicked(const QUuid &id);
    void contextMenuRequested(const QUuid &id, const QPoint &globalPos);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    HistoryRow *createRow(const HistoryEntry &entry);
    int indexOf(const QUuid &id) const;
    void updateCurrent();

    QWidget *m_container = nullptr;
    QVBoxLayout *m_layout = nullptr;
    QList<HistoryRow *> m_rows;
    int m_current = -1;
};

// Contents of the slide-in History panel: search, starred filter, list.
class HistoryPanel : public ui::Card
{
    Q_OBJECT

public:
    explicit HistoryPanel(HistoryStore *store, QWidget *parent = nullptr);

    void refresh(bool animated = false);
    int visibleCount() const;
    void activateRow(int row);
    void setStarredOnly(bool on);
    void focusSearch();

    QLineEdit *searchBox() const { return m_search; }
    HistoryList *list() const { return m_list; }

    // "Just now", "5 min ago", "Yesterday 14:05", ...
    static QString relativeTime(const QDateTime &utc, const QDateTime &nowUtc = QDateTime());

signals:
    void entryActivated(const sct::HistoryEntry &entry);
    void statusMessage(const QString &message, const QString &iconName);
    void closeRequested();

private:
    void toggleStar(const QUuid &id);
    void deleteEntry(const QUuid &id);
    void showContextMenu(const QUuid &id, const QPoint &globalPos);
    void confirmClear();
    const HistoryEntry *findEntry(const QUuid &id) const;

    HistoryStore *m_store = nullptr;
    QLineEdit *m_search = nullptr;
    ui::IconButton *m_starredOnly = nullptr;
    ui::IconButton *m_more = nullptr;
    QLabel *m_count = nullptr;
    QStackedWidget *m_stack = nullptr;
    HistoryList *m_list = nullptr;
    QLabel *m_empty = nullptr;
};

} // namespace sct
