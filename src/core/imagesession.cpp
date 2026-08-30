#include <QApplication>
#include <QCache>
#include <QDateTime>
#include <QFileInfo>
#include <QThreadPool>
#include <QTimer>
#include <QtConcurrent>
#include <qlogging.h>
#include <qnamespace.h>
#include <qobject.h>
#include "core/imagesession.h"
#include "config/appsettings.h"
#include "core/coloranalyzer.h"
#include "core/diskutils.h"
#include "core/thumbnailcache.h"
#include "core/thumbnailmanager.h"
#include "core/vksnapshotreconstructor.h"
#include "core/vulkancontext.h"

ImageSession::ImageSession(QObject *parent) : QObject(parent), m_monitor(new ImageMonitor(this)) {
    m_clusterCache.setMaxCost(MaxClusterCacheSize);
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
    ThumbnailCache::invalidate(SnapshotManager::cacheKeyForPath(filePath),
                               ThumbnailConstants::CurrentVersion);

    m_filePath = filePath;
    m_lastModified = QFileInfo(m_filePath).lastModified();
    if (m_isSnapshotOnly) {
        m_diskImage = QImage();
    } else {
        m_diskImage = DiskUtils::loadImage(m_filePath);
        if (m_diskImage.isNull()) {
            emit statusMessage(QString("Failed to load!: %1").arg(m_filePath));
            return false;
        }
    }

    rebuildSnapshotList();
    m_monitor->watch(m_filePath);

    if (m_isSnapshotOnly && m_snapshots.isEmpty()) {
        return false;
    }

    m_currentUuid =
        m_isSnapshotOnly ? m_snapshots[0].uuid.toString(QUuid::WithoutBraces) : c_currentId;
    updateColorClusters();

    // Initialize view state with image dimensions
    QSize dims = m_diskImage.size();
    if (m_isSnapshotOnly && !m_snapshots.isEmpty()) {
        auto optBase = SnapshotManager::loadBaseImage(m_filePath, m_snapshots[0]);
        if (optBase) {
            dims = optBase->image.size();
        }
    }
    m_viewState.resetState(dims.width(), dims.height());

    emit imageChanged();
    return true;
}

bool ImageSession::setFilePath(const QString& newPath) {
    if (m_filePath.isEmpty())
        return false;

    QString normalized = SnapshotManager::normalizePath(newPath);
    if (normalized == m_filePath)
        return false;

    QString oldPath = m_filePath;
    switch (SnapshotManager::updateImagePath(oldPath, normalized)) {
        case SnapshotManager::UpdatePathResult::Ok:
            break;
        case SnapshotManager::UpdatePathResult::TargetAlreadyRegistered:
            emit statusMessage(tr("Cannot update path: that file already has its own "
                                  "snapshot history."));
            return false;
        case SnapshotManager::UpdatePathResult::Failed:
            emit statusMessage(tr("Failed to update image path: %1").arg(normalized));
            return false;
    }

    QImage newImage = DiskUtils::loadImage(normalized);
    if (newImage.isNull()) {
        SnapshotManager::updateImagePath(normalized, oldPath);
        emit statusMessage(tr("Failed to load image at new path: %1").arg(normalized));
        return false;
    }

    m_filePath = normalized;
    m_lastModified = QFileInfo(m_filePath).lastModified();
    m_diskImage = newImage;
    m_isSnapshotOnly = false;
    ThumbnailCache::invalidate(SnapshotManager::cacheKeyForPath(m_filePath),
                               ThumbnailConstants::CurrentVersion);
    m_baseCache = {-1, QImage()};
    m_clusterCache.clear();
    m_monitor->watch(m_filePath);

    rebuildSnapshotList();
    updateColorClusters();

    QSize dims = dimensions();
    if (dims != QSize(m_viewState.imageWidth(), m_viewState.imageHeight())) {
        m_viewState.updateImageSize(dims.width(), dims.height());
    }

    emit statusMessage(tr("Image path updated to: %1").arg(m_filePath));
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
    m_currentUuid = c_currentId;
    m_secondarySnapshotId = c_secondaryNoneId;
    m_baseCache = {-1, QImage()};
    m_clusterCache.clear();

    if (m_uiReconstructor) {
        m_uiReconstructor->cleanup();
        m_uiReconstructor.reset();
    }
}

