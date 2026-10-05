#include "ui/HistoryPanel.h"

#include "ui/Motion.h"
#include "ui/Theme.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QEnterEvent>
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
#include <QStackedWidget>
#include <QVBoxLayout>

namespace sct {

using ui::IconTone;

namespace {

constexpr int kRowHeight = 76;
constexpr int kIconSize = 17;

QString shortDirection(Direction d)
{
    return d == Direction::EnglishToCantonese ? QStringLiteral("EN → 粵") : QStringLiteral("粵 → EN");
}

QString oneLine(const QString &s) { return s.simplified(); }

} // namespace

// ---- HistoryRow ---------------------------------------------------------------------

HistoryRow::HistoryRow(const HistoryEntry &entry, QWidget *parent)
    : QWidget(parent)
    , m_entry(entry)
    , m_height(kRowHeight)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover);
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(kRowHeight);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setEntry(entry);
}

void HistoryRow::setEntry(const HistoryEntry &entry)
{
    m_entry = entry;
    setAccessibleName(QStringLiteral("%1 → %2").arg(oneLine(entry.result.request.text), oneLine(entry.result.translation)));
    QString tip = entry.result.request.text.trimmed().toHtmlEscaped() + QStringLiteral("<br><b>")
                  + entry.result.translation.trimmed().toHtmlEscaped() + QStringLiteral("</b>");
    if (!entry.result.jyutping.trimmed().isEmpty())
        tip += QStringLiteral("<br><i>") + entry.result.jyutping.trimmed().toHtmlEscaped() + QStringLiteral("</i>");
    setToolTip(tip);
    update();
}

void HistoryRow::setCurrent(bool current, bool listFocused)
{
    if (m_current == current && m_listFocused == listFocused)
        return;
    m_current = current;
    m_listFocused = listFocused;
    update();
}

QSize HistoryRow::sizeHint() const { return QSize(280, m_height); }

void HistoryRow::appear(int delayMs)
{
    motion::animate(this, QStringLiteral("appear"), 0.0, 1.0, motion::kSlow, [this](const QVariant &v) {
        m_opacity = v.toReal();
        update();
    }, {}, motion::outCubic(), delayMs);
}

void HistoryRow::collapseAndDelete()
{
    m_collapsing = true;
    setAttribute(Qt::WA_TransparentForMouseEvents);
    motion::animate(
        this, QStringLiteral("collapse"), 1.0, 0.0, motion::kNormal,
        [this](const QVariant &v) {
            const qreal t = v.toReal();
            m_opacity = t;
            m_height = qRound(kRowHeight * t);
            setFixedHeight(m_height);
            update();
        },
        [this] { deleteLater(); }, motion::inOutCubic());
}

QRect HistoryRow::starRect(const QRect &r) { return QRect(r.right() - 38, r.top() + 8, 30, 30); }

QRect HistoryRow::deleteRect(const QRect &r) { return QRect(r.right() - 38, r.bottom() - 37, 30, 30); }

HistoryRow::Hit HistoryRow::hitTest(const QPoint &pos) const
{
    if (!rect().contains(pos))
        return Hit::None;
    if (starRect(rect()).contains(pos))
        return Hit::Star;
    if (deleteRect(rect()).contains(pos))
        return Hit::Delete;
    return Hit::Row;
}

void HistoryRow::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    motion::animate(this, QStringLiteral("hover"), m_hover, 1.0, motion::kFast, [this](const QVariant &v) {
        m_hover = v.toReal();
        update();
    });
}

void HistoryRow::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_hoverHit = Hit::None;
    motion::animate(this, QStringLiteral("hover"), m_hover, 0.0, motion::kFast, [this](const QVariant &v) {
        m_hover = v.toReal();
        update();
    });
}

