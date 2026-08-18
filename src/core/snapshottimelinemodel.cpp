#include "core/snapshottimelinemodel.h"

#include <QUuid>

SnapshotTimelineModel::SnapshotTimelineModel(QObject *parent) : QAbstractListModel(parent) {
}

int SnapshotTimelineModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return m_thumbnails.size();
}

QVariant SnapshotTimelineModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_thumbnails.size())
        return QVariant();

    int row = index.row();

    switch (role) {
        case ThumbnailRole:
            return QVariant::fromValue(m_thumbnails[row]);
        case LabelRole:
            return m_labels.isEmpty() ? QVariant(QString("v%1").arg(row + 1))
                                      : QVariant(m_labels[row]);
        case UuidRole:
            return m_uuids[row];
        case IsNewRole:
            return m_newSnapshots.contains(m_uuids[row]);
        case IsCurrentImageRole:
            return m_uuids[row].isNull() || m_uuids[row] == QUuid();
        case Qt::ToolTipRole:
            return m_labels.isEmpty() ? QVariant(QString("v%1").arg(row + 1))
                                      : QVariant(m_labels[row]);
        default:
            return QVariant();
    }
}

void SnapshotTimelineModel::setThumbnails(const QVector<QPixmap>& thumbnails,
                                          const QVector<QString>& labels,
                                          const QVector<QUuid>&   uuids) {
    beginResetModel();
    m_thumbnails = thumbnails;
    m_labels = labels;
    m_uuids = uuids;
    // If it's snapshot-only, the model should only contain snapshots.
    // If not, the last item is the current image.
    endResetModel();
}

void SnapshotTimelineModel::markSnapshotAsNew(const QUuid& uuid) {
    if (uuid.isNull())
        return;
    m_newSnapshots.insert(uuid);

    // Notify view that the item with this index has changed
    for (int i = 0; i < m_uuids.size(); ++i) {
        if (m_uuids[i] == uuid) {
            emit dataChanged(index(i), index(i), {IsNewRole});
            break;
        }
    }
}

void SnapshotTimelineModel::clearNewStatus(const QUuid& uuid) {
    if (uuid.isNull())
        return;
    if (m_newSnapshots.contains(uuid)) {
        m_newSnapshots.remove(uuid);
        for (int i = 0; i < m_uuids.size(); ++i) {
            if (m_uuids[i] == uuid) {
                emit dataChanged(index(i), index(i), {IsNewRole});
                break;
            }
        }
    }
}

void SnapshotTimelineModel::updateThumbnail(int index, const QPixmap& pixmap) {
    if (index < 0 || index >= m_thumbnails.size())
        return;
    m_thumbnails[index] = pixmap;
    QModelIndex modelIndex = this->index(index);
    emit        dataChanged(modelIndex, modelIndex, {ThumbnailRole});
}

int SnapshotTimelineModel::rowForUuidString(const QString& uuid) const {
    if (uuid == "current") {
        return m_uuids.isEmpty() ? -1 : m_uuids.size() - 1;
    }

    QUuid quuid = QUuid::fromString(uuid);
    if (quuid.isNull())
        return -1;

    for (int i = 0; i < m_uuids.size(); ++i) {
        if (m_uuids[i] == quuid) {
            return i;
        }
    }
    return -1;
}
