#pragma once

#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVector>
#include <QWidget>

#include "controllers/snapshottimelinecontroller.h"
#include "ui/snapshottimelinedelegate.h"

#include <QHBoxLayout>
#include <QListView>

static constexpr int c_startPadding = 6;
static constexpr int c_thumbnailSize = 48;
static constexpr int c_clipAmount = 23;

/**
 * @class SnapshotTimeline
 * @brief A widget providing a horizontal timeline of image snapshots.
 *
 * This component allows users to browse through historical versions of an image,
 * select a snapshot to restore it, or create new snapshots.
 */
class SnapshotTimeline : public QWidget {
    Q_OBJECT

  public:
    explicit SnapshotTimeline(QWidget *parent = nullptr);

    /**
     * @brief Associate a controller with this timeline.
     * @param controller The controller that manages the model and selection state.
     */
    void setController(SnapshotTimelineController *controller);

    /// @brief Sets whether the create snapshot button is enabled.
    /// @param enabled True to enable the button, false to disable it.
    void setCreateButtonEnabled(bool enabled);

    /// @brief Updates the secondary selection highlight in the timeline.
    /// @param id The identity of the snapshot to highlight as secondary.
    void setSecondaryIdentity(const QString& id);

    /// @brief Updates the view to reflect the current selection.
    /// @param newIndex The index of the snapshot that was selected.
    void updateSelection(int newIndex);

    bool eventFilter(QObject *obj, QEvent *event) override;

  protected:
    void resizeEvent(QResizeEvent *event) override;

  signals:
    /**
     * @brief Emitted when a snapshot is selected from the timeline.
     * @param index The index of the selected snapshot.
     */
    void snapshotSelected(int index);

    /**
     * @brief Emitted when a request is made to delete a snapshot.
     * @param uuid The identity of the snapshot to delete.
     */
    void snapshotDeletionRequested(const QUuid& uuid);

    /**
     * @brief Emitted when multiple snapshots are requested to be deleted.
     * @param uuids The identities of the snapshots to delete.
     */
    void multipleSnapshotsDeletionRequested(const QVector<QUuid>& uuids);

    /**
     * @brief Emitted when the user clicks the "+" button to create a snapshot.
     */
    void createSnapshotRequested();

    /// @brief Emitted when a snapshot is set as the secondary/comparison snapshot.
    void secondarySnapshotSelected(const QString& id);

    /// @brief Emitted when the multi-selection set changes.
    void selectionChanged();

  private:
    QUuid uuidAt(const QModelIndex& index) const;

    SnapshotTimelineController *m_controller = nullptr;
    QListView                  *m_listView = nullptr;
    SnapshotTimelineDelegate   *m_delegate = nullptr;

    QPushButton *m_createButton = nullptr;
    QWidget     *m_stripContainer = nullptr;
};