void HistoryRow::mouseMoveEvent(QMouseEvent *event)
{
    const Hit hit = hitTest(event->position().toPoint());
    if (hit != m_hoverHit) {
        m_hoverHit = hit;
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void HistoryRow::mousePressEvent(QMouseEvent *event)
{
    m_pressed = event->button() == Qt::LeftButton ? hitTest(event->position().toPoint()) : Hit::None;
    event->accept();
}

void HistoryRow::mouseReleaseEvent(QMouseEvent *event)
{
    const Hit hit = hitTest(event->position().toPoint());
    const Hit pressed = m_pressed;
    m_pressed = Hit::None;
    if (event->button() != Qt::LeftButton || hit != pressed)
        return;
    const QUuid id = m_entry.id;
    switch (hit) {
    case Hit::Star:
        emit starClicked(id);
        break;
    case Hit::Delete:
        emit deleteClicked(id);
        break;
    case Hit::Row:
        emit clicked(id);
        break;
    case Hit::None:
        break;
    }
}

void HistoryRow::contextMenuEvent(QContextMenuEvent *event)
{
    emit contextMenuRequested(m_entry.id, event->globalPos());
}

void HistoryRow::paintEvent(QPaintEvent *)
{
    if (m_opacity <= 0.001 || height() < 4)
        return;
    const ThemeColors &c = Theme::colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(m_opacity);
    p.setClipRect(rect());

    const QRectF card = QRectF(rect()).adjusted(2, 1, -2, -1);
    QPainterPath path;
    path.addRoundedRect(card, 10, 10);
    if (m_current)
        p.fillPath(path, c.accentSoft);
    p.fillPath(path, ui::withAlpha(c.hover, m_hover));
    if (m_current && m_listFocused) {
        p.setPen(QPen(c.accent, 1.5));
        p.drawPath(path);
    }

    const qreal dpr = devicePixelRatioF();
    const QRect full(0, 0, width(), kRowHeight);  // layout as if fully expanded
    const QRect text = full.adjusted(14, 9, -46, -9);
    const Direction dir = m_entry.result.request.direction;
    const ChineseScript script = m_entry.result.request.script;

    const QFont srcFont = Theme::textFont(sourceLanguage(dir), 9.5, script);
    const QFontMetrics fmSrc(srcFont);
    p.setFont(srcFont);
    p.setPen(c.textMuted);
    const QRect srcRect(text.left(), text.top(), text.width(), fmSrc.height());
    p.drawText(srcRect, Qt::AlignLeft | Qt::AlignVCenter,
               fmSrc.elidedText(oneLine(m_entry.result.request.text), Qt::ElideRight, srcRect.width()));

    QFont trFont = Theme::textFont(targetLanguage(dir), 11.5, script);
    trFont.setWeight(QFont::Medium);
    const QFontMetrics fmTr(trFont);
    p.setFont(trFont);
    p.setPen(c.text);
    const QRect trRect(text.left(), srcRect.bottom() + 3, text.width(), fmTr.height());
    p.drawText(trRect, Qt::AlignLeft | Qt::AlignVCenter,
               fmTr.elidedText(oneLine(m_entry.result.translation), Qt::ElideRight, trRect.width()));

    const QFont capFont = Theme::uiFont(8.5);
    const QFontMetrics fmCap(capFont);
    p.setFont(capFont);
    p.setPen(c.textMuted);
    const QRect capRect(text.left(), text.bottom() - fmCap.height() + 1, text.width(), fmCap.height());
    const QString caption = QStringLiteral("%1   ·   %2").arg(shortDirection(dir), HistoryPanel::relativeTime(m_entry.result.timestamp));
    p.drawText(capRect, Qt::AlignLeft | Qt::AlignVCenter, fmCap.elidedText(caption, Qt::ElideRight, capRect.width()));

    // Star: always shown when starred, otherwise fades in on hover.
    const qreal starAlpha = m_entry.starred ? 1.0 : m_hover;
    if (starAlpha > 0.001) {
        const QRect sr = starRect(full);
        p.save();
        p.setOpacity(m_opacity * starAlpha);
        if (m_hoverHit == Hit::Star)
            p.fillPath([&] { QPainterPath pp; pp.addRoundedRect(QRectF(sr), 8, 8); return pp; }(), c.hover);
        const QColor col = m_entry.starred ? c.star : (m_hoverHit == Hit::Star ? c.text : c.textMuted);
        p.drawPixmap(sr.center() - QPoint(kIconSize / 2, kIconSize / 2) + QPoint(1, 1),
                     ui::iconPixmap(m_entry.starred ? QStringLiteral("star-filled") : QStringLiteral("star"), col,
                                    kIconSize, dpr));
        p.restore();
    }
    if (m_hover > 0.001) {
        const QRect dr = deleteRect(full);
        p.save();
        p.setOpacity(m_opacity * m_hover);
        if (m_hoverHit == Hit::Delete)
            p.fillPath([&] { QPainterPath pp; pp.addRoundedRect(QRectF(dr), 8, 8); return pp; }(), c.hover);
        const QColor col = m_hoverHit == Hit::Delete ? c.errorText : c.textMuted;
        p.drawPixmap(dr.center() - QPoint(kIconSize / 2, kIconSize / 2) + QPoint(1, 1),
                     ui::iconPixmap(QStringLiteral("trash"), col, kIconSize, dpr));
        p.restore();
    }
}

// ---- HistoryList ---------------------------------------------------------------------

HistoryList::HistoryList(QWidget *parent)
    : QScrollArea(parent)
{
    setObjectName(QStringLiteral("historyList"));
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setAutoFillBackground(false);
    setAccessibleName(tr("History"));

    m_container = new QWidget(this);
    m_container->setAutoFillBackground(false);
    m_layout = new QVBoxLayout(m_container);
    m_layout->setContentsMargins(0, 0, 4, 0);
    m_layout->setSpacing(2);
    m_layout->addStretch(1);
    setWidget(m_container);
    m_container->setAutoFillBackground(false);  // setWidget() switches it on
}

HistoryRow *HistoryList::createRow(const HistoryEntry &entry)
{
    auto *row = new HistoryRow(entry, m_container);
    connect(row, &HistoryRow::clicked, this, [this](const QUuid &id) {
        const int i = indexOf(id);
        if (i < 0)
            return;
        setCurrentIndex(i);
        emit activated(m_rows.at(i)->entry());
    });
    connect(row, &HistoryRow::starClicked, this, &HistoryList::starClicked);
    connect(row, &HistoryRow::deleteClicked, this, &HistoryList::deleteClicked);
    connect(row, &HistoryRow::contextMenuRequested, this, &HistoryList::contextMenuRequested);
    return row;
}

int HistoryList::indexOf(const QUuid &id) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i)->entry().id == id)
            return i;
    }
    return -1;
}

