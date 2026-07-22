#include "ui/imagetab.h"
#include "core/imagecache.h"

#include <QDir>
#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>

ImageTab::ImageTab(QWidget *parent) : QWidget(parent), m_session(new ImageSession(this)) {
    connect(m_session, &ImageSession::imageChanged, this, &ImageTab::onImageChanged);
    connect(m_session, &ImageSession::snapshotsChanged, this, &ImageTab::onSnapshotsChanged);
    connect(m_session, &ImageSession::effectsChanged, this, &ImageTab::onEffectsChanged);
    connect(m_session, &ImageSession::statusMessage, this, &ImageTab::statusMessage);

    setupUi();
}

ImageTab::~ImageTab() = default;

void ImageTab::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
}

void ImageTab::openImage(const QString& filePath) {
    if (m_session->openImage(filePath)) {
        emit snapshotChanged(m_session->currentSnapshotIndex());
    }
}

void ImageTab::closeImage() {
    m_session->close();
}

const QImage& ImageTab::currentImage() const {
    return m_session->currentImage();
}

void ImageTab::selectSnapshot(int index) {
    m_session->selectSnapshot(index);
}

void ImageTab::onImageChanged() {
    emit snapshotChanged(m_session->currentSnapshotIndex());
}

void ImageTab::onSnapshotsChanged() {
    emit snapshotsChanged();
}

void ImageTab::onEffectsChanged() {
    if (m_session) {
        emit effectsChanged(m_session->grayscaleEnabled(), m_session->mirrorEnabled());
    }
}

std::pair<QVector<QPixmap>, QVector<QString>> ImageTab::snapshotThumbnails(int size) const {
    auto [images, labels] = m_session->snapshotThumbnails(size);
    QVector<QPixmap> thumbs;
    thumbs.reserve(images.size());

    for (const auto& img : images) {
        thumbs.append(QPixmap::fromImage(img));
    }

    return {thumbs, labels};
}

QPixmap ImageTab::thumbnail(int size) const {
    QImage img = m_session->currentImage();
    if (img.isNull())
        return {};
    return QPixmap::fromImage(ImageCache::formatThumbnail(img, size));
}

void ImageTab::saveSnapshot() {
    m_session->saveSnapshot();
}

void ImageTab::deleteSnapshot(int index) {
    m_session->deleteSnapshot(index);
}
