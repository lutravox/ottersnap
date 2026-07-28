#include <QCache>
#include <QFutureWatcher>
#include <QTimer>
#include <QtConcurrent>
#include <qlogging.h>
#include "core/imagesession.h"
#include "config/appsettings.h"
#include "core/diskutils.h"
#include "core/imagecache.h"
#include "core/vksnapshotreconstructor.h"
#include "core/vulkancontext.h"

ImageSession::ImageSession(QObject *parent) : QObject(parent), m_monitor(new ImageMonitor(this)) {
    connect(m_monitor, &ImageMonitor::fileChanged, this, &ImageSession::onFileChanged);
    connect(&VulkanContext::instance(),
            &VulkanContext::deviceChanged,
            this,
            &ImageSession::onDeviceChanged);
}

ImageSession::~ImageSession() {
    close();
}

bool ImageSession::openImage(const QString& filePath) {
    m_filePath = filePath;
    m_diskImage = DiskUtils::loadImage(m_filePath);
    if (m_diskImage.isNull()) {
        emit statusMessage(QString("Failed to load: %1").arg(m_filePath));
        return false;
    }

    createReconstructor();

    rebuildSnapshotList();
    m_monitor->watch(m_filePath);
    m_currentIndex = static_cast<int>(m_snapshots.size());

    // Initialize view state with image dimensions
    m_viewState.resetState(m_diskImage.width(), m_diskImage.height());

    emit imageChanged();
    return true;
}

void ImageSession::close() {
    if (m_filePath != "") {
        qDebug() << "[ImageSession] Closing session for:" << m_filePath;
    }

    m_monitor->stop();
    m_filePath.clear();
    m_diskImage = QImage();
    m_snapshots.clear();
    m_labels.clear();
    m_currentIndex = 0;
    m_baseCache = {-1, QImage()};

    if (m_reconstructor) {
        m_reconstructor->cleanup();
        m_reconstructor.reset();
    }
}

std::optional<ReconstructionSequence> ImageSession::getReconstructionSequence() const {
    return getReconstructionSequence(m_currentIndex);
}

std::optional<ReconstructionSequence> ImageSession::getReconstructionSequence(int index) const {
    if (index < 0 || index >= static_cast<int>(m_snapshots.size())) {
        qWarning() << "[ImageSession] Index out of bounds for reconstruction sequence";
        return std::nullopt;
    }

    const ImageSnapshot& target = m_snapshots[index];
    int                  baseIdx = -1;
    for (int i = index; i >= 0; --i) {
        if (m_snapshots[i].isBase) {
            baseIdx = i;
            break;
        }
    }

    if (baseIdx == -1) {
        qWarning() << "[ImageSession] No base image found for reconstruction sequence";
        return std::nullopt;
    }

    ReconstructionSequence seq;
    seq.baseIdx = baseIdx;

    int baseSnapshotIdx = m_snapshots[baseIdx].snapshotIndex;
    if (m_baseCache.index == baseSnapshotIdx && !m_baseCache.image.isNull()) {
        seq.base = m_baseCache.image;
        seq.baseChecksum = m_snapshots[baseIdx].checksum;
    } else {
        auto optBase = SnapshotStore::loadBaseImage(m_filePath, baseSnapshotIdx);
        if (!optBase)
            return std::nullopt;

        seq.base = std::move(optBase->image);
        seq.baseChecksum = optBase->checksum;
        m_baseCache = {baseSnapshotIdx, seq.base};
    }

    for (int i = baseIdx + 1; i <= index; ++i) {
        auto optDelta = SnapshotStore::loadDelta(m_filePath, m_snapshots[i].snapshotIndex);
        if (!optDelta)
            return std::nullopt;
        seq.deltas.append(std::move(*optDelta));
    }

    return seq;
}

void ImageSession::selectSnapshot(int index) {
    if (index < 0 || index > static_cast<int>(m_snapshots.size()))
        return;
    m_currentIndex = index;
    m_thumbnailCache = {-1, QImage()};
    emit imageChanged();
}

