#include "ui/imagetab.h"
#include "core/thumbnailcache.h"

#include <QDir>
#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>

ImageTab::ImageTab(QWidget *parent, ImageSession *session, ImageSessionController *controller)
    : QWidget(parent), m_session(session), m_controller(controller) {
    connect(m_session, &ImageSession::imageChanged, this, &ImageTab::onImageChanged);

    connect(m_session, &ImageSession::snapshotsChanged, this, &ImageTab::onSnapshotsChanged);
    connect(m_session, &ImageSession::effectsChanged, this, &ImageTab::onEffectsChanged);
    connect(m_session, &ImageSession::statusMessage, this, &ImageTab::statusMessage);
    connect(m_session, &ImageSession::thumbnailChanged, this, &ImageTab::onThumbnailChanged);
    connect(m_session, &ImageSession::snapshotCreated, this, [this](const QUuid& uuid) {
        emit snapshotCreated(uuid);
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

void ImageTab::setSnapshotOnly(bool snapshotOnly) {
    m_isSnapshotOnly = snapshotOnly;
}

void ImageTab::notifyImageOpened() {
    if (!m_session)
        return;
    emit snapshotChanged(m_session->currentSnapshotIndex());
}

void ImageTab::close() {
    if (m_session) {
        m_session->close();
    }
}

const QImage& ImageTab::diskImage() const {
    if (!m_session) {
        static QImage empty;
        return empty;
    }
    return m_session->diskImage();
}

void ImageTab::selectSnapshot(int index) {
    if (m_controller && m_session) {
        m_session->selectSnapshot(index);
    }
}

void ImageTab::onImageChanged() {
    if (m_session) {
        emit snapshotChanged(m_session->currentSnapshotIndex());
    }
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
    if (!m_session)
        return;
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
    if (m_session) {
        emit snapshotChanged(m_session->currentSnapshotIndex());
    }
}

void ImageTab::saveSnapshot() {
    if (m_controller) {
        m_controller->saveSnapshot();
    }
}

void ImageTab::deleteSnapshot(const QUuid& uuid, bool silent) {
    if (m_controller) {
        m_controller->deleteSnapshot(uuid, silent);
    }
}

void ImageTab::deleteAllSnapshots() {
    if (m_controller) {
        m_controller->deleteAllSnapshots();
    }
}