std::optional<ReconstructionSequence> ImageSession::getReconstructionSequence() const {
    return getReconstructionSequence(currentSnapshotIndex());
}

std::optional<ReconstructionSequence> ImageSession::getReconstructionSequence(int index) const {
    if (!m_isSnapshotOnly && index == static_cast<int>(m_snapshots.size())) {
        ReconstructionSequence seq;
        seq.base = m_diskImage;
        seq.baseChecksum = QString("%1:%2").arg(m_filePath, m_lastModified.toString(Qt::ISODate));
        return seq;
    }

    if (index < 0 || index >= static_cast<int>(m_snapshots.size())) {
        return std::nullopt;
    }

    // Walk the snapshot's own parent chain: other chains interleaved on the
    // timeline are never part of this sequence.
    auto chainOpt = SnapshotManager::snapshotChain(m_snapshots, m_snapshots[index]);
    if (!chainOpt)
        return std::nullopt;
    const QVector<ImageSnapshot>& chain = *chainOpt;
    const ImageSnapshot&          base  = chain.first();

    int baseIdx = -1;
    for (int i = 0; i < static_cast<int>(m_snapshots.size()); ++i) {
        if (m_snapshots[i].uuid == base.uuid) {
            baseIdx = i;
            break;
        }
    }

    ReconstructionSequence seq;

    if (m_baseCache.index == baseIdx && !m_baseCache.image.isNull()) {
        seq.base = m_baseCache.image;
        seq.baseChecksum = base.checksum;
    } else {
        auto optBase = SnapshotManager::loadBaseImage(m_filePath, base);
        if (!optBase)
            return std::nullopt;

        seq.base = std::move(optBase->image);
        seq.baseChecksum = optBase->checksum;
        m_baseCache = {baseIdx, seq.base};
    }

    QString imageKey = SnapshotManager::cacheKeyForPath(m_filePath);
    for (int i = 1; i < chain.size(); ++i) {
        auto optDelta = SnapshotManager::loadDelta(m_filePath, chain[i]);
        if (!optDelta)
            return std::nullopt;
        seq.deltas.append(DeltaEntry{imageKey + ":" + chain[i].fileName, std::move(*optDelta)});
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

    int oldIndex = currentSnapshotIndex();
    if (oldIndex == index)
        return;

    if (index == static_cast<int>(m_snapshots.size())) {
        m_currentUuid = c_currentId;
    } else if (index >= 0 && index < static_cast<int>(m_snapshots.size())) {
        m_currentUuid = m_snapshots[index].uuid.toString(QUuid::WithoutBraces);
    } else {
        m_currentUuid = c_currentId;
    }

    // Update view state if dimensions change
    QSize newDims = dimensions();
    if (newDims != QSize(m_viewState.imageWidth(), m_viewState.imageHeight())) {
        m_viewState.updateImageSize(newDims.width(), newDims.height());
    }

    updateColorClusters();
    emit imageChanged();
}

void ImageSession::selectSnapshot(const QUuid& uuid) {
    if (uuid.isNull()) {
        selectSnapshot(c_currentId);
        return;
    }
    selectSnapshot(uuid.toString(QUuid::WithoutBraces));
}

void ImageSession::selectSnapshot(const QString& uuid) {
    if (m_currentUuid == uuid)
        return;

    m_currentUuid = uuid;

    // Update view state if dimensions change
    QSize newDims = dimensions();
    if (newDims != QSize(m_viewState.imageWidth(), m_viewState.imageHeight())) {
        m_viewState.updateImageSize(newDims.width(), newDims.height());
    }

    updateColorClusters();
    emit imageChanged();
}

void ImageSession::saveSnapshot() {
    if (m_diskImage.isNull())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    performSave(m_diskImage, false);
}

void ImageSession::deleteSnapshot(const QUuid& uuid, bool silent) {
    if (uuid.isNull())
        return;

    int relativeVersion = -1;
    for (int i = 0; i < static_cast<int>(m_snapshots.size()); ++i) {
        if (m_snapshots[i].uuid == uuid) {
            relativeVersion = i + 1;
            break;
        }
    }

    QString path = m_filePath;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto    watcher = new QFutureWatcher<std::optional<QVector<ImageSnapshot>>>(this);
    connect(watcher, &QObject::destroyed, []() { QApplication::restoreOverrideCursor(); });
    connect(watcher, &QFutureWatcher<std::optional<QVector<ImageSnapshot>>>::finished, [this, watcher, uuid, relativeVersion, silent]() {
        auto result = watcher->result();
        watcher->deleteLater();

        applySnapshotDeletion(uuid, relativeVersion, silent, result);
        emit deletionFinished();
    });
    watcher->setFuture(QtConcurrent::run([path, uuid]() { return SnapshotManager::deleteSnapshot(path, uuid); }));
}

void ImageSession::deleteSnapshots(const QVector<QUuid>& uuids, bool silent) {
    if (uuids.isEmpty() || m_filePath.isEmpty())
        return;

    QString path = m_filePath;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto    watcher = new QFutureWatcher<std::optional<QVector<ImageSnapshot>>>(this);
    connect(watcher, &QObject::destroyed, []() { QApplication::restoreOverrideCursor(); });
    connect(
        watcher,
        &QFutureWatcher<std::optional<QVector<ImageSnapshot>>>::finished,
        [this, watcher, uuids, silent]() {
            auto result = watcher->result();
            watcher->deleteLater();

            applySnapshotDeletions(uuids, silent, result);
            emit deletionFinished();
        });
    watcher->setFuture(
        QtConcurrent::run([path, uuids]() { return SnapshotManager::deleteSnapshots(path, uuids); }));
}

void ImageSession::applySnapshotDeletion(const QUuid& uuid, int relativeVersion, bool silent, const std::optional<QVector<ImageSnapshot>>& result) {
    if (!result) {
        if (!silent)
            emit statusMessage(tr("Failed to delete snapshot!"));
        return;
    }

    m_clusterCache.remove(uuid.toString(QUuid::WithoutBraces));
    applySnapshotList(*result);

    // Update view state if dimensions change
    QSize newDims = dimensions();
    if (newDims != QSize(m_viewState.imageWidth(), m_viewState.imageHeight())) {
        m_viewState.updateImageSize(newDims.width(), newDims.height());
    }

    emit imageChanged();
    if (!silent) {
        emit statusMessage(tr("Snapshot %1 deleted successfully.").arg(relativeVersion));
    }
}

void ImageSession::applySnapshotDeletions(const QVector<QUuid>& uuids,
                                          bool silent,
                                          const std::optional<QVector<ImageSnapshot>>& result) {
    if (!result) {
        if (!silent)
            emit statusMessage(tr("Failed to delete snapshots!"));
        return;
    }

    for (const auto& uuid : uuids) {
        m_clusterCache.remove(uuid.toString(QUuid::WithoutBraces));
    }
    applySnapshotList(*result);

    // Update view state if dimensions change
    QSize newDims = dimensions();
    if (newDims != QSize(m_viewState.imageWidth(), m_viewState.imageHeight())) {
        m_viewState.updateImageSize(newDims.width(), newDims.height());
    }

    emit imageChanged();
    if (!silent) {
        emit statusMessage(tr("Deleted %1 snapshots successfully.").arg(uuids.size()));
    }
}

void ImageSession::deleteAllSnapshots() {
    if (m_filePath.isEmpty())
        return;

    int     total = m_snapshots.size();
    QString path = m_filePath;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto    watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QObject::destroyed, []() { QApplication::restoreOverrideCursor(); });
    connect(watcher, &QFutureWatcher<bool>::finished, [this, watcher, total]() {
        bool ok = watcher->result();
        watcher->deleteLater();

        if (ok) {
            m_clusterCache.clear();
            applySnapshotList(QVector<ImageSnapshot>());
            emit imageChanged();
            emit statusMessage(tr("Deleted all %1 snapshots.").arg(total));
        }
        emit deletionFinished();
    });
    watcher->setFuture(QtConcurrent::run([path]() {
        SnapshotManager::deleteAllSnapshots(path);
        return true;
    }));
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

        ThumbnailCache::invalidate(SnapshotManager::cacheKeyForPath(m_filePath),
                                   ThumbnailConstants::CurrentVersion);

        if (!m_diskImage.isNull() && AppSettings::autosaveSnapshots()) {
            autosaveSnapshot(m_diskImage);
        }

        m_diskImage = newImage;
        m_lastModified = QFileInfo(m_filePath).lastModified();
        m_clusterCache.remove(c_currentId);
        updateColorClusters();
        emit statusMessage(tr("Current image reloaded."));
        emit imageChanged();
    });

    watcher->setFuture(QtConcurrent::run([path]() { return DiskUtils::loadImage(path); }));
}

