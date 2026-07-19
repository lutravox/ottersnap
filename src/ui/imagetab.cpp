#include "ui/imagetab.h"
#include "core/imageloader.h"
#include "core/versionstore.h"

#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QVBoxLayout>

ImageTab::ImageTab(QWidget *parent)
    : QWidget(parent), m_currentIndex(0), m_grayscale(false), m_mirror(false),
      m_watcher(new QFileSystemWatcher(this)), m_debounceTimer(new QTimer(this)) {
    m_debounceTimer->setSingleShot(true);
    connect(m_debounceTimer, &QTimer::timeout, this, &ImageTab::onFileChanged);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this]() {
        m_debounceTimer->start(1000); // 1s debounce — give external editors time to finish writing
    });

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
    m_image = loadImage(m_filePath);
    if (m_image.isNull()) {
        emit statusMessage(tr("Failed to load: %1").arg(m_filePath));
        return;
    }

    rebuildImageList();
    m_watcher->addPath(m_filePath);

    m_currentIndex = static_cast<int>(m_images.size()) - 1;
    emit versionChanged(m_currentIndex);
}

void ImageTab::closeImage() {
    m_watcher->removePath(m_filePath);
    m_filePath.clear();
    m_image = QImage();
    m_images.clear();
    m_labels.clear();
    m_currentIndex = 0;
}

void ImageTab::rebuildImageList() {
    m_images.clear();
    m_labels.clear();

    auto versions = VersionStore::loadVersions(m_filePath);
    for (const auto& v : versions) {
        auto versionImg = VersionStore::loadVersionImage(m_filePath, v.version);
        if (versionImg.has_value()) {
            m_images.append(std::move(versionImg).value());
            m_labels.append(tr("v%1 — %2").arg(v.version).arg(v.timestamp.toString(Qt::ISODate)));
        }
    }

    if (!m_image.isNull()) {
        m_images.append(m_image);
        m_labels.append(tr("Current"));
    }
}

const QImage& ImageTab::currentImage() const {
    if (m_currentIndex < 0 || m_currentIndex >= static_cast<int>(m_images.size())) {
        static QImage s_null;
        return s_null;
    }
    return m_images[m_currentIndex];
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
    if (index < 0 || index >= static_cast<int>(m_images.size()))
        return;
    m_currentIndex = index;
    emit versionChanged(m_currentIndex);
}

void ImageTab::onFileChanged() {
    // Re-add watcher — Linux drops the path after the event fires.
    m_watcher->addPath(m_filePath);
    reloadCurrentFile();
}

void ImageTab::reloadCurrentFile() {
    QFile f(m_filePath);
    if (!f.exists()) {
        emit statusMessage(
            tr("File unavailable: %1").arg(m_filePath.section(QLatin1Char('/'), -1)));
        return;
    }

    // Verify the file isn't still being written to
    qint64 size1 = f.size();
    if (size1 <= 0) {
        emit statusMessage(
            tr("File empty, reload skipped: %1").arg(m_filePath.section(QLatin1Char('/'), -1)));
        return;
    }
    m_stableSize = size1;
    QTimer::singleShot(200, this, &ImageTab::checkFileStable);
}

void ImageTab::checkFileStable() {
    QFile f(m_filePath);
    if (f.size() != m_stableSize)
        return;

    QImage newImage = loadImage(m_filePath);
    if (newImage.isNull()) {
        emit statusMessage(tr("Failed to reload, file may still be in use: %1")
                               .arg(m_filePath.section(QLatin1Char('/'), -1)));
        return;
    }
    if (newImage == m_image)
        return;

    if (!m_image.isNull()) {
        VersionStore::saveVersion(m_filePath, m_image);
        emit statusMessage(
            tr("Auto-saved version for: %1").arg(m_filePath.section(QLatin1Char('/'), -1)));
    }

    m_image = newImage;
    rebuildImageList();
    m_currentIndex = static_cast<int>(m_images.size()) - 1;
    emit versionChanged(m_currentIndex);
}

std::pair<QVector<QPixmap>, QVector<QString>> ImageTab::versionThumbnails(int size) const {
    QVector<QPixmap> thumbs;
    thumbs.reserve(m_images.size());
    for (const QImage& img : m_images) {
        thumbs.append(createCenteredThumbnail(img, size));
    }
    return {thumbs, m_labels};
}

QPixmap ImageTab::thumbnail(int size) const {
    if (m_image.isNull())
        return {};
    return QPixmap::fromImage(m_image).scaled(
        size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}