void HistoryList::setEntries(const QList<HistoryEntry> &entries, bool animated)
{
    const QUuid currentId = (m_current >= 0 && m_current < m_rows.size()) ? m_rows.at(m_current)->entry().id : QUuid();
    const int previousIndex = m_current;

    QHash<QUuid, HistoryRow *> existing;
    for (HistoryRow *row : std::as_const(m_rows))
        existing.insert(row->entry().id, row);

    // Animate only simple changes: new rows at the top and/or removals, with
    // the survivors keeping their relative order.
    bool simple = animated && !motion::reduced();
    int prefix = 0;
    if (simple) {
        while (prefix < entries.size() && !existing.contains(entries.at(prefix).id))
            ++prefix;
        int last = -1;
        for (int i = prefix; i < entries.size() && simple; ++i) {
            const int oldIndex = int(m_rows.indexOf(existing.value(entries.at(i).id)));
            if (oldIndex < 0 || oldIndex < last)
                simple = false;
            last = oldIndex;
        }
    }

    QList<HistoryRow *> rows;
    rows.reserve(entries.size());
    for (const HistoryEntry &e : entries) {
        HistoryRow *row = existing.take(e.id);
        if (row)
            row->setEntry(e);
        rows.append(row);  // nullptr = new, created below
    }
    for (HistoryRow *gone : std::as_const(existing)) {
        if (simple)
            gone->collapseAndDelete();
        else
            delete gone;
    }

    if (simple) {
        for (int i = 0; i < prefix; ++i) {
            rows[i] = createRow(entries.at(i));
            m_layout->insertWidget(i, rows[i]);
            rows[i]->appear(40 * i);
        }
    } else {
        for (int i = 0; i < rows.size(); ++i) {
            if (!rows[i])
                rows[i] = createRow(entries.at(i));
            else
                m_layout->removeWidget(rows[i]);
        }
        for (int i = 0; i < rows.size(); ++i) {
            m_layout->insertWidget(i, rows[i]);
            rows[i]->show();
        }
    }
    m_rows = rows;
    m_current = currentId.isNull() ? -1 : indexOf(currentId);
    if (m_current < 0 && !currentId.isNull() && !m_rows.isEmpty())
        m_current = qMin(previousIndex, int(m_rows.size()) - 1);  // the current row was deleted
    updateCurrent();
}

