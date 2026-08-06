#include <QDateTime>
#include <QFileInfo>
#include <QThreadPool>
#include <QTimer>
#include <QtConcurrent>
#include <qlogging.h>
#include <qnamespace.h>
#include <qobject.h>
#include <QApplication>
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
    ThumbnailCache::invalidate(SnapshotManager::imageKey(filePath), -1);

    m_filePath = filePath;
    m_lastModified = QFileInfo(m_filePath).lastModified();
    if (m_isSnapshotOnly) {
        m_diskImage = QImage();
    } else {
        m_diskImage = DiskUtils::loadImage(m_filePath);
        if (m_diskImage.isNull()) {
            emit statusMessage(QString("Failed to load: %1").arg(m_filePath));
            return false;
        }
    }

    rebuildSnapshotList();
    m_monitor->watch(m_filePath);

    if (m_isSnapshotOnly && m_snapshots.isEmpty()) {
        return false;
    }

    m_selectedIndex = m_isSnapshotOnly ? 0 : static_cast<int>(m_snapshots.size());

    // Initialize view state with image dimensions
    QSize dims = m_diskImage.size();
    if (m_isSnapshotOnly && !m_snapshots.isEmpty()) {
        auto optBase = SnapshotManager::loadBaseImage(m_filePath, m_snapshots[0].snapshotIndex);
        if (optBase) {
            dims = optBase->image.size();
        }
    }
    m_viewState.resetState(dims.width(), dims.height());

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
    m_secondarySnapshotIndex = SecondaryNone;
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
        return std::nullopt;
    }

    int baseIdx = -1;
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
    } else if (baseSnapshotIdx == -1) {
        seq.base = m_diskImage;
        seq.baseChecksum = m_snapshots[baseIdx].checksum;
        m_baseCache = {baseSnapshotIdx, seq.base};
    } else {
        auto optBase = SnapshotManager::loadBaseImage(m_filePath, baseSnapshotIdx);
        if (!optBase)
            return std::nullopt;

        seq.base = std::move(optBase->image);
        seq.baseChecksum = optBase->checksum;
        m_baseCache = {baseSnapshotIdx, seq.base};
    }

    QString imageKey = SnapshotManager::imageKey(m_filePath);
    for (int i = baseIdx + 1; i <= index; ++i) {
        auto optDelta = SnapshotManager::loadDelta(m_filePath, m_snapshots[i].snapshotIndex);
        if (!optDelta)
            return std::nullopt;
        seq.deltas.append({imageKey + ":" + m_snapshots[i].fileName, std::move(*optDelta)});
    }

    return seq;
}

int ImageSession::maxValidIndex() const {
    return m_isSnapshotOnly ? std::max(0, static_cast<int>(m_snapshots.size()) - 1)
                            : static_cast<int>(m_snapshots.size());
}

void ImageSession::selectSnapshot(int index) {
    if (index < 0 || index > maxValidIndex())
        return;
    m_selectedIndex = index;
    emit imageChanged();
}

void ImageSession::saveSnapshot() {
    if (m_diskImage.isNull())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    performSave(m_diskImage, false);
}

void ImageSession::deleteSnapshot(int index) {
    if (index < 0 || index >= static_cast<int>(m_snapshots.size()))
        return;

    int snapshotId = m_snapshots[index].snapshotIndex;
    int relativeVersion = index + 1;

    if (SnapshotManager::deleteSnapshot(m_filePath, snapshotId)) {
        // Store current index to adjust it after the list is rebuilt
        int oldIndex = m_selectedIndex;

        if (m_secondarySnapshotIndex == snapshotId) {
            m_secondarySnapshotIndex = SecondaryNone;
        }

        rebuildSnapshotList();

        if (oldIndex == index) {
            // Viewing the deleted snapshot -> move to current disk image (or last available
            // snapshot if snapshot-only)
            m_selectedIndex = maxValidIndex();
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
        emit statusMessage(QString("Snapshot %1 deleted successfully.").arg(relativeVersion));
    } else {
        emit statusMessage("Failed to delete snapshot.");
    }
}

bool ImageSession::isCurrentImage(int index) const {
    if (m_isSnapshotOnly) {
        return false;
    }
    return index == static_cast<int>(m_snapshots.size());
}

void ImageSession::onFileChanged() {
    if (!AppSettings::autoreloadImages())
        return;

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

        ThumbnailCache::invalidate(SnapshotManager::imageKey(m_filePath), -1);

        if (!m_diskImage.isNull() && AppSettings::autosaveSnapshots()) {
            autosaveSnapshot(m_diskImage);
        }

        m_diskImage = newImage;
        m_lastModified = QFileInfo(m_filePath).lastModified();
        emit statusMessage(tr("Current image reloaded."));
        emit imageChanged();
    });

    watcher->setFuture(QtConcurrent::run([path]() { return DiskUtils::loadImage(path); }));
}

