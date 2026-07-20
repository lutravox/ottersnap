#pragma once

#include <QWidget>

#include "ui/snapshottimeline.h"
#include "ui/statusbar.h"
#include "ui/vkimageviewer.h"

/**
 * @brief Groups the thumbnail strip, image viewer, and status bar into a single
 */
class ViewerContainer : public QWidget {
    Q_OBJECT

  public:
    /// @brief Constructs the container
    /// @param parent Optional parent widget.
    explicit ViewerContainer(QWidget *parent = nullptr);

    /// @brief Returns the snapshot timeline for version history.
    SnapshotTimeline *snapshotTimeline() {
        return m_snapshotTimeline;
    }

    /// @brief Returns the image viewer widget.
    ImageViewer *viewer() {
        return m_viewer;
    }

    /// @brief Returns the status bar with zoom controls.
    StatusBar *statusBar() {
        return m_statusBar;
    }

  private:
    SnapshotTimeline *m_snapshotTimeline = nullptr;
    ImageViewer      *m_viewer = nullptr;
    StatusBar        *m_statusBar = nullptr;
};
