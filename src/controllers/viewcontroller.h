#pragma once

#include <QObject>
#include "core/imagesession.h"
#include "core/viewer_interfaces.h"

/// @brief Coordinates the viewport state between an ImageSession and a Viewer.
class ViewController : public QObject {
    Q_OBJECT
  public:
    explicit ViewController(QObject *parent = nullptr);

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

    /// @brief Command the viewer to reset zoom to 1:1.
    void resetZoom();

    /// @brief Notify the controller that the viewport size has changed.
    void handleViewportResize(int width, int height);

  private:
    ImageSession *m_session = nullptr;
    IViewer      *m_viewer = nullptr;
};
