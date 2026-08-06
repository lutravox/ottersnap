#pragma once

#include <QObject>
#include "controllers/appsettingscontroller.h"
#include "core/imagesession.h"
#include "core/viewer_interfaces.h"

/// @brief Coordinates the viewport state between an ImageSession and a Viewer.
class ViewerController : public QObject {
    Q_OBJECT
  public:
    explicit ViewerController(AppSettingsController *settings, QObject *parent = nullptr);

    /// @brief Set the active session to track.
    void setActiveSession(ImageSession *session);

    /// @brief Set the viewer to control.
    void setViewer(IViewer *viewer);

    /// @brief Sync the session's ViewState to the viewer.
    void syncSessionToViewer();

    /// @brief Sync the viewer's current ViewState back to the session.
    void syncViewerToSession();

    /// @brief Command the viewer to fit the image to the window.
    void fitToWindow();

    /// @brief Set whether images should scale with window on resize.
    void setScaleWithWindowEnabled(bool enabled);

    /// @brief Return whether images should scale with window on resize.
    bool isScaleWithWindowEnabled() const;

    /// @brief Set whether the viewer toolbar should be visible.
    void setToolbarVisible(bool visible);

    /// @brief Return whether the viewer toolbar is currently visible.
    bool isToolbarVisible() const;

    /// @brief Set the secondary snapshot for comparison.
    void setSecondarySnapshot(int index);

    /// @brief Return the index of the secondary snapshot, or -1 if none is set.
    int secondarySnapshotIndex() const;

    /// @brief Check if swapping primary and secondary is currently possible.
    bool canSwap() const;

    /// @brief Swap the primary and secondary snapshots.
    void swapPrimaryAndSecondary();

    /// @brief Notify the controller that the viewport size has changed.
    void handleViewportResize(int width, int height);

  public slots:
    void handleZoomRequested(bool zoomIn, bool ctrlHeld);
    void handlePanRequested(int dx, int dy);
    void setZoomPercentage(double pct);

  signals:
    void grayscaleToggled(bool enabled);
    void mirrorToggled(bool enabled);
    void toolbarVisibilityToggled(bool visible);
    void secondarySnapshotChanged(int index);

  private:
    AppSettingsController *m_settings;
    ImageSession          *m_session = nullptr;
    IViewer               *m_viewer = nullptr;
    QSize                  m_lastViewportSize;
};