void ImageSession::autosaveSnapshot(const QImage& img) {
    performSave(img, true);
}

void ImageSession::updateColorClusters() {
    QImage thumb = ThumbnailManager::instance().getThumbnail(currentSnapshotIndex(),
                                                             256,
                                                             m_filePath,
                                                             isCurrentImage(currentSnapshotIndex()),
                                                             m_snapshots,
                                                             m_diskImage);
    if (thumb.isNull()) {
        // Thumbnail not yet available, we'll be notified via handleThumbnailGenerated
        return;
    }

    QString clusterId = m_currentUuid;

    if (auto *cached = m_clusterCache.object(clusterId)) {
        m_colorClusters = *cached;
        emit colorClustersChanged();
        return;
    }

    // Calculate clusters
    auto watcher = new QFutureWatcher<QList<ColorAnalyzer::ColorCluster>>(this);
    connect(watcher,
            &QFutureWatcher<QList<ColorAnalyzer::ColorCluster>>::finished,
            this,
            [this, watcher, clusterId]() {
                QList<ColorAnalyzer::ColorCluster> result = watcher->result();
                m_clusterCache.insert(clusterId, new QList<ColorAnalyzer::ColorCluster>(result));
                m_colorClusters = result;
                watcher->deleteLater();
                emit colorClustersChanged();
            });

    watcher->setFuture(
        QtConcurrent::run([thumb]() { return ColorAnalyzer::calculateClusters(thumb); }));
}

