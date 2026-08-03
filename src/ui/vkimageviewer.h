#pragma once

#include <QImage>
#include <QMouseEvent>
#include <QWidget>

#include <QVulkanInstance>
#include <QVulkanWindow>

#include "core/effects_interfaces.h"
#include "core/viewer_interfaces.h"
#include "core/viewstate.h"

class VkImageViewerRenderer;

/// @brief Vulkan-accelerated image viewer with GPU mipmapping.
class VkImageViewer : public QWidget, public IEffectsRenderer, public IViewer {
    Q_OBJECT

  public:
    /// @brief Construct the Vulkan image viewer.
    /// @param parent Optional parent widget.
    explicit VkImageViewer(QWidget *parent = nullptr);

    /// @brief Destructor. Cleans up Vulkan resources.
    ~VkImageViewer() override;

    /// @brief Display an image in the viewer.
    /// @param image The image to display.
    /// @param preserveView If true, keep the current zoom and pan. If false, reset to fit.
    void setImage(const QImage& image, bool preserveView = false) override;

    /// @brief Reconstructs a snapshot from a base image and a series of deltas.
    void reconstruct(const ReconstructionSequence& seq) override;

    /// @brief Get the current zoom level as a percentage (100.0 = 1:1).
    /// @return The zoom percentage.
    double zoomPercentage() const override {
        return m_currentViewState.percentage();
    }

    /// @brief Get the viewport size.
    /// @return The size of the container widget.
    QSize getViewportSize() const override {
        return m_container ? m_container->size() : QSize(0, 0);
    }

    /// @brief Clear the current image from the viewer.
    void clear();

    /// @brief Get the current view state.
    /// @return The current ViewState containing zoom and pan information.
    ViewState getViewState() const override {
        return m_currentViewState;
    }

    /// @brief Set the view state.
    /// @param state The new view state.
    void setViewState(const ViewState& state) override;

    /// @brief Set the reconstructor to be used for GPU acceleration.
    /// @param reconstructor The reconstructor instance to use.
    void setReconstructor(std::shared_ptr<VkSnapshotReconstructor> reconstructor) override;

    /// @brief Set the session associated with this viewer.
    /// @param session The image session to associate.
    void setSession(class ImageSession *session);

    /// @brief Set the checked state of the 'Scale with Window' menu option.
    /// @param checked True if scaling with window should be enabled.
    void setScaleWithWindowChecked(bool checked);

    /// @brief Trigger a redraw of the viewer.
    void update() override {
        QWidget::update();
        if (m_vulkanWindow) {
            m_vulkanWindow->requestUpdate();
        }
    }

  signals:
    /// @brief Emitted when the user clicks the image.
    void imageClicked();

    /// @brief Emitted whenever the viewport size changes.
    /// @param width The new width of the viewport.
    /// @param height The new height of the viewport.
    void viewportResized(int width, int height);

    /// @brief Emitted when the user requests a zoom change via the wheel.
    /// @param zoomIn True if zooming in, false if zooming out.
    /// @param ctrlHeld True if the Ctrl modifier was held during the request.
    void zoomRequested(bool zoomIn, bool ctrlHeld);

    /// @brief Emitted when the user requests a pan change via dragging.
    /// @param dx The change in X position.
    /// @param dy The change in Y position.
    void panRequested(int dx, int dy);

    /// @brief Emitted whenever the zoom level changes.
    /// @param percentage The new zoom percentage.
    void zoomChanged(double percentage);

    /// @brief Emitted when the user requests a view reset.
    void resetViewRequested();

    /// @brief Emitted when the user requests actual size (100%).
    void actualSizeRequested();

    /// @brief Emitted when the user requests zoom in.
    void zoomInRequested();

    /// @brief Emitted when the user requests zoom out.
    void zoomOutRequested();

    /// @brief Emitted when scale with window is toggled.
    /// @param enabled True if scaling with window is now enabled.
    void scaleWithWindowToggled(bool enabled);

    /// @brief Emitted when grayscale mode is toggled.
    /// @param enabled True if grayscale mode is now enabled.
    void grayscaleToggled(bool enabled);

    /// @brief Emitted when mirroring mode is toggled.
    /// @param enabled True if mirroring mode is now enabled.
    void mirrorToggled(bool enabled);

    /// @brief Emitted when all effects are reset.
    void resetEffectsRequested();

    /// @brief Emitted when the user requests to open an image.
    /// @param path The file path of the image to open.
    void imageOpenRequested(const QString& path);

  protected:
    /// @brief Handles the show event.
    void showEvent(QShowEvent *event) override;

    /// @brief Handles the resize event.
    void resizeEvent(QResizeEvent *event) override;

    /// @brief Filters events for the embedded Vulkan window.
    /// @param obj The object the event is delivered to.
    /// @param event The event to filter.
    /// @return True if the event was handled, false otherwise.
    bool eventFilter(QObject *obj, QEvent *event) override;

  public slots:
    /// @brief Handles changes in effects state.
    void onEffectsChanged();

  private:
    QVulkanWindow         *m_vulkanWindow = nullptr;
    VkImageViewerRenderer *m_renderer = nullptr;
    QWidget               *m_container = nullptr;

    bool      m_hasImage = false;
    ViewState m_currentViewState;
    bool      m_isDragging = false;
    QPoint    m_lastMousePos;
    bool      m_scaleWithWindow = false;
};

using ImageViewer = VkImageViewer;
