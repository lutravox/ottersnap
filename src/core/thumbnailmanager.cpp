#include <QDebug>
#include <QFutureWatcher>
#include <QtConcurrent>
#include "core/thumbnailmanager.h"
#include "core/diskutils.h"
#include "core/thumbnailcache.h"
#include "core/vulkancontext.h"

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
                                      const QVector<ImageSnapshot>& snapshots) {
    if (index < 0 || index > static_cast<int>(snapshots.size()))
        return QImage();

    QString key = SnapshotStore::imageKey(filePath);
    int     version = isCurrent ? -1 : snapshots[index].snapshotIndex;

    QImage thumb = ThumbnailCache::loadThumbnail(key, version, QSize(size, size));
    if (!thumb.isNull())
        return thumb;

    QString requestKey = filePath + ":" + QString::number(index);
    if (m_activeRequests.contains(requestKey)) {
        return QImage();
    }

    enqueueRequest({index, filePath, version});
    return QImage();
}

void ThumbnailManager::enqueueRequest(const ThumbnailRequest& request) {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(
            this, [this, request]() { this->enqueueRequest(request); }, Qt::QueuedConnection);
        return;
    }

    QString requestKey = request.filePath + ":" + QString::number(request.index);
    if (m_activeRequests.contains(requestKey)) {
        return;
    }
    m_activeRequests.insert(requestKey);

    // Prioritize current image thumbnails (snapshotIndex == -1)
    if (request.snapshotIndex == -1) {
        // We use QList::prepend via QQueue's inheritance to put current images at the front
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
    if (request.snapshotIndex == -1) {
        worker = [path = request.filePath]() { return reconstructDiskImage(path); };
    } else {
        int snapshotIdx = request.snapshotIndex;
        worker = [path = request.filePath, snapshotIdx]() {
            return reconstructSnapshot(path, snapshotIdx);
        };
    }

    watcher->setFuture(QtConcurrent::run(ThumbnailThreadPool::instance(), worker));
}

void ThumbnailManager::onReconstructionFinished(QFutureWatcher<std::optional<QImage>> *watcher,
                                                const ThumbnailRequest&                request) {
    auto res = watcher->result();
    if (res && !res->isNull()) {
        const QImage& img = *res;
        saveThumbnail(request.filePath, request.snapshotIndex, img);

        // Always cache in memory, regardless of whether it's a snapshot or current
        QString cacheKey = (request.snapshotIndex == -1)
                               ? QString("%1:current:%2x%3")
                                     .arg(SnapshotStore::imageKey(request.filePath))
                                     .arg(ThumbnailConstants::StorageSize)
                                     .arg(ThumbnailConstants::StorageSize)
                               : QString("%1:%2:%3x%4")
                                     .arg(SnapshotStore::imageKey(request.filePath))
                                     .arg(request.snapshotIndex)
                                     .arg(ThumbnailConstants::StorageSize)
                                     .arg(ThumbnailConstants::StorageSize);

        ThumbnailCache::insert(cacheKey, new QImage(img), img.sizeInBytes());

        emit thumbnailGenerated(request.filePath, request.index, img);
    }

    QString requestKey = request.filePath + ":" + QString::number(request.index);
    m_activeRequests.remove(requestKey);
    watcher->deleteLater();
    processNext();
}

void ThumbnailManager::saveThumbnail(const QString& filePath,
                                     int            snapshotIndex,
                                     const QImage&  image) {
    QString key = SnapshotStore::imageKey(filePath);
    QString sd = SnapshotStore::thumbnailDir() + '/' + key;
    if (!DiskUtils::ensureDir(sd))
        return;

    QString path = sd + '/' + QString::asprintf("v%04d.webp", snapshotIndex);
    image.save(path, "WEBP");
}

std::optional<QImage> ThumbnailManager::reconstructDiskImage(const QString& path) {
    QImage img = DiskUtils::loadImage(path);
    if (img.isNull())
        return std::nullopt;

    auto reconstructor = VulkanContext::instance().getUtilityReconstructor();
    if (!reconstructor)
        return std::nullopt;

    ReconstructionSequence diskSeq;
    diskSeq.base = img;
    diskSeq.baseChecksum = "disk_image";

    float aspect = float(img.width()) / float(img.height());
    int   storageSize = 256;
    QSize targetSize = (aspect > 1.0f) ? QSize(storageSize, qRound(storageSize / aspect))
                                       : QSize(qRound(storageSize * aspect), storageSize);

    return reconstructor->reconstructToImage(diskSeq, targetSize, reconstructor.get());
}

std::optional<QImage> ThumbnailManager::reconstructSnapshot(const QString& path, int snapshotIdx) {
    auto snapList = SnapshotStore::loadSnapshots(path);
    int  localIdx = -1;
    for (int i = 0; i < static_cast<int>(snapList.size()); ++i) {
        if (snapList[i].snapshotIndex == snapshotIdx) {
            localIdx = i;
            break;
        }
    }
    if (localIdx == -1)
        return std::nullopt;

    int baseIdx = -1;
    for (int i = localIdx; i >= 0; --i) {
        if (snapList[i].isBase) {
            baseIdx = i;
            break;
        }
    }
    if (baseIdx == -1)
        return std::nullopt;

    ReconstructionSequence seq;
    seq.baseIdx = baseIdx;
    auto optBase = SnapshotStore::loadBaseImage(path, snapList[baseIdx].snapshotIndex);
    if (!optBase)
        return std::nullopt;
    seq.base = std::move(optBase->image);
    seq.baseChecksum = optBase->checksum;

    for (int i = baseIdx + 1; i <= localIdx; ++i) {
        auto optDelta = SnapshotStore::loadDelta(path, snapList[i].snapshotIndex);
        if (!optDelta)
            return std::nullopt;
        seq.deltas.append(std::move(*optDelta));
    }

    auto reconstructor = VulkanContext::instance().getUtilityReconstructor();
    if (!reconstructor)
        return std::nullopt;

    float aspect = float(seq.base.width()) / float(seq.base.height());
    int   storageSize = 256;
    QSize targetSize = (aspect > 1.0f) ? QSize(storageSize, qRound(storageSize / aspect))
                                       : QSize(qRound(storageSize * aspect), storageSize);

    return reconstructor->reconstructToImage(seq, targetSize, reconstructor.get());
}
