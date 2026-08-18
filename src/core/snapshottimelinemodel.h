#pragma once

#include <QAbstractListModel>
#include <QPixmap>
#include <QSet>
#include <QVector>

/**
 * @class SnapshotTimelineModel
 * @brief A model that manages the data for the snapshot timeline.
 *
 * This model stores the thumbnails, labels, and indices of snapshots,
 * providing them to the view via custom roles.
 */
class SnapshotTimelineModel : public QAbstractListModel {
    Q_OBJECT

  public:
    /**
     * @brief Custom roles used to retrieve specific data from the model.
     */
    enum SnapshotRoles {
        ThumbnailRole = Qt::UserRole + 1, ///< The thumbnail image of the snapshot.
        LabelRole,                        ///< The display label for the snapshot.
        UuidRole,                         ///< The internal UUID of the snapshot.
        IsNewRole,                        ///< Whether the snapshot was recently created.
        IsCurrentImageRole                ///< Whether the item is the current disk image.
    };;

    explicit SnapshotTimelineModel(QObject *parent = nullptr);

    int      rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    /**
     * @brief Updates the complete set of snapshots in the model.
     * @param thumbnails List of thumbnail images.
     * @param labels List of display labels.
     * @param uuids List of snapshot UUIDs.
     */
    void setThumbnails(const QVector<QPixmap>& thumbnails,
                       const QVector<QString>& labels,
                       const QVector<QUuid>&     uuids);

    /**
     * @brief Marks a specific snapshot as "new" to trigger a visual highlight.
     * @param uuid The UUID of the snapshot to mark.
     */
    void markSnapshotAsNew(const QUuid& uuid);

    /**
     * @brief Removes the "new" status from a snapshot.
     * @param uuid The UUID of the snapshot.
     */
    void clearNewStatus(const QUuid& uuid);

    /**
     * @brief Updates the thumbnail for a specific snapshot.
     * @param index The row index in the model.
     * @param pixmap The new thumbnail image.
     */
    void updateThumbnail(int index, const QPixmap& pixmap);

    /**
     * @brief Find the row index corresponding to a given snapshot UUID string.
     * @param uuid The UUID string or "current".
     * @return The row index, or -1 if not found.
     */
    int rowForUuidString(const QString& uuid) const;

    /// @brief Check if the current session is in snapshot-only mode.
    /// @return True if in snapshot-only mode, false otherwise.
    bool isSnapshotOnly() const {
        return m_isSnapshotOnly;
    }

    /// @brief Set whether the session is in snapshot-only mode.
    /// @param snapshotOnly True to enable snapshot-only mode.
    void setSnapshotOnly(bool snapshotOnly) {
        m_isSnapshotOnly = snapshotOnly;
    }

  private:
    bool             m_isSnapshotOnly = false;
    QVector<QPixmap> m_thumbnails;
    QVector<QString> m_labels;
    QVector<QUuid>   m_uuids;
    QSet<QUuid>      m_newSnapshots;
};