void HistoryList::staggerIn()
{
    const int n = qMin(int(m_rows.size()), 12);
    for (int i = 0; i < n; ++i)
        m_rows.at(i)->appear(60 + 35 * i);
}

void HistoryList::setCurrentIndex(int index)
{
    if (m_rows.isEmpty()) {
        m_current = -1;
        return;
    }
    m_current = qBound(0, index, int(m_rows.size()) - 1);
    updateCurrent();
    ensureWidgetVisible(m_rows.at(m_current), 0, 8);
}

void HistoryList::updateCurrent()
{
    for (int i = 0; i < m_rows.size(); ++i)
        m_rows.at(i)->setCurrent(i == m_current, hasFocus());
}

void HistoryList::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Down:
        setCurrentIndex(m_current < 0 ? 0 : m_current + 1);
        return;
    case Qt::Key_Up:
        setCurrentIndex(m_current < 0 ? 0 : m_current - 1);
        return;
    case Qt::Key_Home:
        setCurrentIndex(0);
        return;
    case Qt::Key_End:
        setCurrentIndex(int(m_rows.size()) - 1);
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        if (m_current >= 0 && m_current < m_rows.size())
            emit activated(m_rows.at(m_current)->entry());
        return;
    case Qt::Key_Delete:
        if (m_current >= 0 && m_current < m_rows.size())
            emit deleteClicked(m_rows.at(m_current)->entry().id);
        return;
    default:
        break;
    }
    QScrollArea::keyPressEvent(event);
}

void HistoryList::focusInEvent(QFocusEvent *event)
{
    QScrollArea::focusInEvent(event);
    if (m_current < 0 && !m_rows.isEmpty())
        m_current = 0;
    updateCurrent();
}

void HistoryList::focusOutEvent(QFocusEvent *event)
{
    QScrollArea::focusOutEvent(event);
    updateCurrent();
}

// ---- HistoryPanel ------------------------------------------------------------------------

