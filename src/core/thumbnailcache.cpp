#include <QDebug>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QStandardPaths>
#include "core/thumbnailcache.h"
#include "config/appsettings.h"

QThreadPool *ThumbnailThreadPool::instance() {
    static ThumbnailThreadPool pool;
    return &pool;
}

ThumbnailThreadPool::ThumbnailThreadPool() {
    setMaxThreadCount(1);
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

void ThumbnailCache::invalidate(const QString& imageKey, int version) {
    const int storageSize = ThumbnailConstants::StorageSize;

    QString cacheKey =
        (version == -1)
            ? QString("%1:current:%2x%3").arg(imageKey).arg(storageSize).arg(storageSize)
            : QString("%1:%2:%3x%4").arg(imageKey).arg(version).arg(storageSize).arg(storageSize);

    ThumbnailCache::remove(cacheKey);

    QString baseThumbDir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/thumbnails";
    QString thumbPath = baseThumbDir + '/' + imageKey + '/' + QString::asprintf("v%04d", version) +
                        ThumbnailConstants::Extension;

    QFile::remove(thumbPath);
}

QImage ThumbnailCache::loadThumbnail(const QString& imageKey, int version, const QSize& size) {
    const int storageSize = ThumbnailConstants::StorageSize;

    // -1 = Current Image
    QString cacheKey =
        (version == -1)
            ? QString("%1:current:%2x%3").arg(imageKey).arg(storageSize).arg(storageSize)
            : QString("%1:%2:%3x%4").arg(imageKey).arg(version).arg(storageSize).arg(storageSize);

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
        cacheDir + '/' + QString::asprintf("v%04d", version) + ThumbnailConstants::Extension;

    if (QFile::exists(thumbPath)) {
        QImage thumb(thumbPath);
        if (!thumb.isNull()) {
            ThumbnailCache::insert(cacheKey, new QImage(thumb), thumb.sizeInBytes());
            return thumb.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    return {};
}

QImage ThumbnailCache::formatThumbnail(const QImage& image, int size) {
    if (image.isNull()) {
        qWarning() << "[ThumbnailCache] Null image provided for thumbnail";
        return QImage(size, size, QImage::Format_ARGB32);
    }

    QImage scaled = image.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QImage canvas(size, size, QImage::Format_ARGB32);
    canvas.fill(Qt::transparent);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QPoint dest((canvas.width() - scaled.width()) / 2, (canvas.height() - scaled.height()) / 2);
    painter.drawImage(dest, scaled);
    return canvas;
}
