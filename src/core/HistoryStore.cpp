#include "core/HistoryStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>

namespace sct {

namespace {

constexpr int kFormatVersion = 1;

// "nei5 hou2" -> "nei hou", so a search without tone numbers still matches.
QString stripToneNumbers(const QString &jyutping)
{
    static const QRegularExpression digits(QStringLiteral("[1-6]"));
    QString s = jyutping;
    s.remove(digits);
    return s;
}

bool matches(const HistoryEntry &e, const QString &needle)
{
    const TranslationResult &r = e.result;
    return r.request.text.contains(needle, Qt::CaseInsensitive)
           || r.translation.contains(needle, Qt::CaseInsensitive)
           || r.jyutping.contains(needle, Qt::CaseInsensitive)
           || stripToneNumbers(r.jyutping).contains(needle, Qt::CaseInsensitive);
}

} // namespace

QJsonObject HistoryEntry::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), id.toString(QUuid::WithoutBraces));
    o.insert(QStringLiteral("starred"), starred);
    o.insert(QStringLiteral("result"), result.toJson());
    return o;
}

HistoryEntry HistoryEntry::fromJson(const QJsonObject &obj)
{
    HistoryEntry e;
    e.id = QUuid::fromString(obj.value(QStringLiteral("id")).toString());
    e.starred = obj.value(QStringLiteral("starred")).toBool(false);
    e.result = TranslationResult::fromJson(obj.value(QStringLiteral("result")).toObject());
    return e;
}

HistoryStore::HistoryStore(const QString &filePath, int maxEntries, QObject *parent)
    : QObject(parent)
    , m_filePath(filePath)
    , m_maxEntries(maxEntries)
{
}

bool HistoryStore::load()
{
    m_entries.clear();

    QFile file(m_filePath);
    if (!file.exists()) {
        emit changed();
        return true;
    }

    bool ok = false;
    if (file.open(QIODevice::ReadOnly)) {
        QJsonParseError err{};
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
        file.close();
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonObject root = doc.object();
            const QJsonValue entries = root.value(QStringLiteral("entries"));
            if (entries.isArray()) {
                ok = true;
                for (const QJsonValue &v : entries.toArray()) {
                    if (!v.isObject())
                        continue;
                    HistoryEntry e = HistoryEntry::fromJson(v.toObject());
                    if (!e.result.isValid())
                        continue;
                    if (e.id.isNull())
                        e.id = QUuid::createUuid();
                    m_entries.append(e);
                }
            }
        }
    }

    if (!ok) {
        // Keep the unreadable file for inspection instead of overwriting it on
        // the next save.
        const QString backup = m_filePath + QStringLiteral(".corrupt");
        QFile::remove(backup);
        QFile::rename(m_filePath, backup);
        m_entries.clear();
    } else {
        enforceLimit();
    }
    emit changed();
    return ok;
}

bool HistoryStore::save() const
{
    const QFileInfo info(m_filePath);
    if (!QDir().mkpath(info.absolutePath()))
        return false;

    QJsonArray entries;
    for (const HistoryEntry &e : m_entries)
        entries.append(e.toJson());
    QJsonObject root;
    root.insert(QStringLiteral("version"), kFormatVersion);
    root.insert(QStringLiteral("entries"), entries);

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

QList<HistoryEntry> HistoryStore::search(const QString &query, bool starredOnly) const
{
    const QString needle = query.trimmed();
    QList<HistoryEntry> out;
    for (const HistoryEntry &e : m_entries) {
        if (starredOnly && !e.starred)
            continue;
        if (needle.isEmpty() || matches(e, needle))
            out.append(e);
    }
    return out;
}

QUuid HistoryStore::add(const TranslationResult &result)
{
    if (!result.isValid())
        return {};

    TranslationResult stored = result;
    stored.fromCache = false;
    if (!stored.timestamp.isValid())
        stored.timestamp = QDateTime::currentDateTimeUtc();

    QUuid id;
    if (!m_entries.isEmpty()) {
        HistoryEntry &newest = m_entries.first();
        const TranslationResult &prev = newest.result;
        if (prev.request.direction == stored.request.direction
            && prev.request.text.trimmed() == stored.request.text.trimmed()
            && prev.translation.trimmed() == stored.translation.trimmed()) {
            newest.result = stored;  // keep id and star
            id = newest.id;
        }
    }

    if (id.isNull()) {
        HistoryEntry e;
        e.id = QUuid::createUuid();
        e.result = stored;
        m_entries.prepend(e);
        id = e.id;
        enforceLimit();
    }

    save();
    emit changed();
    return id;
}

bool HistoryStore::setStarred(const QUuid &id, bool starred)
{
    for (HistoryEntry &e : m_entries) {
        if (e.id != id)
            continue;
        if (e.starred != starred) {
            e.starred = starred;
            enforceLimit();
            save();
            emit changed();
        }
        return true;
    }
    return false;
}

bool HistoryStore::remove(const QUuid &id)
{
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).id == id) {
            m_entries.removeAt(i);
            save();
            emit changed();
            return true;
        }
    }
    return false;
}

void HistoryStore::clear(bool keepStarred)
{
    if (keepStarred) {
        m_entries.removeIf([](const HistoryEntry &e) { return !e.starred; });
    } else {
        m_entries.clear();
    }
    save();
    emit changed();
}

void HistoryStore::enforceLimit()
{
    if (m_maxEntries <= 0)
        return;  // unlimited
    // The limit applies to unstarred entries only: starred entries are never
    // evicted and do not push out new translations either. The oldest
    // unstarred entries go first.
    qsizetype unstarred = 0;
    for (const HistoryEntry &e : std::as_const(m_entries))
        unstarred += e.starred ? 0 : 1;
    qsizetype excess = unstarred - m_maxEntries;
    for (qsizetype i = m_entries.size() - 1; i >= 0 && excess > 0; --i) {
        if (!m_entries.at(i).starred) {
            m_entries.removeAt(i);
            --excess;
        }
    }
}

} // namespace sct
