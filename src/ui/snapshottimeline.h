#pragma once

#include <QPixmap>
#include <QPushButton>
#include <QVector>
#include <QWidget>

#include "ui/snapshotmodel.h"
#include "ui/snapshottimelinedelagate.h"

#include <QHBoxLayout>
#include <QListView>

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
     * @brief Updates the timeline with a new set of thumbnails and labels.
     * @param thumbnails List of thumbnail images to display.
     * @param labels List of labels for each thumbnail.
     * @param indices The internal indices mapping thumbnails to snapshots.
     */
    void setThumbnails(const QVector<QPixmap>& thumbnails,
                       const QVector<QString>& labels,
                       const QVector<int>&     indices);

    /**
     * @brief Marks a snapshot as new to highlight it in the timeline.
     * @param snapshotIndex The index of the snapshot to mark.
     */
    void markSnapshotAsNew(int snapshotIndex);

    /**
     * @brief Sets the currently selected snapshot index.
     * @param index The index to select.
     */
    void setSelectedIndex(int index);

    /**
     * @brief Updates a single thumbnail in the timeline.
     * @param index The row index in the model.
     * @param pixmap The new thumbnail image.
     */
    void updateThumbnail(int index, const QPixmap& pixmap);

    /**
     * @brief Checks if the timeline is currently empty.
     */
    bool isEmpty() const;

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
     * @brief Emitted when the user clicks the "+" button to create a snapshot.
     */
    void createSnapshotRequested();

    /**
     * @brief Emitted when a request is made to delete a snapshot.
     * @param index The index of the snapshot to delete.
     */
    void snapshotDeletionRequested(int index);

  private:
    void updateSelection(int oldIndex, int newIndex);

    int                       m_currentIndex = -1;
    SnapshotModel            *m_model = nullptr;
    QListView                *m_listView = nullptr;
    SnapshotTimelineDelegate *m_delegate = nullptr;

    QPushButton *m_createButton = nullptr;
};
