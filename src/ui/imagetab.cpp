#include "ui/imagetab.h"
#include "config/appsettings.h"
#include "core/imageloader.h"
#include "core/versionstore.h"

#include <QDir>
#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>

ImageTab::ImageTab(QWidget *parent)
    : QWidget(parent), m_currentIndex(0), m_grayscale(false), m_mirror(false),
      m_watcher(new QFileSystemWatcher(this)), m_debounceTimer(new QTimer(this)) {
    m_debounceTimer->setSingleShot(true);
    connect(&m_saveWatcher,
            &QFutureWatcher<std::optional<VersionStore::SaveResult>>::finished,
            this,
            &ImageTab::onSaveFinished);

    setupUi();
}

ImageTab::~ImageTab() = default;

void ImageTab::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
}

void ImageTab::openImage(const QString& filePath) {
    m_filePath = filePath;
    m_diskImage = loadImage(m_filePath);
    if (m_diskImage.isNull()) {
        emit statusMessage(tr("Failed to load: %1").arg(m_filePath));
        return;
    }

    rebuildImageList();
    m_watcher->addPath(m_filePath);

    // The current image on disk is always the most recent version
    m_currentIndex = static_cast<int>(m_versions.size());
    emit versionChanged(m_currentIndex);
}

void ImageTab::closeImage() {
    m_watcher->removePath(m_filePath);
    m_filePath.clear();
    m_diskImage = QImage();
    m_cachedImage = QImage();
    m_versions.clear();
    m_labels.clear();
    m_currentIndex = 0;
    m_loadedVersionIndex = -1;
}

void ImageTab::rebuildImageList() {
    m_versions.clear();
    m_labels.clear();

    auto versions = VersionStore::loadVersions(m_filePath);
    for (const auto& v : versions) {
        m_versions.append(v);
        m_labels.append(tr("v%1 — %2").arg(v.version).arg(v.timestamp.toString(Qt::ISODate)));
    }

    // The current file on disk is always the most recent version
    m_labels.append(tr("Current"));

    emit versionsChanged();
}

const QImage& ImageTab::currentImage() const {
    if (m_currentIndex < 0 || m_currentIndex > static_cast<int>(m_versions.size())) {
        static QImage s_null;
        return s_null;
    }

    auto *mutableThis = const_cast<ImageTab *>(this);

    // Current image (the one on disk) is always at index m_versions.size()
    if (m_currentIndex == static_cast<int>(m_versions.size())) {
        return mutableThis->m_diskImage;
    }

    // Return cached image if it matches current index
    if (mutableThis->m_loadedVersionIndex == m_currentIndex) {
        return mutableThis->m_cachedImage;
    }

    // Load from store and cache
    const ImageVersion& v = m_versions[m_currentIndex];
    auto                optImg = VersionStore::loadVersionImage(m_filePath, v.version);
    if (optImg.has_value()) {
        mutableThis->m_cachedImage = std::move(optImg).value();
        mutableThis->m_loadedVersionIndex = m_currentIndex;
        return mutableThis->m_cachedImage;
    }

    static QImage s_null;
    return s_null;
}

void ImageTab::setGrayscale(bool enabled) {
    if (m_grayscale == enabled)
        return;
    m_grayscale = enabled;
    emit modifiersChanged(m_grayscale, m_mirror);
}

void ImageTab::setMirror(bool enabled) {
    if (m_mirror == enabled)
        return;
    m_mirror = enabled;
    emit modifiersChanged(m_grayscale, m_mirror);
}

static QPixmap createCenteredThumbnail(const QImage& image, int size) {
    QPixmap scaled =
        QPixmap::fromImage(image).scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPixmap canvas(size, size);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QPoint dest((canvas.width() - scaled.width()) / 2, (canvas.height() - scaled.height()) / 2);
    painter.drawPixmap(dest, scaled);
    return canvas;
}

void ImageTab::selectVersion(int index) {
    if (index < 0 || index > static_cast<int>(m_versions.size())) {
        qDebug() << "[ImageTab] index of version selection out of bounds";
        return;
    }
    m_currentIndex = index;
    emit versionChanged(m_currentIndex);
}

void ImageTab::onFileChanged() {
    // Re-add watcher — Linux drops the path after the event fires.
    m_watcher->addPath(m_filePath);
    reloadImage();
}