void ImageSession::performSave(const QImage& img, bool isAutosave) {
    QString path = m_filePath;

    auto watcher = new QFutureWatcher<std::optional<SnapshotManager::SaveResult>>(this);
    if (!isAutosave) {
        connect(watcher, &QObject::destroyed, []() { QApplication::restoreOverrideCursor(); });
    }
    connect(watcher,
            &QFutureWatcher<std::optional<SnapshotManager::SaveResult>>::finished,
            [this, watcher, isAutosave]() { handleSaveFinished(watcher, isAutosave); });

    watcher->setFuture(
        QtConcurrent::run([path, img]() { return SnapshotManager::saveSnapshot(path, img); }));
}

void ImageSession::handleSaveFinished(
    QFutureWatcher<std::optional<SnapshotManager::SaveResult>> *watcher, bool isAutosave) {
    auto res = watcher->result();
    if (res && res->status == SnapshotManager::SaveStatus::Created) {
        bool wasViewingCurrent = isCurrentImageSelected();
        rebuildSnapshotList();

        if (wasViewingCurrent) {
            m_currentUuid = c_currentId;
            emit imageChanged();
        }

        QString msg = isAutosave ? "Autosave successful." : "New snapshot created successfully.";
        emit    statusMessage(msg);
        emit    snapshotCreated(res->uuid);
    } else if (res && res->status == SnapshotManager::SaveStatus::Existing && !isAutosave) {
        QString msg = QString("Current image already saved as latest snapshot.");
        emit    statusMessage(msg);
    } else if (!res || (res && res->status != SnapshotManager::SaveStatus::Existing &&
                        res->status != SnapshotManager::SaveStatus::Created)) {
        emit statusMessage("Save failed!");
    }
    watcher->deleteLater();
}

