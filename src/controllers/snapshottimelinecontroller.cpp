#include <QPixmap>
#include "controllers/snapshottimelinecontroller.h"
#include "core/imagesession.h"
#include "core/snapshottimelinemodel.h"

SnapshotTimelineController::SnapshotTimelineController(QObject *parent) : QObject(parent) {
    m_model = std::make_unique<SnapshotTimelineModel>();
}

void SnapshotTimelineController::setSessionController(ImageSessionController *controller) {
    if (m_sessionController == controller)
        return;

    if (m_sessionController) {
        disconnect(m_sessionController,
                   &ImageSessionController::activeSessionChanged,
                   this,
                   &SnapshotTimelineController::onActiveSessionChanged);
        disconnect(m_sessionController,
                   &ImageSessionController::activeSessionSnapshotsChanged,
                   this,
                   &SnapshotTimelineController::onSessionChanged);
    }

    m_sessionController = controller;

    if (m_sessionController) {
        connect(m_sessionController,
                &ImageSessionController::activeSessionChanged,
                this,
                &SnapshotTimelineController::onActiveSessionChanged);
        connect(m_sessionController,
                &ImageSessionController::activeSessionSnapshotsChanged,
                this,
                &SnapshotTimelineController::onSessionChanged);
    }
}

void SnapshotTimelineController::onActiveSessionChanged(ImageSession *session) {
    if (session) {
        updateModel();
    } else {
        m_model->setThumbnails({}, {}, {});
    }
}

void SnapshotTimelineController::updateModel() {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session)
        return;

    auto [images, labels, indices] =
        session->snapshotTimelineThumbnails(ThumbnailSize);
    QVector<QPixmap> thumbs;
    thumbs.reserve(images.size());

    for (const auto& img : images) {
        if (img.isNull()) {
            thumbs.append(QPixmap());
        } else {
            thumbs.append(QPixmap::fromImage(img));
        }
    }

    m_model->setThumbnails(thumbs, labels, indices);
    m_model->setSnapshotOnly(session->isSnapshotOnly());
}

void SnapshotTimelineController::onSessionChanged() {
    updateModel();
}

void SnapshotTimelineController::selectSnapshot(int row) {
    if (row == m_currentIndex)
        return;

    m_currentIndex = row;

    if (m_model && row >= 0 && row < m_model->rowCount()) {
        int snapshotIdx =
            m_model->data(m_model->index(row), SnapshotTimelineModel::IndexRole).toInt();
        m_model->clearNewStatus(snapshotIdx);
    }

    emit snapshotSelected(row);
}

void SnapshotTimelineController::clearNewStatus(int dbId) {
    if (m_model) {
        m_model->clearNewStatus(dbId);
    }
}

void SnapshotTimelineController::setSecondarySnapshot(int dbId) {
    if (secondarySnapshotDbId() == dbId)
        return;

    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (session) {
        session->setSecondarySnapshotIndex(dbId);
    }
    emit secondarySnapshotSelected(dbId);
}

int SnapshotTimelineController::secondarySnapshotDbId() const {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    return session ? session->secondarySnapshotIndex() : ImageSession::SecondaryNone;
}

int SnapshotTimelineController::rowForDbId(int dbId) const {
    if (!m_model)
        return -1;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        if (m_model->data(m_model->index(i), SnapshotTimelineModel::IndexRole).toInt() == dbId) {
            return i;
        }
    }
    return -1;
}

void SnapshotTimelineController::updateThumbnail(int index, const QPixmap& pixmap) {
    if (!m_model || index < 0 || index >= m_model->rowCount())
        return;
    m_model->updateThumbnail(index, pixmap);
}
