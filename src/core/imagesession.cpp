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

    const ImageVersion& v = m_snapshots[m_currentIndex];
    auto                optImg = VersionStore::loadVersionImage(m_filePath, v.version);
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

    auto watcher = new QFutureWatcher<std::optional<VersionStore::SaveResult>>(this);
    connect(watcher,
            &QFutureWatcher<std::optional<VersionStore::SaveResult>>::finished,
            [this, watcher]() {
                auto res = watcher->result();
                if (res && res->status == VersionStore::SaveStatus::Created) {
                    emit statusMessage("Snapshot saved.");
                    rebuildSnapshotList();
                } else {
                    emit statusMessage(res ? "Version already exists." : "Save failed.");
                }
                watcher->deleteLater();
            });
    watcher->setFuture(
        QtConcurrent::run([path, img]() { return VersionStore::saveVersion(path, img); }));
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
    auto res = VersionStore::saveVersion(m_filePath, img);
    if (res && res->status == VersionStore::SaveStatus::Created) {
        emit statusMessage("Snapshot autosaved.");
        return true;
    }
    return false;
}

void ImageSession::rebuildSnapshotList() {
    m_snapshots = VersionStore::loadVersions(m_filePath);
    m_labels.clear();
    for (const auto& v : m_snapshots) {
        m_labels.append(QString("v%1 — %2").arg(v.version).arg(v.timestamp.toString()));
    }
    m_labels.append("Current");
    emit snapshotsChanged();
}

std::pair<QVector<QImage>, QVector<QString>> ImageSession::snapshotThumbnails(int size) {
    QVector<QImage> thumbs;
    thumbs.reserve(m_snapshots.size() + 1);

    for (const auto& v : m_snapshots) {
        QImage thumbImg = ImageCache::loadThumbnail(
            VersionStore::imageKey(m_filePath), v.version, QSize(size, size), [this, &v]() {
                auto optFull = VersionStore::loadVersionImage(m_filePath, v.version);
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
