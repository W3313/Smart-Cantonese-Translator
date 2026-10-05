#include "ui/HistoryPanel.h"

#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace sct {

using ui::IconTone;

namespace {

constexpr int kRowHeight = 78;
constexpr int kIconSize = 18;

QString shortDirection(Direction d)
{
    return d == Direction::EnglishToCantonese ? QStringLiteral("EN → 粵") : QStringLiteral("粵 → EN");
}

QString oneLine(const QString &s) { return s.simplified(); }

} // namespace

// ---- HistoryModel ----------------------------------------------------------------

void HistoryModel::setEntries(const QList<HistoryEntry> &entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

const HistoryEntry *HistoryModel::entryAt(int row) const
{
    return (row >= 0 && row < m_entries.size()) ? &m_entries.at(row) : nullptr;
}

int HistoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant HistoryModel::data(const QModelIndex &index, int role) const
{
    const HistoryEntry *e = entryAt(index.row());
    if (!e)
        return {};
    switch (role) {
    case Qt::DisplayRole:
    case Qt::AccessibleTextRole:
        return QStringLiteral("%1 → %2").arg(oneLine(e->result.request.text), oneLine(e->result.translation));
    case Qt::ToolTipRole: {
        QString tip = e->result.request.text.trimmed().toHtmlEscaped() + QStringLiteral("<br><b>")
                      + e->result.translation.trimmed().toHtmlEscaped() + QStringLiteral("</b>");
        if (!e->result.jyutping.trimmed().isEmpty())
            tip += QStringLiteral("<br><i>") + e->result.jyutping.trimmed().toHtmlEscaped() + QStringLiteral("</i>");
        return tip;
    }
    default:
        return {};
    }
}

// ---- HistoryDelegate ------------------------------------------------------------

QRect HistoryDelegate::starRect(const QRect &itemRect)
{
    const QRect r = itemRect.adjusted(4, 2, -4, -2);
    return QRect(r.right() - 34, r.top() + 8, 28, 28);
}

QRect HistoryDelegate::deleteRect(const QRect &itemRect)
{
    const QRect r = itemRect.adjusted(4, 2, -4, -2);
    return QRect(r.right() - 34, r.bottom() - 34, 28, 28);
}

QSize HistoryDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const
{
    return QSize(option.rect.width() > 0 ? option.rect.width() : 240, kRowHeight);
}

void HistoryDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const auto *model = qobject_cast<const HistoryModel *>(index.model());
    const HistoryEntry *e = model ? model->entryAt(index.row()) : nullptr;
    if (!e)
        return;
    const ThemeColors &c = Theme::colors();
    const bool hovered = option.state & QStyle::State_MouseOver;
    const bool selected = option.state & QStyle::State_Selected;
    const bool focused = option.state & QStyle::State_HasFocus;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QRect card = option.rect.adjusted(4, 2, -4, -2);
    if (selected || hovered) {
        QPainterPath path;
        path.addRoundedRect(QRectF(card), 9, 9);
        painter->fillPath(path, selected ? c.accentSoft : c.hover);
    }
    if (focused && selected) {
        painter->setPen(QPen(c.accent, 1));
        painter->drawRoundedRect(QRectF(card).adjusted(0.5, 0.5, -0.5, -0.5), 9, 9);
    }

    const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
    const QRect text = card.adjusted(12, 8, -44, -8);
    const Direction dir = e->result.request.direction;

    // Source (muted, small)
    QFont srcFont = Theme::textFont(sourceLanguage(dir), 9.5, e->result.request.script);
    painter->setFont(srcFont);
    painter->setPen(c.textMuted);
    QFontMetrics fmSrc(srcFont);
    const QRect srcRect(text.left(), text.top(), text.width(), fmSrc.height());
    painter->drawText(srcRect, Qt::AlignLeft | Qt::AlignVCenter,
                      fmSrc.elidedText(oneLine(e->result.request.text), Qt::ElideRight, srcRect.width()));

    // Translation (primary)
    QFont trFont = Theme::textFont(targetLanguage(dir), 11.5, e->result.request.script);
    painter->setFont(trFont);
    painter->setPen(c.text);
    QFontMetrics fmTr(trFont);
    const QRect trRect(text.left(), srcRect.bottom() + 3, text.width(), fmTr.height());
    painter->drawText(trRect, Qt::AlignLeft | Qt::AlignVCenter,
                      fmTr.elidedText(oneLine(e->result.translation), Qt::ElideRight, trRect.width()));

    // Direction · time
    QFont capFont = Theme::uiFont(8.5);
    painter->setFont(capFont);
    painter->setPen(c.textMuted);
    QFontMetrics fmCap(capFont);
    const QRect capRect(text.left(), text.bottom() - fmCap.height() + 1, text.width(), fmCap.height());
    const QString caption = QStringLiteral("%1  ·  %2").arg(shortDirection(dir), HistoryPanel::relativeTime(e->result.timestamp));
    painter->drawText(capRect, Qt::AlignLeft | Qt::AlignVCenter, fmCap.elidedText(caption, Qt::ElideRight, capRect.width()));

    // Star (always visible when starred, otherwise on hover/selection)
    const QRect star = starRect(option.rect);
    if (e->starred || hovered || selected) {
        const QPixmap pm = ui::iconPixmap(e->starred ? QStringLiteral("star-filled") : QStringLiteral("star"),
                                          e->starred ? c.star : c.textMuted, kIconSize, dpr);
        painter->drawPixmap(star.center() - QPoint(kIconSize / 2, kIconSize / 2), pm);
    }
    if (hovered || selected) {
        const QRect del = deleteRect(option.rect);
        const QPixmap pm = ui::iconPixmap(QStringLiteral("trash"), c.textMuted, kIconSize - 2, dpr);
        painter->drawPixmap(del.center() - QPoint((kIconSize - 2) / 2, (kIconSize - 2) / 2), pm);
    }
    painter->restore();
}

