#pragma once

#include <QAction>
#include <QLabel>
#include <QToolBar>
#include <QWidget>

#include "ui/snapshottimeline.h"
#include "ui/statusbar.h"
#include "ui/viewertoolbar.h"
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

    /// @brief Returns the viewer toolbar for effects.
    ViewerToolbar *toolbar() {
        return m_viewerToolbar;
    }

    /// @brief Sets whether the viewer toolbar should be visible.
    void setToolbarVisible(bool visible) {
        if (m_viewerToolbar) {
            m_viewerToolbar->setVisible(visible);
        }
    }

    /// @brief Updates the secondary snapshot identity in the timeline.
    void setSecondarySnapshotId(const QString& id) {
        if (m_snapshotTimeline) {
            m_snapshotTimeline->setSecondaryIdentity(id);
        }
    }

    /// @brief Toggles the visibility of the snapshot-only indicator.
    void setSnapshotOnlyIndicator(bool visible);

  signals:
    void zoomRequested(double pct);
    void fitRequested();

  private:
    SnapshotTimeline *m_snapshotTimeline = nullptr;
    ImageViewer      *m_viewer = nullptr;
    StatusBar        *m_statusBar = nullptr;
    ViewerToolbar    *m_viewerToolbar = nullptr;
    QLabel           *m_snapshotOnlyLabel = nullptr;
};
