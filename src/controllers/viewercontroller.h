#pragma once

#include <QObject>
#include "controllers/appsettingscontroller.h"
#include "controllers/imagesessioncontroller.h"
#include "core/imagesession.h"
#include "core/viewer_interfaces.h"

/// @brief Coordinates the viewport state between an ImageSession and a Viewer.
class ViewerController : public QObject {
    Q_OBJECT
  public:
    explicit ViewerController(AppSettingsController *settings, QObject *parent = nullptr);

    /// @brief Link the controller to the session coordinator.
    void setSessionController(ImageSessionController *controller);

    /// @brief Set the viewer to control.
    void setViewer(IViewer *viewer);

    /// @brief Request a change of the active session and current snapshot.
    void requestSessionChange(ImageSession *session, int index);

    /// @brief Sync the session's ViewState to the viewer.
    void syncSessionToViewer();

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

    /// @brief Set whether color picking is enabled.
    void setPickingEnabled(bool enabled);

    /// @brief Return whether color picking is enabled.
    bool isPickingEnabled() const;

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

    /// @brief Handle a pan request from the viewer.
    void handlePanRequested(int dx, int dy);

  public slots:
    void handleZoomRequested(bool zoomIn, bool ctrlHeld);
    void setZoomPercentage(double pct);

  private slots:
    void onActiveSessionChanged(ImageSession *session);
    void onSessionImageChanged();

  signals:
    void grayscaleToggled(bool enabled);
    void mirrorToggled(bool enabled);
    void toolbarVisibilityToggled(bool visible);
    void secondarySnapshotChanged(int index);

  private:
    AppSettingsController  *m_settings;
    ImageSessionController *m_sessionController = nullptr;
    IViewer                *m_viewer = nullptr;
    QSize                   m_lastViewportSize;
    bool                    m_pickingEnabled = false;
    QMetaObject::Connection m_effectsConnection;
    QMetaObject::Connection m_imageChangedConnection;
};