// ---- HistoryListView ----------------------------------------------------------------

HistoryListView::HistoryListView(QWidget *parent)
    : QListView(parent)
{
    setObjectName(QStringLiteral("historyList"));
    setMouseTracking(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setUniformItemSizes(true);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setFrameShape(QFrame::NoFrame);
    viewport()->setAttribute(Qt::WA_Hover, true);
}

HistoryListView::Hit HistoryListView::hitTest(const QPoint &pos, int *row) const
{
    const QModelIndex index = indexAt(pos);
    if (!index.isValid())
        return Hit::None;
    *row = index.row();
    const QRect r = visualRect(index);
    if (HistoryDelegate::starRect(r).contains(pos))
        return Hit::Star;
    if (HistoryDelegate::deleteRect(r).contains(pos))
        return Hit::Delete;
    return Hit::Row;
}

void HistoryListView::mousePressEvent(QMouseEvent *event)
{
    int row = -1;
    const Hit hit = event->button() == Qt::LeftButton ? hitTest(event->position().toPoint(), &row) : Hit::None;
    m_pressedHit = hit;
    m_pressedRow = row;
    if (hit == Hit::Star || hit == Hit::Delete) {
        event->accept();
        return;
    }
    QListView::mousePressEvent(event);
}

void HistoryListView::mouseReleaseEvent(QMouseEvent *event)
{
    int row = -1;
    const Hit hit = event->button() == Qt::LeftButton ? hitTest(event->position().toPoint(), &row) : Hit::None;
    const Hit pressed = m_pressedHit;
    const int pressedRow = m_pressedRow;
    m_pressedHit = Hit::None;
    m_pressedRow = -1;
    if (pressed == Hit::Star || pressed == Hit::Delete) {
        event->accept();
        if (hit == pressed && row == pressedRow) {
            if (hit == Hit::Star)
                emit starClicked(row);
            else
                emit deleteClicked(row);
        }
        return;
    }
    QListView::mouseReleaseEvent(event);
    if (pressed == Hit::Row && hit == Hit::Row && row == pressedRow)
        emit rowClicked(row);
}

void HistoryListView::mouseMoveEvent(QMouseEvent *event)
{
    int row = -1;
    const Hit hit = hitTest(event->position().toPoint(), &row);
    viewport()->setCursor(hit == Hit::None ? Qt::ArrowCursor : Qt::PointingHandCursor);
    if (hit == Hit::Star)
        setToolTip(tr("Star / unstar"));
    else if (hit == Hit::Delete)
        setToolTip(tr("Delete from history"));
    else
        setToolTip(QString());
    QListView::mouseMoveEvent(event);
}

void HistoryListView::leaveEvent(QEvent *event)
{
    viewport()->unsetCursor();
    QListView::leaveEvent(event);
}

// ---- HistoryPanel ---------------------------------------------------------------------

HistoryPanel::HistoryPanel(HistoryStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
    , m_model(new HistoryModel(this))
{
    setObjectName(QStringLiteral("historyPanel"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 4, 12, 4);
    layout->setSpacing(8);

    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("historySearch"));
    m_search->setPlaceholderText(tr("Search history"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(ui::icon(QStringLiteral("search"), IconTone::Muted), QLineEdit::LeadingPosition);
    m_search->setAccessibleName(tr("Search history"));
    connect(m_search, &QLineEdit::textChanged, this, &HistoryPanel::refresh);
    layout->addWidget(m_search);

    auto *row = new QHBoxLayout;
    row->setSpacing(6);
    m_starredOnly = new QToolButton(this);
    m_starredOnly->setObjectName(QStringLiteral("starredOnly"));
    m_starredOnly->setText(tr("Starred only"));
    m_starredOnly->setCheckable(true);
    m_starredOnly->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_starredOnly->setIcon(ui::icon(QStringLiteral("star"), IconTone::Text));
    m_starredOnly->setIconSize(QSize(16, 16));
    m_starredOnly->setCursor(Qt::PointingHandCursor);
    m_starredOnly->setToolTip(tr("Show only starred translations"));
    connect(m_starredOnly, &QToolButton::toggled, this, [this](bool on) {
        m_starredOnly->setIcon(ui::icon(on ? QStringLiteral("star-filled") : QStringLiteral("star"),
                                        on ? IconTone::Star : IconTone::Text));
        refresh();
    });
    row->addWidget(m_starredOnly);
    row->addStretch(1);
    m_clear = new QPushButton(tr("Clear…"), this);
    m_clear->setObjectName(QStringLiteral("clearHistory"));
    m_clear->setCursor(Qt::PointingHandCursor);
    m_clear->setToolTip(tr("Delete all history"));
    connect(m_clear, &QPushButton::clicked, this, &HistoryPanel::confirmClear);
    row->addWidget(m_clear);
    layout->addLayout(row);

    m_stack = new QStackedWidget(this);
    m_list = new HistoryListView(m_stack);
    m_list->setModel(m_model);
    m_list->setItemDelegate(new HistoryDelegate(m_list));
    m_stack->addWidget(m_list);

    m_empty = new QLabel(m_stack);
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);
    m_empty->setProperty("role", QStringLiteral("muted"));
    m_empty->setMargin(24);
    m_stack->addWidget(m_empty);
    layout->addWidget(m_stack, 1);

    connect(m_list, &HistoryListView::rowClicked, this, &HistoryPanel::activateRow);
    connect(m_list, &QListView::activated, this, [this](const QModelIndex &idx) { activateRow(idx.row()); });
    connect(m_list, &HistoryListView::starClicked, this, &HistoryPanel::toggleStar);
    connect(m_list, &HistoryListView::deleteClicked, this, &HistoryPanel::deleteRow);
    connect(m_list, &QWidget::customContextMenuRequested, this, &HistoryPanel::showContextMenu);

    auto *del = new QShortcut(QKeySequence(QKeySequence::Delete), m_list);
    del->setContext(Qt::WidgetShortcut);
    connect(del, &QShortcut::activated, this, [this] { deleteRow(m_list->currentIndex().row()); });

    if (m_store)
        connect(m_store, &HistoryStore::changed, this, &HistoryPanel::refresh);
    refresh();
}

void HistoryPanel::refresh()
{
    const QString query = m_search->text().trimmed();
    const bool starredOnly = m_starredOnly->isChecked();
    const int previousRow = m_list->currentIndex().row();
    const QUuid previousId = m_model->entryAt(previousRow) ? m_model->entryAt(previousRow)->id : QUuid();

    QList<HistoryEntry> entries;
    if (m_store)
        entries = m_store->search(query, starredOnly);
    m_model->setEntries(entries);

    // Keep the selection on the same entry across refreshes.
    if (!previousId.isNull()) {
        for (int i = 0; i < entries.size(); ++i) {
            if (entries.at(i).id == previousId) {
                m_list->setCurrentIndex(m_model->index(i));
                break;
            }
        }
    }

    const bool storeEmpty = !m_store || m_store->entries().isEmpty();
    m_clear->setEnabled(!storeEmpty);
    if (!entries.isEmpty()) {
        m_stack->setCurrentWidget(m_list);
        return;
    }
    if (storeEmpty)
        m_empty->setText(tr("No translations yet.\nEverything you translate is saved here."));
    else if (!query.isEmpty())
        m_empty->setText(tr("No matches for “%1”.").arg(query));
    else
        m_empty->setText(tr("No starred translations yet.\nClick the star on a result to keep it here."));
    m_stack->setCurrentWidget(m_empty);
}

int HistoryPanel::visibleCount() const { return m_model->rowCount(); }

void HistoryPanel::setStarredOnly(bool on) { m_starredOnly->setChecked(on); }

void HistoryPanel::activateRow(int row)
{
    if (const HistoryEntry *e = m_model->entryAt(row)) {
        const HistoryEntry copy = *e;  // the model may reset while handling the signal
        emit entryActivated(copy);
    }
}

void HistoryPanel::toggleStar(int row)
{
    const HistoryEntry *e = m_model->entryAt(row);
    if (!e || !m_store)
        return;
    const QUuid id = e->id;
    const bool starred = !e->starred;
    m_store->setStarred(id, starred);
    emit statusMessage(starred ? tr("Starred") : tr("Unstarred"));
}

void HistoryPanel::deleteRow(int row)
{
    const HistoryEntry *e = m_model->entryAt(row);
    if (!e || !m_store)
        return;
    const QUuid id = e->id;
    m_store->remove(id);
    emit statusMessage(tr("Deleted from history"));
    // Keep keyboard focus on a neighbouring row.
    const int count = m_model->rowCount();
    if (count > 0)
        m_list->setCurrentIndex(m_model->index(qMin(row, count - 1)));
}

void HistoryPanel::showContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_list->indexAt(pos);
    const HistoryEntry *e = m_model->entryAt(index.row());
    if (!e)
        return;
    const int row = index.row();
    const QString translation = e->result.translation.trimmed();
    const QString source = e->result.request.text.trimmed();
    const bool starred = e->starred;

    QMenu menu(this);
    auto add = [&menu](const QIcon &icon, const QString &text) { return menu.addAction(icon, text); };
    connect(add(ui::icon(QStringLiteral("history")), tr("Open")), &QAction::triggered, this,
            [this, row] { activateRow(row); });
    menu.addSeparator();
    connect(add(ui::icon(QStringLiteral("copy")), tr("Copy translation")), &QAction::triggered, this,
            [this, translation] {
                QApplication::clipboard()->setText(translation);
                emit statusMessage(tr("Copied translation"));
            });
    connect(add(QIcon(), tr("Copy original text")), &QAction::triggered, this, [this, source] {
        QApplication::clipboard()->setText(source);
        emit statusMessage(tr("Copied"));
    });
    menu.addSeparator();
    connect(add(ui::icon(starred ? QStringLiteral("star") : QStringLiteral("star-filled"),
                         starred ? IconTone::Text : IconTone::Star),
                starred ? tr("Unstar") : tr("Star")),
            &QAction::triggered, this, [this, row] { toggleStar(row); });
    connect(add(ui::icon(QStringLiteral("trash")), tr("Delete")), &QAction::triggered, this,
            [this, row] { deleteRow(row); });
    menu.exec(m_list->viewport()->mapToGlobal(pos));
}

void HistoryPanel::confirmClear()
{
    if (!m_store)
        return;
    QMessageBox box(QMessageBox::Question, tr("Clear history"),
                    tr("Delete your translation history?"), QMessageBox::NoButton, this);
    box.setInformativeText(tr("Starred translations are kept unless you choose otherwise."));
    auto *alsoStarred = new QCheckBox(tr("Also delete starred translations"), &box);
    box.setCheckBox(alsoStarred);
    QPushButton *clear = box.addButton(tr("Clear history"), QMessageBox::DestructiveRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() != clear)
        return;
    m_store->clear(!alsoStarred->isChecked());
    emit statusMessage(tr("History cleared"));
}

QString HistoryPanel::relativeTime(const QDateTime &utc, const QDateTime &nowUtc)
{
    if (!utc.isValid())
        return QString();
    const QDateTime now = (nowUtc.isValid() ? nowUtc : QDateTime::currentDateTimeUtc()).toLocalTime();
    const QDateTime t = utc.toLocalTime();
    const qint64 secs = t.secsTo(now);
    const QLocale locale;
    const QString time = locale.toString(t.time(), QLocale::ShortFormat);
    if (secs < 60)
        return tr("Just now");
    if (secs < 3600)
        return tr("%n min ago", nullptr, int(secs / 60));
    const qint64 days = t.date().daysTo(now.date());
    if (days == 0)
        return tr("Today %1").arg(time);
    if (days == 1)
        return tr("Yesterday %1").arg(time);
    if (days < 7)
        return QStringLiteral("%1 %2").arg(locale.dayName(t.date().dayOfWeek(), QLocale::ShortFormat), time);
    return locale.toString(t.date(), QLocale::ShortFormat);
}

} // namespace sct
