#include <QDebug>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QStandardPaths>
#include "core/imagecache.h"
#include "config/appsettings.h"

QCache<QString, QImage> ImageCache::s_imageCache;

QImage *ImageCache::get(const QString& key) {
    return s_imageCache.object(key);
}

void ImageCache::insert(const QString& key, QImage *image, int cost) {
    s_imageCache.insert(key, image, cost);
}

void ImageCache::updateMaxCost(int sizeMB) {
    int maxBytes = sizeMB * 1024 * 1024;
    if (s_imageCache.maxCost() != maxBytes) {
        s_imageCache.setMaxCost(maxBytes);
    }
}

void ImageCache::clear() {
    s_imageCache.clear();
}

void ImageCache::remove(const QString& key) {
    s_imageCache.remove(key);
}

QImage ImageCache::loadThumbnail(const QString&          imageKey,
                                 int                     version,
                                 const QSize&            size,
                                 std::function<QImage()> loadFullImage) {
    QString baseThumbDir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/thumbnails";
    QString cacheDir = baseThumbDir + '/' + imageKey;

    QDir().mkpath(cacheDir);

    QString thumbPath = cacheDir + '/' + QString::asprintf("v%04d.webp", version);

    if (QFile::exists(thumbPath)) {
        return QImage(thumbPath);
    }

    QImage full = loadFullImage();
    if (full.isNull()) {
        return {};
    }

    QImage thumb = full.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (!thumb.save(thumbPath, "WEBP")) {
        qWarning() << "[ImageCache] Failed to save thumbnail in cache:" << thumbPath;
    }

    return thumb;
}

QImage ImageCache::formatThumbnail(const QImage& image, int size) {
    if (image.isNull())
        return QImage(size, size, QImage::Format_ARGB32);

    QImage scaled = image.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QImage canvas(size, size, QImage::Format_ARGB32);
    canvas.fill(Qt::transparent);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QPoint dest((canvas.width() - scaled.width()) / 2, (canvas.height() - scaled.height()) / 2);
    painter.drawImage(dest, scaled);
    return canvas;
}