void ImageSession::saveSnapshot() {
    if (m_diskImage.isNull())
        return;

    QString path = m_filePath;
    QImage  img = m_diskImage;

    auto watcher = new QFutureWatcher<std::optional<SnapshotStore::SaveResult>>(this);
    connect(watcher,
            &QFutureWatcher<std::optional<SnapshotStore::SaveResult>>::finished,
            [this, watcher]() {
                auto res = watcher->result();
                if (res && res->status == SnapshotStore::SaveStatus::Created) {
                    rebuildSnapshotList();
                    emit statusMessage("Snapshot saved.");
                } else if (res && res->status == SnapshotStore::SaveStatus::Existing) {
                    int  pos = getRelativeVersion(res->snapshotIndex);
                    emit statusMessage(QString("Current already saved as snapshot %1.")
                                           .arg(pos != -1 ? QString::number(pos)
                                                          : QString::number(res->snapshotIndex)));
                } else {
                    emit statusMessage("Save failed.");
                }
                watcher->deleteLater();
            });
    watcher->setFuture(
        QtConcurrent::run([path, img]() { return SnapshotStore::saveSnapshot(path, img); }));
}

void ImageSession::deleteSnapshot(int index) {
    if (index < 0 || index >= static_cast<int>(m_snapshots.size()))
        return;

    int snapshotId = m_snapshots[index].snapshotIndex;
    int relativeVersion = index + 1;

    if (SnapshotStore::deleteSnapshot(m_filePath, snapshotId)) {
        // Store current index to adjust it after the list is rebuilt
        int oldIndex = m_currentIndex;

        rebuildSnapshotList();

        if (oldIndex == index) {
            // Viewing the deleted snapshot -> move to current disk image
            m_currentIndex = static_cast<int>(m_snapshots.size());
        } else if (oldIndex > index && oldIndex < static_cast<int>(m_snapshots.size()) + 1) {
            // Viewed snapshot shifted left, or we were viewing the disk image
            // If oldIndex was S, and we deleted one, new S is S-1.
            // Either way, if it's above the deletion point, it shifts.
            m_currentIndex = oldIndex - 1;
        } else {
            m_currentIndex = oldIndex;
        }

        // Ensure we are still within valid bounds [0, m_snapshots.size()]
        m_currentIndex = qBound(0, m_currentIndex, static_cast<int>(m_snapshots.size()));

        emit imageChanged();
        emit statusMessage(QString("Snapshot %1 deleted.").arg(relativeVersion));
    } else {
        emit statusMessage("Failed to delete snapshot.");
    }
}

void ImageSession::onFileChanged() {
    reloadImage();
}

void ImageSession::reloadImage() {
    qDebug() << "[ImageSession] Reloading image:" << m_filePath;

    QString path = m_filePath;
    auto    watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, [this, watcher]() {
        QImage newImage = watcher->result();
        watcher->deleteLater();

        if (newImage.isNull() || newImage == m_diskImage)
            return;

        if (!m_diskImage.isNull() && AppSettings::autosaveSnapshots()) {
            autosaveSnapshot(m_diskImage);
        }

        m_diskImage = newImage;
        rebuildSnapshotList();
        m_thumbnailCache = {-1, QImage()};
        emit imageChanged();
    });

    watcher->setFuture(QtConcurrent::run([path]() { return DiskUtils::loadImage(path); }));
}

void ImageSession::autosaveSnapshot(const QImage& img) {
    QString path = m_filePath;
    QImage  image = img;

    auto watcher = new QFutureWatcher<std::optional<SnapshotStore::SaveResult>>(this);
    connect(watcher,
            &QFutureWatcher<std::optional<SnapshotStore::SaveResult>>::finished,
            [this, watcher]() {
                auto res = watcher->result();
                if (res && res->status == SnapshotStore::SaveStatus::Created) {
                    bool wasViewingCurrent =
                        (m_currentIndex == static_cast<int>(m_snapshots.size()));
                    rebuildSnapshotList();
                    if (wasViewingCurrent) {
                        m_currentIndex = static_cast<int>(m_snapshots.size());
                        emit imageChanged();
                    }
                    emit statusMessage("Snapshot autosaved.");
                }
                watcher->deleteLater();
            });
    watcher->setFuture(
        QtConcurrent::run([path, image]() { return SnapshotStore::saveSnapshot(path, image); }));
}

int ImageSession::getRelativeVersion(int snapshotIndex) const {
    for (int i = 0; i < static_cast<int>(m_snapshots.size()); ++i) {
        if (m_snapshots[i].snapshotIndex == snapshotIndex) {
            return i + 1;
        }
    }
    return -1;
}

void ImageSession::setGrayscale(bool enabled) {
    m_effects.grayscale = enabled;
    emit effectsChanged();
}

void ImageSession::setMirror(bool enabled) {
    m_effects.mirror = enabled;
    emit effectsChanged();
}