int ImageSession::getRelativeVersion(const QUuid& uuid) const {
    for (int i = 0; i < static_cast<int>(m_snapshots.size()); ++i) {
        if (m_snapshots[i].uuid == uuid) {
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
    applySnapshotList(SnapshotManager::loadSnapshots(m_filePath));
}

void ImageSession::applySnapshotList(const QVector<ImageSnapshot>& list) {
    m_snapshots = list;
    m_labels.clear();
    for (const auto& v : m_snapshots) {
        m_labels.append(v.timestamp.toString("MMMM d, yyyy h:mm:ss AP"));
    }
    if (!m_isSnapshotOnly) {
        m_labels.append("Current");
    }

    // Validate current selection: if the selected snapshot no longer exists,
    // fall back to the disk image (if available).
    if (m_currentUuid != c_currentId) {
        bool stillExists = false;
        for (const auto& s : m_snapshots) {
            if (s.uuid.toString(QUuid::WithoutBraces) == m_currentUuid) {
                stillExists = true;
                break;
            }
        }
        if (!stillExists) {
            m_currentUuid = m_isSnapshotOnly && !m_snapshots.isEmpty()
                                ? m_snapshots[0].uuid.toString(QUuid::WithoutBraces)
                                : c_currentId;
            emit imageChanged();
        }
    }

    // Validate secondary selection: if it no longer exists, reset it.
    if (m_secondarySnapshotId != c_secondaryNoneId && m_secondarySnapshotId != c_currentId) {
        bool stillExists = false;
        for (const auto& s : m_snapshots) {
            if (s.uuid.toString(QUuid::WithoutBraces) == m_secondarySnapshotId) {
                stillExists = true;
                break;
            }
        }
        if (!stillExists) {
            m_secondarySnapshotId = c_secondaryNoneId;
            emit secondarySnapshotChanged(m_secondarySnapshotId);
        }
    }

    emit snapshotsChanged();
}

std::tuple<QVector<QImage>, QVector<QString>, QVector<QUuid>>
ImageSession::snapshotTimelineThumbnails(int size) {
    QVector<QImage> thumbs;
    QVector<QUuid>  indices;
    thumbs.reserve(m_snapshots.size() + 1);
    indices.reserve(m_snapshots.size() + 1);

    for (int i = 0; i < static_cast<int>(m_snapshots.size()); ++i) {
        thumbs.append(generateThumbnail(i, size));
        indices.append(m_snapshots[i].uuid);
    }

    if (!m_isSnapshotOnly) {
        thumbs.append(generateThumbnail(static_cast<int>(m_snapshots.size()), size));
        indices.append(QUuid()); // Current image is not a snapshot in the store
    }

    return {thumbs, m_labels, indices};
}

QImage ImageSession::thumbnail(int size) {
    QImage img = generateThumbnail(currentSnapshotIndex(), size);
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

void ImageSession::handleThumbnailGenerated(const QString& path, const QUuid& uuid) {
    if (path == m_filePath) {
        // Find the current position of this snapshot in the list
        int index = -1;
        if (uuid.isNull()) {
            index = static_cast<int>(m_snapshots.size());
        } else {
            for (int i = 0; i < static_cast<int>(m_snapshots.size()); ++i) {
                if (m_snapshots[i].uuid == uuid) {
                    index = i;
                    break;
                }
            }
        }

        if (index != -1) {
            emit thumbnailChanged(index);
            if (index == currentSnapshotIndex()) {
                updateColorClusters();
            }
        }
    }
}

QImage ImageSession::generateThumbnail(int index, int size, bool padded) {
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

    if (padded) {
        return ThumbnailManager::formatThumbnail(result, size);
    }
    return result;
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

QString ImageSession::currentImageTimestamp() const {
    if (m_currentUuid == c_currentId) {
        return tr("Current");
    }

    if (m_currentUuid != c_currentId && !m_currentUuid.isEmpty()) {
        for (const auto& s : m_snapshots) {
            if (s.uuid.toString(QUuid::WithoutBraces) == m_currentUuid) {
                return s.timestamp.toString("MMMM d, yyyy h:mm:ss AP");
            }
        }
    }

    return tr("Current");
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
