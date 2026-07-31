#pragma once

#include <QObject>
#include "core/imagesession.h"
#include "core/viewer_interfaces.h"

/// @brief Coordinates the viewport state between an ImageSession and a Viewer.
class ViewerController : public QObject {
    Q_OBJECT
  public:
    explicit ViewerController(QObject *parent = nullptr);

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

    /// @brief Notify the controller that the viewport size has changed.
    void handleViewportResize(int width, int height);

  public slots:
    void handleZoomRequested(bool zoomIn, bool ctrlHeld);
    void handlePanRequested(int dx, int dy);
    void setZoomPercentage(double pct);

  signals:
    void grayscaleToggled(bool enabled);
    void mirrorToggled(bool enabled);

  private:
    ImageSession *m_session = nullptr;
    IViewer      *m_viewer = nullptr;
    QSize         m_lastViewportSize;
};
