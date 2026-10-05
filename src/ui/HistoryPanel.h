#pragma once

#include "core/HistoryStore.h"

#include <QAbstractListModel>
#include <QListView>
#include <QStyledItemDelegate>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QToolButton;

namespace sct {

// Flat list model over a filtered snapshot of HistoryStore entries.
class HistoryModel : public QAbstractListModel
{
    Q_OBJECT

public:
    using QAbstractListModel::QAbstractListModel;

    void setEntries(const QList<HistoryEntry> &entries);
    const HistoryEntry *entryAt(int row) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    QList<HistoryEntry> m_entries;
};

// Paints one history row: source, translation, direction + time, and the
// star / delete affordances on the right.
class HistoryDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    static QRect starRect(const QRect &itemRect);
    static QRect deleteRect(const QRect &itemRect);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

class HistoryListView : public QListView
{
    Q_OBJECT

public:
    explicit HistoryListView(QWidget *parent = nullptr);

signals:
    void starClicked(int row);
    void deleteClicked(int row);
    void rowClicked(int row);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    int m_pressedRow = -1;
    enum class Hit { None, Row, Star, Delete } m_pressedHit = Hit::None;
    Hit hitTest(const QPoint &pos, int *row) const;
};

// Contents of the History dock: search, starred filter, list, clear.
class HistoryPanel : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryPanel(HistoryStore *store, QWidget *parent = nullptr);

    void refresh();
    int visibleCount() const;
    void activateRow(int row);
    void setStarredOnly(bool on);

    QLineEdit *searchBox() const { return m_search; }
    HistoryListView *listView() const { return m_list; }
    HistoryModel *model() const { return m_model; }

    // "Just now", "5 min ago", "Yesterday 14:05", ...
    static QString relativeTime(const QDateTime &utc, const QDateTime &nowUtc = QDateTime());

signals:
    void entryActivated(const sct::HistoryEntry &entry);
    void statusMessage(const QString &message);

private:
    void toggleStar(int row);
    void deleteRow(int row);
    void showContextMenu(const QPoint &pos);
    void confirmClear();

    HistoryStore *m_store = nullptr;
    HistoryModel *m_model = nullptr;
    QLineEdit *m_search = nullptr;
    QToolButton *m_starredOnly = nullptr;
    QPushButton *m_clear = nullptr;
    QStackedWidget *m_stack = nullptr;
    HistoryListView *m_list = nullptr;
    QLabel *m_empty = nullptr;
};

} // namespace sct