void ImageSession::rebuildSnapshotList() {
    m_snapshots = SnapshotStore::loadSnapshots(m_filePath);
    m_labels.clear();
    for (const auto& v : m_snapshots) {
        m_labels.append(v.timestamp.toString("MMMM d, yyyy h:mm:ss AP"));
    }
    m_labels.append("Current");
    emit snapshotsChanged();
}

std::pair<QVector<QImage>, QVector<QString>> ImageSession::snapshotTimelineThumbnails(int size) {
    QVector<QImage> thumbs;
    thumbs.reserve(m_snapshots.size() + 1);

    for (int i = 0; i < static_cast<int>(m_snapshots.size()); ++i) {
        thumbs.append(generateThumbnail(i, size));
    }

    thumbs.append(generateThumbnail(static_cast<int>(m_snapshots.size()), size));

    return {thumbs, m_labels};
}

QImage ImageSession::thumbnail(int size) {
    if (m_thumbnailCache.size == size && !m_thumbnailCache.image.isNull()) {
        qDebug() << "[ImageSession] Using cached thumbnail for current image";
        return m_thumbnailCache.image;
    }

    QImage thumb = generateThumbnail(m_currentIndex, size);
    m_thumbnailCache = {size, thumb};
    return thumb;
}

QImage ImageSession::generateThumbnail(int index, int size) {
    if (index < 0 || index > static_cast<int>(m_snapshots.size())) {
        qDebug() << "[ImageSession] generateThumbnail: index out of bounds";
        return QImage();
    }

    if (index == static_cast<int>(m_snapshots.size())) {
        // Current disk image
        if (m_diskImage.isNull()) {
            if (m_filePath.isEmpty()) {
                qDebug() << "[ImageSession] generateThumbnail: filepath empty";
                return QImage();
            }
            qDebug() << "[ImageSession] Using placeholder thumbnail for current image"
                     << m_filePath;
            return QImage(size, size, QImage::Format_ARGB32); // Placeholder
        }

        auto *reconstructor = VulkanContext::instance().getUtilityReconstructor();
        if (reconstructor) {
            ReconstructionSequence diskSeq;
            diskSeq.base = m_diskImage;
            diskSeq.baseChecksum = "disk_image";

            float aspect = float(m_diskImage.width()) / float(m_diskImage.height());
            QSize targetSize = (aspect > 1.0f) ? QSize(size, qRound(size / aspect))
                                               : QSize(qRound(size * aspect), size);

            QImage thumb = reconstructor->reconstructToImage(diskSeq, targetSize, reconstructor);
            if (!thumb.isNull())
                return thumb;
        }
        return ImageCache::formatThumbnail(m_diskImage, size);
    } else {
        // Snapshot
        const auto& v = m_snapshots[index];
        QString     key = SnapshotStore::imageKey(m_filePath);
        QImage      thumb = ImageCache::loadThumbnail(key, v.snapshotIndex, QSize(size, size));

        if (!thumb.isNull())
            return thumb;

        auto  seq = getReconstructionSequence(index);
        auto *reconstructor = VulkanContext::instance().getUtilityReconstructor();
        if (seq && reconstructor) {
            float aspect = float(seq->base.width()) / float(seq->base.height());
            QSize targetSize = (aspect > 1.0f) ? QSize(size, qRound(size / aspect))
                                               : QSize(qRound(size * aspect), size);

            thumb = reconstructor->reconstructToImage(*seq, targetSize, reconstructor);
            if (!thumb.isNull()) {
                SnapshotStore::saveThumbnail(m_filePath, v.snapshotIndex, thumb);
                return thumb;
            }
        }

        qDebug() << "[ImageSession] Using placeholder thumbnail for snapshot";
        QImage placeholder(size, size, QImage::Format_ARGB32);
        placeholder.fill(Qt::transparent);
        return placeholder;
    }
}

void ImageSession::onDeviceChanged() {
    if (m_reconstructor) {
        m_reconstructor->cleanup();
    }
    createReconstructor();
    emit imageChanged();
}

void ImageSession::createReconstructor() {
    auto&         ctx = VulkanContext::instance();
    VulkanHandles handles;
    handles.physicalDevice = ctx.getPhysicalDevice();
    handles.device = ctx.getDevice();
    handles.queue = ctx.getQueue();
    handles.queueFamilyIndex = ctx.getQueueFamilyIndex();
    handles.deviceFunctions = ctx.getDeviceFunctions();
    handles.commandPool = ctx.getCommandPool();
    m_reconstructor = std::make_shared<VkSnapshotReconstructor>(handles);
}
