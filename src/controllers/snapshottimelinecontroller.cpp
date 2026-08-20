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
        m_currentUuid = session->currentUuid();
        emit snapshotSelected(currentSelectedIndex());
        connect(session,
                &ImageSession::imageChanged,
                this,
                &SnapshotTimelineController::onSessionImageChanged);
    } else {
        m_model->setThumbnails({}, {}, {});
        m_currentUuid = "";
        emit snapshotSelected(-1);
    }
}

void SnapshotTimelineController::onSessionImageChanged() {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (session) {
        QString uuid = session->currentUuid();
        if (uuid != m_currentUuid) {
            m_currentUuid = uuid;
            emit snapshotSelected(currentSelectedIndex());
        }
    }
}

void SnapshotTimelineController::updateModel() {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session)
        return;

    auto [images, labels, indices] = session->snapshotTimelineThumbnails(ThumbnailSize);
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
    emit snapshotSelected(currentSelectedIndex());
}

void SnapshotTimelineController::selectSnapshot(int row) {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session)
        return;

    if (row == currentSelectedIndex())
        return;

    if (!m_model)
        return;

    QUuid uuid = m_model->data(m_model->index(row), SnapshotTimelineModel::UuidRole).value<QUuid>();
    QString identity =
        uuid.isNull() ? ImageSession::c_currentId : uuid.toString(QUuid::WithoutBraces);

    // Clear "new" status if it's a snapshot
    if (!uuid.isNull()) {
        clearNewStatus(uuid);
    }

    m_sessionController->selectSnapshot(identity);
}

void SnapshotTimelineController::markSnapshotAsNew(const QUuid& uuid) {
    if (m_model) {
        m_model->markSnapshotAsNew(uuid);
    }
}

void SnapshotTimelineController::clearNewStatus(const QUuid& uuid) {
    if (m_model) {
        m_model->clearNewStatus(uuid);
    }
}

void SnapshotTimelineController::setSecondarySnapshot(const QString& id) {
    if (secondarySnapshotId() == id)
        return;

    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (session) {
        session->setSecondarySnapshotId(id);
    }
    emit secondarySnapshotSelected(id);
}

QString SnapshotTimelineController::secondarySnapshotId() const {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session)
        return QString();
    return session->secondarySnapshotId();
}

int SnapshotTimelineController::rowForUuid(const QUuid& uuid) const {
    if (!m_model)
        return -1;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        if (m_model->data(m_model->index(i), SnapshotTimelineModel::UuidRole).value<QUuid>() ==
            uuid) {
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

void SnapshotTimelineController::toggleSelection(int row) {
    if (!m_model || row < 0 || row >= m_model->rowCount())
        return;

    QModelIndex idx = m_model->index(row);
    QUuid uuid = m_model->data(idx, SnapshotTimelineModel::UuidRole).value<QUuid>();

    if (uuid.isNull()) {
        return; // Protect disk image from multi-selection
    }

    if (m_selectedIndices.contains(row)) {
        m_selectedIndices.remove(row);
    } else {
        m_selectedIndices.insert(row);
    }

    emit selectionChanged();
}

void SnapshotTimelineController::selectRange(int start, int end) {
    if (!m_model || start < 0 || end < 0)
        return;

    int active = currentSelectedIndex();
    if (active < 0)
        return;

    int low = qMin(active, start);
    int high = qMax(active, end);

    m_selectedIndices.clear();
    for (int i = low; i <= high && i < m_model->rowCount(); ++i) {
        QModelIndex idx = m_model->index(i);
        QUuid uuid = m_model->data(idx, SnapshotTimelineModel::UuidRole).value<QUuid>();
        if (!uuid.isNull()) {
            m_selectedIndices.insert(i);
        }
    }
    emit selectionChanged();
}

void SnapshotTimelineController::clearSelection() {
    if (m_selectedIndices.isEmpty())
        return;
    m_selectedIndices.clear();
    emit selectionChanged();
}

QVector<QUuid> SnapshotTimelineController::selectedUuids() const {
    QVector<QUuid> uuids;
    if (!m_model) return uuids;

    for (int row : m_selectedIndices) {
        QModelIndex idx = m_model->index(row);
        QUuid uuid = m_model->data(idx, SnapshotTimelineModel::UuidRole).value<QUuid>();
        if (!uuid.isNull()) {
            uuids.append(uuid);
        }
    }
    return uuids;
}
