#include "controllers/snapshottimelinecontroller.h"
#include "core/imagesession.h"
#include "core/snapshottimelinemodel.h"
#include <QPixmap>

SnapshotTimelineController::SnapshotTimelineController(QObject *parent)
    : QObject(parent) {
    m_model = std::make_unique<SnapshotTimelineModel>();
}

void SnapshotTimelineController::setSession(ImageSession *session) {
    if (m_session == session)
        return;

    if (m_session) {
        disconnect(m_session, &ImageSession::snapshotsChanged, this, &SnapshotTimelineController::onSessionChanged);
    }

    m_session = session;
    if (m_session) {
        connect(m_session, &ImageSession::snapshotsChanged, this, &SnapshotTimelineController::onSessionChanged);
        updateModel();
    } else {
        m_model->setThumbnails({}, {}, {});
    }
}

void SnapshotTimelineController::updateModel() {
    if (!m_session)
        return;

    auto [images, labels, indices] = m_session->snapshotTimelineThumbnails(48); // Using 48 as default size
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
    m_model->setSnapshotOnly(m_session->isSnapshotOnly());
}

void SnapshotTimelineController::onSessionChanged() {
    updateModel();
}

void SnapshotTimelineController::selectSnapshot(int row) {
    if (row == m_currentIndex)
        return;

    m_currentIndex = row;

    if (m_model && row >= 0 && row < m_model->rowCount()) {
        int snapshotIdx = m_model->data(m_model->index(row), SnapshotTimelineModel::IndexRole).toInt();
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

    if (m_session) {
        m_session->setSecondarySnapshotIndex(dbId);
    }
    emit secondarySnapshotSelected(dbId);
}

int SnapshotTimelineController::secondarySnapshotDbId() const {
    return m_session ? m_session->secondarySnapshotIndex() : ImageSession::SecondaryNone;
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