void ImageTab::reloadImage() {
    QFile f(m_filePath);
    if (!f.exists()) {
        emit statusMessage(tr("Failed to reload, file unavailable: %1")
                               .arg(m_filePath.section(QLatin1Char('/'), -1)));
        return;
    }

    // Verify the file isn't still being written to
    qint64 size1 = f.size();
    if (size1 <= 0) {
        emit statusMessage(
            tr("Failed to reload, file empty: %1").arg(m_filePath.section(QLatin1Char('/'), -1)));
        return;
    }
    m_stableSize = size1;
    QTimer::singleShot(200, this, &ImageTab::tryReload);
}

void ImageTab::tryReload() {
    QFile f(m_filePath);
    if (f.size() != m_stableSize)
        emit statusMessage(tr("Failed to reload, file may be in use: %1")
                               .arg(m_filePath.section(QLatin1Char('/'), -1)));
    return;

    QImage newImage = loadImage(m_filePath);

    if (newImage == m_diskImage)
        return;

    if (!m_diskImage.isNull() && AppSettings::autosaveSnapshots()) {
        autosaveSnapshot(newImage);
    }

    m_diskImage = newImage;
    rebuildImageList();
    m_currentIndex = static_cast<int>(m_versions.size());
    emit versionChanged(m_currentIndex);
}

bool ImageTab::autosaveSnapshot(const QImage& newImage) {
    if (newImage.isNull()) {
        emit statusMessage(tr("Failed to autosave, file may be in use: %1")
                               .arg(m_filePath.section(QLatin1Char('/'), -1)));
        return false;
    }

    if (!m_diskImage.isNull() && AppSettings::autosaveSnapshots()) {
        auto result = VersionStore::saveVersion(m_filePath, m_diskImage);
        if (result && result->status == VersionStore::SaveStatus::Created) {
            emit statusMessage(
                tr("Snapshot autosaved for: %1").arg(m_filePath.section(QLatin1Char('/'), -1)));
        }
    }
    return true;
}

std::pair<QVector<QPixmap>, QVector<QString>> ImageTab::versionThumbnails(int size) const {
    QVector<QPixmap> thumbs;
    thumbs.reserve(m_versions.size() + 1);

    QString cacheDir = VersionStore::thumbnailDir() + '/' + VersionStore::imageKey(m_filePath);
    QDir().mkpath(cacheDir);

    for (const auto& v : m_versions) {
        QString thumbPath = cacheDir + '/' + QString::asprintf("v%04d.jpg", v.version);
        QImage  thumbImg;

        if (QFile::exists(thumbPath)) {
            // Found a cached thumbnail
            thumbImg = QImage(thumbPath);
        } else {
            // Load full image to create thumbnail
            auto optFull = VersionStore::loadVersionImage(m_filePath, v.version);
            if (optFull.has_value()) {
                QImage full = optFull.value();
                // Generate thumbnail
                QPixmap scaled = QPixmap::fromImage(full).scaled(
                    size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                thumbImg = scaled.toImage();
                thumbImg.save(thumbPath, "JPG");
            }
        }

        if (!thumbImg.isNull()) {
            thumbs.append(QPixmap::fromImage(thumbImg));
        } else {
            // Fallback: empty thumbnail
            thumbs.append(QPixmap(size, size));
            thumbs.last().fill(Qt::transparent);
        }
    }

    // Add the thumbnail for the "Current" image
    if (!m_diskImage.isNull()) {
        thumbs.append(thumbnail(size));
    } else {
        thumbs.append(QPixmap(size, size));
        thumbs.last().fill(Qt::transparent);
    }

    return {thumbs, m_labels};
}

QPixmap ImageTab::thumbnail(int size) const {
    if (m_diskImage.isNull())
        return {};
    return QPixmap::fromImage(m_diskImage)
        .scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void ImageTab::saveSnapshot() {
    if (m_diskImage.isNull()) {
        emit statusMessage(tr("No image loaded to snapshot."));
        return;
    }

    if (m_saveWatcher.isRunning()) {
        emit statusMessage(tr("Snapshot already in progress..."));
        return;
    }

    emit statusMessage(tr("Saving snapshot..."));

    auto filePath = m_filePath;
    auto image = m_diskImage;

    auto future = QtConcurrent::run(
        [filePath, image]() { return VersionStore::saveVersion(filePath, image); });

    m_saveWatcher.setFuture(future);
}

void ImageTab::onSaveFinished() {
    auto result = m_saveWatcher.result();
    if (result) {
        if (result->status == VersionStore::SaveStatus::Created) {
            rebuildImageList();
            emit statusMessage(tr("Snapshot saved."));
        } else {
            emit statusMessage(tr("This version is already snapshotted (%1)").arg(result->version),
                               5000);
        }
    } else {
        emit statusMessage(tr("Failed to save snapshot."));
    }
}
