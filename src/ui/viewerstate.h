#pragma once

#include <QAction>
#include <QToolBar>
#include <QWidget>

#include "ui/snapshottimeline.h"
#include "ui/statusbar.h"
#include "ui/vkimageviewer.h"

/**
 * @brief Image viewer stage.
 */
class ViewerState : public QWidget {
    Q_OBJECT

  public:
    /// @brief Constructs the state
    /// @param parent Optional parent widget.
    explicit ViewerState(QWidget *parent = nullptr);

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

    /// @brief Toggles the visibility of the snapshot-only indicator.
    void setSnapshotOnlyIndicator(bool visible) {
        if (m_snapshotTimeline) {
            m_snapshotTimeline->setSnapshotOnlyIndicator(visible);
        }
    }

  signals:
    void zoomRequested(double pct);
    void fitRequested();

  private:
    SnapshotTimeline *m_snapshotTimeline = nullptr;
    ImageViewer      *m_viewer = nullptr;
    StatusBar        *m_statusBar = nullptr;
};