HistoryPanel::HistoryPanel(HistoryStore *store, QWidget *parent)
    : ui::Card(parent)
    , m_store(store)
{
    setObjectName(QStringLiteral("historyPanel"));
    auto *layout = new QVBoxLayout(this);
    const QMargins sm = shadowMargins();
    layout->setContentsMargins(sm.left() + 14, sm.top() + 12, sm.right() + 10, sm.bottom() + 10);
    layout->setSpacing(10);

    auto *header = new QHBoxLayout;
    header->setContentsMargins(4, 0, 0, 0);
    header->setSpacing(2);
    auto *title = new QLabel(tr("History"), this);
    title->setFont(Theme::uiFont(11.5, QFont::DemiBold));
    header->addWidget(title);
    header->addStretch(1);
    m_more = new ui::IconButton(QStringLiteral("more"), tr("More"), this, IconTone::Muted);
    m_more->setObjectName(QStringLiteral("historyMore"));
    connect(m_more, &QAbstractButton::clicked, this, [this] {
        QMenu menu(this);
        QAction *clear = menu.addAction(ui::icon(QStringLiteral("trash")), tr("Clear history…"));
        clear->setEnabled(m_store && !m_store->entries().isEmpty());
        connect(clear, &QAction::triggered, this, &HistoryPanel::confirmClear);
        menu.exec(m_more->mapToGlobal(QPoint(0, m_more->height() + 4)));
    });
    header->addWidget(m_more);
    auto *close = new ui::IconButton(QStringLiteral("close"),
                                     ui::withShortcut(tr("Close history"), QKeySequence(Qt::CTRL | Qt::Key_H)), this,
                                     IconTone::Muted);
    close->setObjectName(QStringLiteral("historyClose"));
    connect(close, &QAbstractButton::clicked, this, &HistoryPanel::closeRequested);
    header->addWidget(close);
    layout->addLayout(header);

    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("historySearch"));
    m_search->setPlaceholderText(tr("Search"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(ui::icon(QStringLiteral("search"), IconTone::Muted), QLineEdit::LeadingPosition);
    m_search->setAccessibleName(tr("Search history"));
    connect(m_search, &QLineEdit::textChanged, this, [this] { refresh(false); });
    layout->addWidget(m_search);

    auto *filters = new QHBoxLayout;
    filters->setContentsMargins(0, 0, 4, 0);
    m_starredOnly = new ui::IconButton(QStringLiteral("star"), tr("Show only starred translations"), this, IconTone::Muted, 16);
    m_starredOnly->setObjectName(QStringLiteral("starredOnly"));
    m_starredOnly->setText(tr("Starred"));
    m_starredOnly->setCheckable(true);
    m_starredOnly->setCheckedBackground(false);
    connect(m_starredOnly, &QAbstractButton::toggled, this, [this](bool on) {
        m_starredOnly->setIconName(on ? QStringLiteral("star-filled") : QStringLiteral("star"),
                                   on ? IconTone::Star : IconTone::Muted);
        motion::crossFade(m_stack, motion::kNormal);
        refresh(false);
    });
    filters->addWidget(m_starredOnly);
    filters->addStretch(1);
    m_count = new QLabel(this);
    m_count->setProperty("role", QStringLiteral("caption"));
    m_count->setFont(Theme::uiFont(8.5));
    filters->addWidget(m_count);
    layout->addLayout(filters);

    m_stack = new QStackedWidget(this);
    m_list = new HistoryList(m_stack);
    m_stack->addWidget(m_list);
    m_empty = new QLabel(m_stack);
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);
    m_empty->setProperty("role", QStringLiteral("muted"));
    m_empty->setContentsMargins(16, 16, 16, 48);
    m_stack->addWidget(m_empty);
    layout->addWidget(m_stack, 1);

    connect(m_list, &HistoryList::activated, this, &HistoryPanel::entryActivated);
    connect(m_list, &HistoryList::starClicked, this, &HistoryPanel::toggleStar);
    connect(m_list, &HistoryList::deleteClicked, this, &HistoryPanel::deleteEntry);
    connect(m_list, &HistoryList::contextMenuRequested, this, &HistoryPanel::showContextMenu);

    if (m_store)
        connect(m_store, &HistoryStore::changed, this, [this] { refresh(true); });
    refresh(false);
}

