#pragma once

#include <QAction>
#include <QToolBar>
#include <QWidget>
#include <QLabel>

#include "ui/snapshottimeline.h"
#include "ui/statusbar.h"
#include "ui/vkimageviewer.h"
#include "ui/vkimageviewer.h"
#include "ui/viewertoolbar.h"
#include "ui/colorinfo.h"

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

    /// @brief Updates the secondary snapshot index in the timeline.
    void setSecondarySnapshotIndex(int index) {
        if (m_snapshotTimeline) {
            m_snapshotTimeline->setSecondaryIndex(index);
        }
    }

    /// @brief Toggles the visibility of the snapshot-only indicator.
    void setSnapshotOnlyIndicator(bool visible);

    /// @brief Update the picked color in the color info overlay.
    void setPickedColor(const QColor& color) {
        if (m_colorInfo) {
            m_colorInfo->setPickedColor(color);
        }
    }

    /// @brief Set the visibility of the color info overlay.
    void setColorInfoVisible(bool visible);

  signals:
    void zoomRequested(double pct);
    void fitRequested();

  private:
    SnapshotTimeline *m_snapshotTimeline = nullptr;
    ImageViewer      *m_viewer = nullptr;
    StatusBar        *m_statusBar = nullptr;
    ViewerToolbar    *m_viewerToolbar = nullptr;
    QLabel           *m_snapshotOnlyLabel = nullptr;
    ColorInfo       *m_colorInfo = nullptr;
};
