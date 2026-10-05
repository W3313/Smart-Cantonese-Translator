#pragma once

// On-disk cache of synthesized audio (Azure mp3), so speaking the same text
// again costs nothing and works offline. Files are named by
// SHA-1(voice|rate|text) and evicted least-recently-used beyond a file-count
// and total-size limit.

#include <QByteArray>
#include <QString>

namespace sct {

class AudioCache
{
public:
    static constexpr int DefaultMaxFiles = 200;
    static constexpr qint64 DefaultMaxBytes = 50LL * 1024 * 1024;

    explicit AudioCache(const QString &directory = QString(), int maxFiles = DefaultMaxFiles,
                        qint64 maxBytes = DefaultMaxBytes);

    void setDirectory(const QString &directory);
    QString directory() const { return m_dir; }
    void setLimits(int maxFiles, qint64 maxBytes);
    int maxFiles() const { return m_maxFiles; }
    qint64 maxBytes() const { return m_maxBytes; }

    // Hex SHA-1 of "voice|rate|text" (rate rounded to 2 decimals), so equal
    // requests always map to the same file.
    static QString key(const QString &voice, double rate, const QString &text);

    // Path where the audio for key is (or would be) stored.
    QString filePath(const QString &key) const;
    // Path of the cached audio for key, or empty if absent. Marks it as
    // recently used.
    QString lookup(const QString &key) const;
    // Writes data atomically and evicts old entries (never the new one).
    // Returns the file path, or empty on failure (errorString set).
    QString store(const QString &key, const QByteArray &data, QString *errorString = nullptr);
    // Removes one entry (e.g. audio that failed to decode).
    void remove(const QString &key);
    // Deletes least-recently-used files until the limits hold. keepPath is
    // never deleted (e.g. the file currently playing). Returns files removed.
    int evict(const QString &keepPath = QString());
    // Deletes every cached file.
    void clear();

    // Number of cached files and their total size.
    int fileCount() const;
    qint64 totalBytes() const;

    static constexpr const char *FileSuffix = ".mp3";

private:
    QString m_dir;
    int m_maxFiles;
    qint64 m_maxBytes;
};

} // namespace sct
