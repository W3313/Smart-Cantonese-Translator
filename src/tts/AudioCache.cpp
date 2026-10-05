#include "tts/AudioCache.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <algorithm>
#include <cmath>

namespace sct {

namespace {

QFileInfoList cacheFiles(const QString &dir)
{
    if (dir.isEmpty())
        return {};
    const QDir d(dir);
    // QDir::Time sorts newest (most recently used) first.
    return d.entryInfoList({QStringLiteral("*") + QLatin1String(AudioCache::FileSuffix)},
                           QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Time);
}

} // namespace

AudioCache::AudioCache(const QString &directory, int maxFiles, qint64 maxBytes)
    : m_dir(directory)
    , m_maxFiles(maxFiles)
    , m_maxBytes(maxBytes)
{
}

void AudioCache::setDirectory(const QString &directory)
{
    m_dir = directory;
}

void AudioCache::setLimits(int maxFiles, qint64 maxBytes)
{
    m_maxFiles = maxFiles;
    m_maxBytes = maxBytes;
}

QString AudioCache::key(const QString &voice, double rate, const QString &text)
{
    if (std::isnan(rate))
        rate = 0.0;
    // Rate in hundredths, so 0.1 and 0.1000001 (or -0.0 and 0.0) share a key.
    const long hundredths = std::lround(std::clamp(rate, -1.0, 1.0) * 100.0);
    const QString material = voice.trimmed() + QLatin1Char('|') + QString::number(hundredths)
                             + QLatin1Char('|') + text;
    return QString::fromLatin1(
        QCryptographicHash::hash(material.toUtf8(), QCryptographicHash::Sha1).toHex());
}

QString AudioCache::filePath(const QString &key) const
{
    if (m_dir.isEmpty() || key.isEmpty())
        return QString();
    return QDir(m_dir).filePath(key + QLatin1String(FileSuffix));
}

QString AudioCache::lookup(const QString &key) const
{
    const QString path = filePath(key);
    if (path.isEmpty())
        return QString();
    const QFileInfo fi(path);
    if (!fi.isFile() || fi.size() <= 0)
        return QString();
    // Mark as recently used (best effort; may fail while the file is playing).
    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::ExistingOnly))
        f.setFileTime(QDateTime::currentDateTimeUtc(), QFileDevice::FileModificationTime);
    return path;
}

QString AudioCache::store(const QString &key, const QByteArray &data, QString *errorString)
{
    auto fail = [errorString](const QString &message) {
        if (errorString)
            *errorString = message;
        return QString();
    };
    const QString path = filePath(key);
    if (path.isEmpty())
        return fail(QStringLiteral("No audio cache directory configured"));
    if (data.isEmpty())
        return fail(QStringLiteral("No audio data"));
    if (!QDir().mkpath(m_dir))
        return fail(QStringLiteral("Cannot create %1").arg(QDir::toNativeSeparators(m_dir)));

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return fail(f.errorString());
    if (f.write(data) != data.size()) {
        const QString message = f.errorString();
        f.cancelWriting();
        return fail(message);
    }
    if (!f.commit()) {
        // e.g. on Windows the old copy is open in the media player - it holds
        // the same audio, so it is still usable.
        const QFileInfo existing(path);
        if (existing.isFile() && existing.size() > 0)
            return path;
        return fail(f.errorString());
    }
    evict(path);
    return path;
}

void AudioCache::remove(const QString &key)
{
    const QString path = filePath(key);
    if (!path.isEmpty())
        QFile::remove(path);
}

int AudioCache::evict(const QString &keepPath)
{
    const QFileInfoList files = cacheFiles(m_dir);
    const QString keep = keepPath.isEmpty() ? QString() : QFileInfo(keepPath).absoluteFilePath();

    int count = 0;
    qint64 bytes = 0;
    for (const QFileInfo &fi : files) {
        if (!keep.isEmpty() && fi.absoluteFilePath() == keep) {
            ++count;
            bytes += fi.size();
        }
    }

    int removed = 0;
    bool full = false;
    for (const QFileInfo &fi : files) {
        if (!keep.isEmpty() && fi.absoluteFilePath() == keep)
            continue;
        if (!full && count + 1 <= m_maxFiles && bytes + fi.size() <= m_maxBytes) {
            ++count;
            bytes += fi.size();
            continue;
        }
        full = true;  // everything older than this goes too (LRU order)
        if (QFile::remove(fi.absoluteFilePath()))
            ++removed;
    }
    return removed;
}

void AudioCache::clear()
{
    for (const QFileInfo &fi : cacheFiles(m_dir))
        QFile::remove(fi.absoluteFilePath());
}

int AudioCache::fileCount() const
{
    return int(cacheFiles(m_dir).size());
}

qint64 AudioCache::totalBytes() const
{
    qint64 total = 0;
    for (const QFileInfo &fi : cacheFiles(m_dir))
        total += fi.size();
    return total;
}

} // namespace sct
