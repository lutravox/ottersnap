#include <QDebug>
#include <QFutureWatcher>
#include <QPainter>
#include <QtConcurrent>
#include "core/thumbnailmanager.h"
#include "core/diskutils.h"
#include "core/thumbnailcache.h"

ThumbnailManager& ThumbnailManager::instance() {
    static ThumbnailManager inst;
    return inst;
}

ThumbnailManager::ThumbnailManager(QObject *parent) : QObject(parent) {
}

QImage ThumbnailManager::getThumbnail(int                           index,
                                      int                           size,
                                      const QString&                filePath,
                                      bool                          isCurrent,
                                      const QVector<ImageSnapshot>& snapshots,
                                      const QImage&                 currentImage) {
    if (index < 0 || index > static_cast<int>(snapshots.size()))
        return QImage();

    QString key = SnapshotManager::cacheKeyForPath(filePath);
    QUuid uuid = (isCurrent || index == static_cast<int>(snapshots.size())) ? QUuid()
                                                                            : snapshots[index].uuid;
    QString idStr = getIdentityString(uuid);

    QImage thumb = ThumbnailCache::loadThumbnail(key, idStr, QSize(size, size));
    if (!thumb.isNull())
        return thumb;

    QString requestKey = getRequestKey(filePath, uuid);
    if (m_activeRequests.contains(requestKey)) {
        return QImage();
    }

    enqueueRequest({index, filePath, uuid, currentImage});
    return QImage();
}

void ThumbnailManager::enqueueRequest(const ThumbnailRequest& request) {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(
            this, [this, request]() { this->enqueueRequest(request); }, Qt::QueuedConnection);
        return;
    }

    QString requestKey = getRequestKey(request.filePath, request.uuid);
    if (m_activeRequests.contains(requestKey)) {
        return;
    }
    m_activeRequests.insert(requestKey);

    if (request.uuid.isNull()) {
        static_cast<QList<ThumbnailRequest>&>(m_queue).prepend(request);
    } else {
        m_queue.enqueue(request);
    }

    if (!m_isProcessing) {
        processNext();
    }
}

void ThumbnailManager::processNext() {
    if (m_queue.isEmpty()) {
        m_isProcessing = false;
        return;
    }

    m_isProcessing = true;
    ThumbnailRequest request = m_queue.dequeue();

    auto watcher = new QFutureWatcher<std::optional<QImage>>(this);

    connect(watcher, &QFutureWatcher<std::optional<QImage>>::finished, [this, watcher, request]() {
        onReconstructionFinished(watcher, request);
    });

    std::function<std::optional<QImage>()> worker;
    if (request.uuid.isNull()) {
        worker = [path = request.filePath, img = request.currentImage]() -> std::optional<QImage> {
            return reconstructDiskImage(path, img);
        };
    } else {
        QUuid uuid = request.uuid;
        worker = [path = request.filePath, uuid]() { return reconstructThumbnail(path, uuid); };
    }

    watcher->setFuture(QtConcurrent::run(ThumbnailThreadPool::instance(), worker));
}

void ThumbnailManager::onReconstructionFinished(QFutureWatcher<std::optional<QImage>> *watcher,
                                                const ThumbnailRequest&                request) {
    auto res = watcher->result();
    if (res && !res->isNull()) {
        const QImage& img = *res;
        saveThumbnail(request.filePath, request.uuid, img);

        // Cache in memory
        QString idStr = getIdentityString(request.uuid);
        QString cacheKey = QString("%1:%2:%3x%4")
                               .arg(SnapshotManager::cacheKeyForPath(request.filePath))
                               .arg(idStr)
                               .arg(ThumbnailConstants::StorageSize)
                               .arg(ThumbnailConstants::StorageSize);

        ThumbnailCache::insert(cacheKey, new QImage(img), img.sizeInBytes());

        emit thumbnailGenerated(request.filePath, request.uuid, img);
    }

    QString requestKey = getRequestKey(request.filePath, request.uuid);
    m_activeRequests.remove(requestKey);
    watcher->deleteLater();
    processNext();
}

void ThumbnailManager::saveThumbnail(const QString& filePath,
                                     const QUuid&   uuid,
                                     const QImage&  image) {
    QString key = SnapshotManager::cacheKeyForPath(filePath);
    QString sd = SnapshotManager::thumbnailDir() + '/' + key;
    if (!DiskUtils::ensureDir(sd))
        return;

    QString idStr = getIdentityString(uuid);
    QString path = sd + '/' + idStr + ThumbnailConstants::Extension;
    image.save(path, ThumbnailConstants::Format.toUtf8().constData());
}

QString ThumbnailManager::getIdentityString(const QUuid& uuid) const {
    return uuid.isNull() ? ThumbnailConstants::CurrentVersion : uuid.toString(QUuid::WithoutBraces);
}

QString ThumbnailManager::getRequestKey(const QString& filePath, const QUuid& uuid) const {
    return filePath + ":" + getIdentityString(uuid);
}

QImage ThumbnailManager::formatThumbnail(const QImage& image, int size) {
    if (image.isNull()) {
        qWarning() << "[ThumbnailManager] Null image provided for thumbnail";
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

std::optional<QImage> ThumbnailManager::reconstructDiskImage(const QString& path,
                                                             const QImage&  currentImage) {
    QImage img = currentImage;
    if (img.isNull()) {
        if (!QFile::exists(path)) {
            return std::nullopt;
        }
        img = DiskUtils::loadImage(path);
    }

    if (img.isNull())
        return std::nullopt;

    float aspect = float(img.width()) / float(img.height());
    int   storageSize = 256;
    QSize targetSize = (aspect > 1.0f) ? QSize(storageSize, qRound(storageSize / aspect))
                                       : QSize(qRound(storageSize * aspect), storageSize);

    QImage result = SnapshotManager::resizeImage(img, targetSize);
    if (result.isNull())
        return std::nullopt;
    return result;
}

std::optional<QImage> ThumbnailManager::reconstructThumbnail(const QString& path,
                                                             const QUuid&   uuid) {
    auto snapList = SnapshotManager::loadSnapshots(path);

    ImageSnapshot target;
    bool          found = false;
    for (const auto& s : snapList) {
        if (s.uuid == uuid) {
            target = s;
            found = true;
            break;
        }
    }
    if (!found)
        return std::nullopt;

    // Find the closest preceding base image to determine dimensions
    int baseIdx = -1;
    for (int i = 0; i < static_cast<int>(snapList.size()); ++i) {
        if (snapList[i].isBase) {
            baseIdx = i;
        }
        if (snapList[i].uuid == uuid) {
            break;
        }
    }

    if (baseIdx == -1)
        return std::nullopt;

    auto optBase = SnapshotManager::loadBaseImage(path, snapList[baseIdx]);
    if (!optBase)
        return std::nullopt;

    float aspect = float(optBase->image.width()) / float(optBase->image.height());
    int   storageSize = 256;
    QSize targetSize = (aspect > 1.0f) ? QSize(storageSize, qRound(storageSize / aspect))
                                       : QSize(qRound(storageSize * aspect), storageSize);

    QImage result = SnapshotManager::reconstructSnapshot(path, uuid, targetSize);
    if (result.isNull())
        return std::nullopt;

    return result;
}