void ImageSession::autosaveSnapshot(const QImage& img) {
    performSave(img, true);
}

void ImageSession::performSave(const QImage& img, bool isAutosave) {
    QString path = m_filePath;

    auto watcher = new QFutureWatcher<std::optional<SnapshotManager::SaveResult>>(this);
    connect(watcher,
            &QFutureWatcher<std::optional<SnapshotManager::SaveResult>>::finished,
            [this, watcher, isAutosave]() { handleSaveFinished(watcher, isAutosave); });

    watcher->setFuture(
        QtConcurrent::run([path, img]() { return SnapshotManager::saveSnapshot(path, img); }));
}

void ImageSession::handleSaveFinished(
    QFutureWatcher<std::optional<SnapshotManager::SaveResult>> *watcher, bool isAutosave) {
    if (!isAutosave) {
        QApplication::restoreOverrideCursor();
    }

    auto res = watcher->result();
    if (res && res->status == SnapshotManager::SaveStatus::Created) {
        bool wasViewingCurrent = isCurrentImage(m_selectedIndex);
        rebuildSnapshotList();

        if (wasViewingCurrent) {
            m_selectedIndex++;
            emit imageChanged();
        }

        QString msg = isAutosave ? "Autosave successful." : "New snapshot created successfully.";
        emit    statusMessage(msg);
        emit    snapshotCreated(res->snapshotIndex);
    } else if (res && res->status == SnapshotManager::SaveStatus::Existing && !isAutosave) {
        int     pos = getRelativeVersion(res->snapshotIndex);
        QString msg =
            QString("Current image already saved as snapshot %1.").arg(QString::number(pos));
        emit statusMessage(msg);
    } else if (!res || (res && res->status != SnapshotManager::SaveStatus::Existing &&
                        res->status != SnapshotManager::SaveStatus::Created)) {
        emit statusMessage("Save failed.");
    }
    watcher->deleteLater();
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
    m_snapshots = SnapshotManager::loadSnapshots(m_filePath);
    m_labels.clear();
    for (const auto& v : m_snapshots) {
        m_labels.append(v.timestamp.toString("MMMM d, yyyy h:mm:ss AP"));
    }
    if (!m_isSnapshotOnly) {
        m_labels.append("Current");
    }
    emit snapshotsChanged();
}

std::tuple<QVector<QImage>, QVector<QString>, QVector<int>>
ImageSession::snapshotTimelineThumbnails(int size) {
    QVector<QImage> thumbs;
    QVector<int>    indices;
    thumbs.reserve(m_snapshots.size() + 1);
    indices.reserve(m_snapshots.size() + 1);

    for (int i = 0; i < static_cast<int>(m_snapshots.size()); ++i) {
        thumbs.append(generateThumbnail(i, size));
        indices.append(m_snapshots[i].snapshotIndex);
    }

    if (!m_isSnapshotOnly) {
        thumbs.append(generateThumbnail(static_cast<int>(m_snapshots.size()), size));
        indices.append(-1); // Current image is not a snapshot in the store
    }

    return {thumbs, m_labels, indices};
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
        return getPlaceholder(size);
    }

    QImage result =
        ThumbnailManager::instance().getThumbnail(index,
                                                  size,
                                                  m_filePath,
                                                  index == static_cast<int>(m_snapshots.size()),
                                                  m_snapshots,
                                                  m_diskImage);

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

QSize ImageSession::dimensions() const {
    auto seq = getReconstructionSequence();
    if (seq) {
        return seq->base.size();
    }
    return m_diskImage.size();
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
