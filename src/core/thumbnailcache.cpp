#include <QDebug>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QStandardPaths>
#include <QThread>
#include "core/thumbnailcache.h"
#include "config/appsettings.h"

QThreadPool *ThumbnailThreadPool::instance() {
    static ThumbnailThreadPool pool;
    return &pool;
}

ThumbnailThreadPool::ThumbnailThreadPool() {
    setMaxThreadCount(qMax(1, qMin(4, QThread::idealThreadCount())));
}

QCache<QString, QImage> ThumbnailCache::s_thumbnailCache;
QMutex                  ThumbnailCache::s_mutex;

struct ThumbnailCacheInitializer {
    ThumbnailCacheInitializer() {
        ThumbnailCache::updateMaxCost(AppSettings::maxThumbnailCacheSizeMB());
    }
};
static ThumbnailCacheInitializer s_initializer;

QImage *ThumbnailCache::get(const QString& key) {
    QMutexLocker locker(&s_mutex);
    return s_thumbnailCache.object(key);
}

void ThumbnailCache::insert(const QString& key, QImage *image, int cost) {
    QMutexLocker locker(&s_mutex);
    s_thumbnailCache.insert(key, image, cost);
}

void ThumbnailCache::updateMaxCost(int sizeMB) {
    QMutexLocker locker(&s_mutex);
    int          maxBytes = sizeMB * 1024 * 1024;
    if (s_thumbnailCache.maxCost() != maxBytes) {
        s_thumbnailCache.setMaxCost(maxBytes);
    }
}

void ThumbnailCache::clear() {
    QMutexLocker locker(&s_mutex);
    s_thumbnailCache.clear();
}

void ThumbnailCache::remove(const QString& key) {
    QMutexLocker locker(&s_mutex);
    s_thumbnailCache.remove(key);
}

void ThumbnailCache::invalidate(const QString& imageKey, const QString& version) {
    ThumbnailCache::remove(keyFor(imageKey, version));

    QString baseThumbDir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/thumbnails";
    QString thumbPath = baseThumbDir + '/' + imageKey + '/' + version +
                        ThumbnailConstants::Extension;

    QFile::remove(thumbPath);
}

QString ThumbnailCache::keyFor(const QString& imageKey, const QString& version) {
    const int storageSize = ThumbnailConstants::StorageSize;
    return QString("%1:%2:%3x%4")
        .arg(imageKey)
        .arg(version)
        .arg(storageSize)
        .arg(storageSize);
}

QImage ThumbnailCache::loadThumbnail(const QString& imageKey, const QString& version, const QSize& size) {
    const QString cacheKey = keyFor(imageKey, version);

    QImage cachedImg;
    {
        QMutexLocker locker(&s_mutex);
        if (QImage *cached = s_thumbnailCache.object(cacheKey)) {
            cachedImg = *cached;
        }
    }

    if (!cachedImg.isNull()) {
        return cachedImg.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QString baseThumbDir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/thumbnails";
    QString cacheDir = baseThumbDir + '/' + imageKey;

    QDir().mkpath(cacheDir);

    QString thumbPath =
        cacheDir + '/' + version + ThumbnailConstants::Extension;

    if (QFile::exists(thumbPath)) {
        QImage thumb(thumbPath);
        if (!thumb.isNull()) {
            ThumbnailCache::insert(cacheKey, new QImage(thumb), thumb.sizeInBytes());
            return thumb.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    return {};
}
