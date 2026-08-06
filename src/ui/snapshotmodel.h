#pragma once

#include <QAbstractListModel>
#include <QPixmap>
#include <QSet>
#include <QVector>

/**
 * @class SnapshotModel
 * @brief A model that manages the data for the snapshot timeline.
 *
 * This model stores the thumbnails, labels, and indices of snapshots,
 * providing them to the view via custom roles.
 */
class SnapshotModel : public QAbstractListModel {
    Q_OBJECT

  public:
    /**
     * @brief Custom roles used to retrieve specific data from the model.
     */
    enum SnapshotRoles {
        ThumbnailRole = Qt::UserRole + 1, ///< The thumbnail image of the snapshot.
        LabelRole,                        ///< The display label for the snapshot.
        IndexRole,                        ///< The internal index of the snapshot.
        IsNewRole,                        ///< Whether the snapshot was recently created.
        IsCurrentImageRole                ///< Whether the item is the current disk image.
    };

    explicit SnapshotModel(QObject *parent = nullptr);

    int      rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    /**
     * @brief Updates the complete set of snapshots in the model.
     * @param thumbnails List of thumbnail images.
     * @param labels List of display labels.
     * @param indices List of snapshot indices.
     */
    void setThumbnails(const QVector<QPixmap>& thumbnails,
                       const QVector<QString>& labels,
                       const QVector<int>&     indices);

    /**
     * @brief Marks a specific snapshot as "new" to trigger a visual highlight.
     * @param snapshotIndex The index of the snapshot to mark.
     */
    void markSnapshotAsNew(int snapshotIndex);

    /**
     * @brief Removes the "new" status from a snapshot.
     * @param snapshotIndex The index of the snapshot.
     */
    void clearNewStatus(int snapshotIndex);

    /**
     * @brief Updates the thumbnail for a specific snapshot.
     * @param index The row index in the model.
     * @param pixmap The new thumbnail image.
     */
    void updateThumbnail(int index, const QPixmap& pixmap);

    /// @brief Check if the current session is in snapshot-only mode.
    bool isSnapshotOnly() const {
        return m_isSnapshotOnly;
    }

    /// @brief Set whether the session is in snapshot-only mode.
    void setSnapshotOnly(bool snapshotOnly) {
        m_isSnapshotOnly = snapshotOnly;
    }

  private:
    bool             m_isSnapshotOnly = false;
    QVector<QPixmap> m_thumbnails;
    QVector<QString> m_labels;
    QVector<int>     m_indices;
    QSet<int>        m_newSnapshots;
};
