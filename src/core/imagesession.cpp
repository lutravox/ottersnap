#include <QFutureWatcher>
#include <QTimer>
#include <QtConcurrent>
#include "core/imagesession.h"
#include "config/appsettings.h"
#include "core/diskutils.h"
#include "core/imagecache.h"

ImageSession::ImageSession(QObject *parent) : QObject(parent), m_monitor(new ImageMonitor(this)) {
    connect(m_monitor, &ImageMonitor::fileChanged, this, &ImageSession::onFileChanged);
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
    rebuildSnapshotList();
    m_monitor->watch(m_filePath);
    m_currentIndex = static_cast<int>(m_snapshots.size());

    // Initialize view state with image dimensions
    m_viewState.resetState(m_diskImage.width(), m_diskImage.height());

    emit imageChanged();
    return true;
}

void ImageSession::close() {
    m_monitor->stop();
    m_filePath.clear();
    m_diskImage = QImage();
    m_cachedImage = QImage();
    m_snapshots.clear();
    m_labels.clear();
    m_currentIndex = 0;
    m_loadedSnapshotIndex = -1;
}

const QImage& ImageSession::currentImage() {
    if (m_currentIndex < 0 || m_currentIndex > static_cast<int>(m_snapshots.size())) {
        static QImage s_null;
        return s_null;
    }
    if (m_currentIndex == static_cast<int>(m_snapshots.size()))
        return m_diskImage;
    if (m_loadedSnapshotIndex == m_currentIndex)
        return m_cachedImage;

    const ImageSnapshot& v = m_snapshots[m_currentIndex];
    auto                 optImg = SnapshotStore::loadSnapshotImage(m_filePath, v.snapshotIndex);
    if (optImg) {
        m_cachedImage = std::move(*optImg);
        m_loadedSnapshotIndex = m_currentIndex;
        return m_cachedImage;
    }
    static QImage s_null;
    return s_null;
}

void ImageSession::selectSnapshot(int index) {
    if (index < 0 || index > static_cast<int>(m_snapshots.size()))
        return;
    m_currentIndex = index;
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
    QImage newImage = DiskUtils::loadImage(m_filePath);
    if (newImage.isNull() || newImage == m_diskImage)
        return;

    if (!m_diskImage.isNull() && AppSettings::autosaveSnapshots()) {
        autosaveSnapshot(m_diskImage);
    }

    m_diskImage = newImage;
    rebuildSnapshotList();
    m_currentIndex = static_cast<int>(m_snapshots.size());
    emit imageChanged();
}

bool ImageSession::autosaveSnapshot(const QImage& img) {
    auto res = SnapshotStore::saveSnapshot(m_filePath, img);
    if (res && res->status == SnapshotStore::SaveStatus::Created) {
        return true;
    }
    return false;
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

std::pair<QVector<QImage>, QVector<QString>> ImageSession::snapshotThumbnails(int size) {
    QVector<QImage> thumbs;
    thumbs.reserve(m_snapshots.size() + 1);

    for (const auto& v : m_snapshots) {
        QImage thumbImg = ImageCache::loadThumbnail(
            SnapshotStore::imageKey(m_filePath), v.snapshotIndex, QSize(size, size), [this, &v]() {
                auto optFull = SnapshotStore::loadSnapshotImage(m_filePath, v.snapshotIndex);
                return optFull.value_or(QImage());
            });

        if (!thumbImg.isNull()) {
            thumbs.append(thumbImg);
        } else {
            thumbs.append(QImage(size, size, QImage::Format_ARGB32));
        }
    }

    QImage current = currentImage();
    if (!current.isNull()) {
        thumbs.append(ImageCache::formatThumbnail(current, size));
    } else {
        thumbs.append(QImage(size, size, QImage::Format_ARGB32));
    }

    return {thumbs, m_labels};
}
