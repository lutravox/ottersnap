#include "ui/snapshotmodel.h"

SnapshotModel::SnapshotModel(QObject *parent) : QAbstractListModel(parent) {
}

int SnapshotModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return m_thumbnails.size();
}

QVariant SnapshotModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_thumbnails.size())
        return QVariant();

    int row = index.row();

    switch (role) {
        case ThumbnailRole:
            return QVariant::fromValue(m_thumbnails[row]);
        case LabelRole:
            return m_labels.isEmpty() ? QVariant(QString("v%1").arg(row + 1))
                                      : QVariant(m_labels[row]);
        case IndexRole:
            return m_indices[row];
        case IsNewRole:
            return m_newSnapshots.contains(m_indices[row]);
        case IsCurrentImageRole:
            return m_indices[row] == -1;
        case Qt::ToolTipRole:
            return m_labels.isEmpty() ? QVariant(QString("v%1").arg(row + 1))
                                      : QVariant(m_labels[row]);
        default:
            return QVariant();
    }
}

void SnapshotModel::setThumbnails(const QVector<QPixmap>& thumbnails,
                                  const QVector<QString>& labels,
                                  const QVector<int>&     indices) {
    beginResetModel();
    m_thumbnails = thumbnails;
    m_labels = labels;
    m_indices = indices;
    // If it's snapshot-only, the model should only contain snapshots.
    // If not, the last item is the current image.
    endResetModel();
}

void SnapshotModel::markSnapshotAsNew(int snapshotIndex) {
    if (snapshotIndex == -1)
        return;
    m_newSnapshots.insert(snapshotIndex);

    // Notify view that the item with this index has changed
    for (int i = 0; i < m_indices.size(); ++i) {
        if (m_indices[i] == snapshotIndex) {
            emit dataChanged(index(i), index(i), {IsNewRole});
            break;
        }
    }
}

void SnapshotModel::clearNewStatus(int snapshotIndex) {
    if (snapshotIndex == -1)
        return;
    if (m_newSnapshots.contains(snapshotIndex)) {
        m_newSnapshots.remove(snapshotIndex);
        for (int i = 0; i < m_indices.size(); ++i) {
            if (m_indices[i] == snapshotIndex) {
                emit dataChanged(index(i), index(i), {IsNewRole});
                break;
            }
        }
    }
}

void SnapshotModel::updateThumbnail(int index, const QPixmap& pixmap) {
    if (index < 0 || index >= m_thumbnails.size())
        return;
    m_thumbnails[index] = pixmap;
    QModelIndex modelIndex = this->index(index);
    emit        dataChanged(modelIndex, modelIndex, {ThumbnailRole});
}
