#pragma once

#include "core/TranslationTypes.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QUuid>

namespace sct {

struct HistoryEntry
{
    QUuid id;
    TranslationResult result;
    bool starred = false;

    QJsonObject toJson() const;
    static HistoryEntry fromJson(const QJsonObject &obj);
};

// Persistent translation history (JSON file). Newest entries first.
// Starred entries are never evicted by the size limit (the limit counts unstarred entries only).
// Saves are atomic (QSaveFile) and happen automatically after each change.
class HistoryStore : public QObject
{
    Q_OBJECT

public:
    explicit HistoryStore(const QString &filePath, int maxEntries = 500, QObject *parent = nullptr);

    bool load();        // returns false if the file exists but is corrupt (store is then empty)
    bool save() const;  // returns false on I/O error

    const QList<HistoryEntry> &entries() const { return m_entries; }
    // Case-insensitive match on source text, translation and Jyutping.
    QList<HistoryEntry> search(const QString &query, bool starredOnly = false) const;

    // Adds a result to the top. If the newest entry has the same source text,
    // direction and translation, it is replaced instead of duplicated.
    QUuid add(const TranslationResult &result);
    bool setStarred(const QUuid &id, bool starred);
    bool remove(const QUuid &id);
    void clear(bool keepStarred = false);

    QString filePath() const { return m_filePath; }

signals:
    void changed();

private:
    void enforceLimit();

    QString m_filePath;
    int m_maxEntries;
    QList<HistoryEntry> m_entries;
};

} // namespace sct
