#include <QCache>
#include <QFutureWatcher>
#include <QThreadPool>
#include <QTimer>
#include <QtConcurrent>
#include <qlogging.h>
#include <qnamespace.h>
#include "core/imagesession.h"
#include "config/appsettings.h"
#include "core/diskutils.h"
#include "core/thumbnailcache.h"
#include "core/thumbnailmanager.h"
#include "core/vksnapshotreconstructor.h"
#include "core/vulkancontext.h"

ImageSession::ImageSession(QObject *parent) : QObject(parent), m_monitor(new ImageMonitor(this)) {
    connect(m_monitor, &ImageMonitor::fileChanged, this, &ImageSession::onFileChanged);
    connect(&VulkanContext::instance(),
            &VulkanContext::deviceChanged,
            this,
            &ImageSession::onDeviceChanged);

    connect(&ThumbnailManager::instance(),
            &ThumbnailManager::thumbnailGenerated,
            this,
            &ImageSession::handleThumbnailGenerated);
}

ImageSession::~ImageSession() {
    close();
}

bool ImageSession::openImage(const QString& filePath) {
    ThumbnailCache::invalidate(SnapshotStore::imageKey(filePath), -1);

    m_filePath = filePath;
    m_diskImage = DiskUtils::loadImage(m_filePath);
    if (m_diskImage.isNull()) {
        emit statusMessage(QString("Failed to load: %1").arg(m_filePath));
        return false;
    }

    rebuildSnapshotList();
    m_monitor->watch(m_filePath);
    m_selectedIndex = static_cast<int>(m_snapshots.size());

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
    m_selectedIndex = 0;
    m_baseCache = {-1, QImage()};

    if (m_uiReconstructor) {
        m_uiReconstructor->cleanup();
        m_uiReconstructor.reset();
    }
}

std::optional<ReconstructionSequence> ImageSession::getReconstructionSequence() const {
    return getReconstructionSequence(m_selectedIndex);
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
    if (index < 0 || (index > static_cast<int>(m_snapshots.size())))
        return;
    m_selectedIndex = index;
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
                    emit statusMessage(
                        QString("Current iamge lalalal  lalalla already saved as snapshot %1.")
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
        int oldIndex = m_selectedIndex;

        rebuildSnapshotList();

        if (oldIndex == index) {
            // Viewing the deleted snapshot -> move to current disk image
            m_selectedIndex = static_cast<int>(m_snapshots.size());
        } else if (oldIndex > index && (isCurrentImage(oldIndex) ||
                                        oldIndex < static_cast<int>(m_snapshots.size()) + 1)) {
            // Viewed snapshot shifted left, or we were viewing the disk image
            // If oldIndex was S, and we deleted one, new S is S-1.
            // Either way, if it's above the deletion point, it shifts.
            m_selectedIndex = oldIndex - 1;
        } else {
            m_selectedIndex = oldIndex;
        }

        // Ensure we are still within valid bounds [0, m_snapshots.size()]
        m_selectedIndex = qBound(0, m_selectedIndex, static_cast<int>(m_snapshots.size()));

        emit imageChanged();
        emit statusMessage(QString("Snapshot %1 deleted.").arg(relativeVersion));
    } else {
        emit statusMessage("Failed to delete snapshot.");
    }
}

bool ImageSession::isCurrentImage(int index) const {
    return index == static_cast<int>(m_snapshots.size());
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

        ThumbnailCache::invalidate(SnapshotStore::imageKey(m_filePath), -1);

        if (!m_diskImage.isNull() && AppSettings::autosaveSnapshots()) {
            autosaveSnapshot(m_diskImage);
        }

        m_diskImage = newImage;
        rebuildSnapshotList();
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
                    bool wasViewingCurrent = isCurrentImage(m_selectedIndex);
                    rebuildSnapshotList();
                    if (wasViewingCurrent) {
                        m_selectedIndex = static_cast<int>(m_snapshots.size());
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
    QImage img = generateThumbnail(m_selectedIndex, size);
    return img;
}

QImage ImageSession::getPlaceholder(int size) {
    if (m_placeholderCache.contains(size)) {
        return m_placeholderCache[size];
    }

    QImage placeholder(size, size, QImage::Format_ARGB32);
    placeholder.fill(Qt::transparent);
    m_placeholderCache[size] = placeholder;
    return placeholder;
}

void ImageSession::handleThumbnailGenerated(const QString& path, int index, const QImage& img) {
    if (path == m_filePath) {
        emit thumbnailChanged(index);
    }
}

QImage ImageSession::generateThumbnail(int index, int size) {
    if (index < 0 || index > static_cast<int>(m_snapshots.size())) {
        qDebug() << "[ImageSession] generateThumbnail: index out of bounds";
        return QImage();
    }

    QImage result = ThumbnailManager::instance().getThumbnail(
        index, size, m_filePath, index == static_cast<int>(m_snapshots.size()), m_snapshots);

    if (result.isNull()) {
        return getPlaceholder(size);
    }

    return ThumbnailCache::formatThumbnail(result, size);
}

void ImageSession::onDeviceChanged() {
    if (m_uiReconstructor) {
        m_uiReconstructor->cleanup();
    }
    // UI reconstructor will be re-initialized by the renderer via setUIReconstructorHandles.
    emit imageChanged();
}

void ImageSession::setUIReconstructorHandles(const VulkanHandles& handles) {
    if (m_uiReconstructor) {
        if (m_uiReconstructor->getHandles().device == handles.device &&
            m_uiReconstructor->getHandles().physicalDevice == handles.physicalDevice) {
            return;
        }
    }
    m_uiReconstructor =
        std::make_shared<VkSnapshotReconstructor>(handles, &VulkanContext::instance());
}

std::shared_ptr<VkSnapshotReconstructor> ImageSession::utilityReconstructor() const {
    return VulkanContext::instance().getUtilityReconstructor();
}
