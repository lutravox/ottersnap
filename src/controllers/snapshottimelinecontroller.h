#pragma once

#include <QObject>
#include <memory>
#include "core/snapshottimelinemodel.h"

class ImageSession;

/// @brief Manages the state and model for the snapshot timeline.
/// Acts as the controller between the ImageSession (model) and the SnapshotTimeline (view).
class SnapshotTimelineController : public QObject {
    Q_OBJECT

  public:
    explicit SnapshotTimelineController(QObject *parent = nullptr);
    ~SnapshotTimelineController() override = default;

    /// @brief Associate a session with this controller.
    /// @param session The image session to manage.
    void setSession(ImageSession *session);

    /// @brief Provide the model to be used by the view.
    /// @return The underlying snapshot timeline model.
    SnapshotTimelineModel* model() const { return m_model.get(); }

    /// @brief Select a snapshot by its row index in the timeline.
    /// @param row The row index of the snapshot to select.
    void selectSnapshot(int row);

    /// @brief Clears the "new" status highlight for a snapshot.
    /// @param dbId The database ID of the snapshot.
    void clearNewStatus(int dbId);

    /// @brief Set the secondary snapshot by its database ID.
    /// @param dbId The database ID of the snapshot to set as secondary.
    void setSecondarySnapshot(int dbId);

    /// @brief Return the current primary selection index.
    /// @return The index of the currently selected snapshot, or -1 if none.
    int currentSelectedIndex() const { return m_currentIndex; }

    /// @brief Return the current secondary selection database ID.
    /// @return The database ID of the secondary snapshot, or ImageSession::SecondaryNone if none.
    int secondarySnapshotDbId() const;

    /// @brief Get the row index for a given snapshot database ID.
    /// @param dbId The database ID to search for.
    /// @return The corresponding row index in the model, or -1 if not found.
    int rowForDbId(int dbId) const;

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
    void secondarySnapshotSelected(int dbId);
    /// @brief Emitted when a new snapshot creation is requested.
    void createSnapshotRequested();
    /// @brief Emitted when a snapshot deletion is requested.
    void snapshotDeletionRequested(int row);

  private:
    void onSessionChanged();

    std::unique_ptr<SnapshotTimelineModel> m_model;
    ImageSession*                  m_session = nullptr;
    int                            m_currentIndex = -1;
};
