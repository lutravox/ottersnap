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
    /// @param controller The session coordinator to use.
    void setSessionController(ImageSessionController *controller);

    /// @brief Set the viewer to control.
    /// @param viewer The viewer implementation to control.
    void setViewer(IViewer *viewer);

    /// @brief Request a change of the active session and current snapshot.
    /// @param session The session to activate.
    /// @param index The positional index of the snapshot to select.
    void requestSessionChange(ImageSession *session, int index);
    /// @brief Request a change of the active session and current snapshot.
    /// @param session The session to activate.
    /// @param uuid The UUID of the snapshot to select.
    void requestSessionChange(ImageSession *session, const QUuid& uuid);
    /// @brief Request a change of the active session and current snapshot.
    /// @param session The session to activate.
    /// @param uuid The identity string (UUID or "current") of the snapshot to select.
    void requestSessionChange(ImageSession *session, const QString& uuid);

    /// @brief Sync the session's ViewState to the viewer.
    void syncSessionToViewer();

    /// @brief Command the viewer to fit the image to the window.
    void fitToWindow();

    /// @brief Set whether images should scale with window on resize.
    /// @param enabled True to enable scaling.
    void setScaleWithWindowEnabled(bool enabled);

    /// @brief Return whether images should scale with window on resize.
    /// @return True if scaling is enabled.
    bool isScaleWithWindowEnabled() const;

    /// @brief Set whether the viewer toolbar should be visible.
    /// @param visible True to show the toolbar.
    void setToolbarVisible(bool visible);

    /// @brief Return whether the viewer toolbar is currently visible.
    /// @return True if the toolbar is visible.
    bool isToolbarVisible() const;

    /// @brief Set whether color picking is enabled.
    /// @param enabled True to enable picking.
    void setPickingEnabled(bool enabled);

    /// @brief Return whether color picking is enabled.
    /// @return True if picking is enabled.
    bool isPickingEnabled() const;

    /// @brief Set the secondary snapshot for comparison.
    /// @param id The identity string of the snapshot to set as secondary.
    void setSecondarySnapshot(const QString& id);

    /// @brief Return the identity of the secondary snapshot, or empty string if none is set.
    /// @return The identity string of the secondary snapshot.
    QString secondarySnapshotId() const;

    /// @brief Check if swapping primary and secondary is currently possible.
    /// @return True if both a primary and secondary are selected and they are different.
    bool canSwap() const;

    /// @brief Swap the primary and secondary snapshots.
    void swapPrimaryAndSecondary();

    /// @brief Notify the controller that the viewport size has changed.
    /// @param width New viewport width.
    /// @param height New viewport height.
    void handleViewportResize(int width, int height);

    /// @brief Handle a pan request from the viewer.
    /// @param dx Horizontal pan delta.
    /// @param dy Vertical pan delta.
    void handlePanRequested(int dx, int dy);

  public slots:
    /// @brief Handle a zoom request.
    /// @param zoomIn True to zoom in, false to zoom out.
    /// @param ctrlHeld True if the Ctrl key was held during the request.
    void handleZoomRequested(bool zoomIn, bool ctrlHeld);

    /// @brief Set the viewer zoom to a specific percentage.
    /// @param pct The zoom percentage (e.g., 100.0 for 1:1).
    void setZoomPercentage(double pct);

  private slots:
    void onActiveSessionChanged(ImageSession *session);
    void onSessionImageChanged();

  signals:
    void grayscaleToggled(bool enabled);
    void mirrorToggled(bool enabled);
    void toolbarVisibilityToggled(bool visible);
    void secondarySnapshotChanged(const QString& id);
    void stateChanged();

  private:
    AppSettingsController  *m_settings;
    ImageSessionController *m_sessionController = nullptr;
    IViewer                *m_viewer = nullptr;
    QSize                   m_lastViewportSize;
    bool                    m_pickingEnabled = false;
    QMetaObject::Connection m_effectsConnection;
    QMetaObject::Connection m_imageChangedConnection;
    QMetaObject::Connection m_secondaryChangedConnection;
};
