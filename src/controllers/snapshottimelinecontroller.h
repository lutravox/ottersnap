#pragma once

#include <QObject>
#include <memory>
#include "controllers/imagesessioncontroller.h"
#include "core/snapshottimelinemodel.h"

class ImageSession;

/// @brief Manages the state and model for the snapshot timeline.
/// Acts as the controller between the ImageSession (model) and the SnapshotTimeline (view).
class SnapshotTimelineController : public QObject {
    Q_OBJECT

  public:
    static constexpr int ThumbnailSize = 48;

    explicit SnapshotTimelineController(QObject *parent = nullptr);
    ~SnapshotTimelineController() override = default;

    /// @brief Link the controller to the session coordinator.
    void setSessionController(ImageSessionController *controller);

    /// @brief Provide the model to be used by the view.
    /// @return The underlying snapshot timeline model.
    SnapshotTimelineModel *model() const {
        return m_model.get();
    }

    /// @brief Select a snapshot by its row index in the timeline.
    /// @param row The row index of the snapshot to select.
    void selectSnapshot(int row);

    /// @brief Marks a specific snapshot as "new" to trigger a visual highlight.
    /// @param uuid The UUID of the snapshot to mark.
    void markSnapshotAsNew(const QUuid& uuid);

    /// @brief Clears the "new" status highlight for a snapshot.
    /// @param uuid The UUID of the snapshot.
    void clearNewStatus(const QUuid& uuid);

    /// @brief Set the secondary snapshot by its identity.
    /// @param id The identity of the snapshot to set as secondary.
    void setSecondarySnapshot(const QString& id);

    /// @brief Return the current primary selection index.
    /// @return The index of the currently selected snapshot, or -1 if none.
    int currentSelectedIndex() const {
        if (!m_model) return -1;
        return m_model->rowForUuidString(m_currentUuid);
    }

    /// @brief Return the current secondary selection identity.
    /// @return The identity of the secondary snapshot, or empty if none.
    QString secondarySnapshotId() const;

    /// @brief Get the row index for a given snapshot UUID.
    /// @param uuid The UUID to search for.
    /// @return The corresponding row index in the model, or -1 if not found.
    int rowForUuid(const QUuid& uuid) const;

    /// @brief Update a thumbnail in the model.
    /// @param index The row index in the model.
    /// @param pixmap The new thumbnail image.
    void updateThumbnail(int index, const QPixmap& pixmap);

    /// @brief Update the model based on the current session state.
    void updateModel();

  signals:
    /// @brief Emitted when the primary selection changes.
    void snapshotSelected(int row);
    /// @brief Emitted when the secondary selection changes.
    void secondarySnapshotSelected(const QString& id);
    /// @brief Emitted when a new snapshot creation is requested.
    void createSnapshotRequested();
    /// @brief Emitted when a snapshot deletion is requested.
    void snapshotDeletionRequested(const QUuid& uuid);

  private:
    void onActiveSessionChanged(ImageSession *session);
    void onSessionImageChanged();
    void onSessionChanged();

    std::unique_ptr<SnapshotTimelineModel> m_model;
    ImageSessionController                *m_sessionController = nullptr;
    QString                                m_currentUuid = "";
};