void HistoryPanel::refresh(bool animated)
{
    const QString query = m_search->text().trimmed();
    const bool starredOnly = m_starredOnly->isChecked();
    QList<HistoryEntry> entries;
    if (m_store)
        entries = m_store->search(query, starredOnly);
    m_list->setEntries(entries, animated && isVisible());

    const int n = int(entries.size());
    if (!query.isEmpty())
        m_count->setText(n == 1 ? tr("1 match") : tr("%1 matches").arg(n));
    else
        m_count->setText(n == 0 ? QString() : n == 1 ? tr("1 translation") : tr("%L1 translations").arg(n));

    const bool storeEmpty = !m_store || m_store->entries().isEmpty();
    if (!entries.isEmpty()) {
        m_stack->setCurrentWidget(m_list);
        return;
    }
    if (storeEmpty)
        m_empty->setText(tr("No translations yet.\nEverything you translate is saved here."));
    else if (!query.isEmpty())
        m_empty->setText(tr("Nothing matches “%1”.").arg(query));
    else
        m_empty->setText(tr("No starred translations yet.\nStar a result to keep it here."));
    m_stack->setCurrentWidget(m_empty);
}

int HistoryPanel::visibleCount() const { return m_list->count(); }

void HistoryPanel::setStarredOnly(bool on) { m_starredOnly->setChecked(on); }

void HistoryPanel::focusSearch()
{
    m_search->setFocus(Qt::ShortcutFocusReason);
    m_search->selectAll();
}

void HistoryPanel::activateRow(int row)
{
    if (HistoryRow *r = m_list->rowAt(row)) {
        m_list->setCurrentIndex(row);
        const HistoryEntry copy = r->entry();
        emit entryActivated(copy);
    }
}

const HistoryEntry *HistoryPanel::findEntry(const QUuid &id) const
{
    if (!m_store)
        return nullptr;
    for (const HistoryEntry &e : m_store->entries()) {
        if (e.id == id)
            return &e;
    }
    return nullptr;
}

void HistoryPanel::toggleStar(const QUuid &id)
{
    const HistoryEntry *e = findEntry(id);
    if (!e)
        return;
    const bool starred = !e->starred;
    m_store->setStarred(id, starred);
    emit statusMessage(starred ? tr("Starred") : tr("Star removed"), starred ? QStringLiteral("star-filled") : QString());
}

void HistoryPanel::deleteEntry(const QUuid &id)
{
    if (!findEntry(id))
        return;
    m_store->remove(id);
    emit statusMessage(tr("Deleted from history"), QStringLiteral("trash"));
}

void HistoryPanel::showContextMenu(const QUuid &id, const QPoint &globalPos)
{
    const HistoryEntry *e = findEntry(id);
    if (!e)
        return;
    const HistoryEntry entry = *e;
    QMenu menu(this);
    connect(menu.addAction(ui::icon(QStringLiteral("history")), tr("Open")), &QAction::triggered, this,
            [this, entry] { emit entryActivated(entry); });
    menu.addSeparator();
    connect(menu.addAction(ui::icon(QStringLiteral("copy")), tr("Copy translation")), &QAction::triggered, this,
            [this, entry] {
                QApplication::clipboard()->setText(entry.result.translation.trimmed());
                emit statusMessage(tr("Copied translation"), QStringLiteral("check"));
            });
    connect(menu.addAction(QIcon(), tr("Copy original text")), &QAction::triggered, this, [this, entry] {
        QApplication::clipboard()->setText(entry.result.request.text.trimmed());
        emit statusMessage(tr("Copied"), QStringLiteral("check"));
    });
    menu.addSeparator();
    connect(menu.addAction(ui::icon(entry.starred ? QStringLiteral("star") : QStringLiteral("star-filled"),
                                    entry.starred ? IconTone::Text : IconTone::Star),
                           entry.starred ? tr("Unstar") : tr("Star")),
            &QAction::triggered, this, [this, id] { toggleStar(id); });
    connect(menu.addAction(ui::icon(QStringLiteral("trash")), tr("Delete")), &QAction::triggered, this,
            [this, id] { deleteEntry(id); });
    menu.exec(globalPos);
}

void HistoryPanel::confirmClear()
{
    if (!m_store)
        return;
    QMessageBox box(QMessageBox::Question, tr("Clear history"), tr("Delete your translation history?"),
                    QMessageBox::NoButton, this);
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
    emit statusMessage(tr("History cleared"), QStringLiteral("trash"));
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
