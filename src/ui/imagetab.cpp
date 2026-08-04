#include "ui/imagetab.h"
#include "core/thumbnailcache.h"

#include <QDir>
#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>

ImageTab::ImageTab(QWidget *parent, ImageSession *session) : QWidget(parent), m_session(session) {
    connect(m_session, &ImageSession::imageChanged, this, &ImageTab::onImageChanged);
    connect(m_session, &ImageSession::snapshotsChanged, this, &ImageTab::onSnapshotsChanged);
    connect(m_session, &ImageSession::effectsChanged, this, &ImageTab::onEffectsChanged);
    connect(m_session, &ImageSession::statusMessage, this, &ImageTab::statusMessage);
    connect(m_session, &ImageSession::thumbnailChanged, this, &ImageTab::onThumbnailChanged);
    connect(m_session, &ImageSession::snapshotCreated, this, [this](int idx) {
        emit snapshotCreated(idx);
    });
    connect(&m_thumbnailUpdateTimer, &QTimer::timeout, this, &ImageTab::onThumbnailTimerTimeout);

    setupUi();
}

ImageTab::~ImageTab() = default;

void ImageTab::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
}

void ImageTab::notifyImageOpened() {
    emit snapshotChanged(m_session->currentSnapshotIndex());
}

void ImageTab::close() {
    m_session->close();
}

const QImage& ImageTab::diskImage() const {
    return m_session->diskImage();
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

void ImageTab::onThumbnailChanged(int index) {
    QImage img = m_session->generateThumbnail(index, ThumbnailConstants::StandardSize);
    if (!img.isNull()) {
        emit thumbnailUpdated(index, QPixmap::fromImage(img));
    }

    if (index == static_cast<int>(m_session->snapshots().size())) {
        if (!img.isNull()) {
            emit tabIconChanged(QPixmap::fromImage(img));
        }
    }
    repaint();
}

void ImageTab::onThumbnailTimerTimeout() {
    emit snapshotChanged(m_session->currentSnapshotIndex());
}

void ImageTab::saveSnapshot() {
    m_session->saveSnapshot();
}

void ImageTab::deleteSnapshot(int index) {
    m_session->deleteSnapshot(index);
}
