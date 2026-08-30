#pragma once

#include <QImage>
#include <QMouseEvent>
#include <QWidget>

#include <QVulkanInstance>
#include <QVulkanWindow>

#include "core/effects_interfaces.h"
#include "core/viewer_interfaces.h"

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

    /// @brief Set the position of the cluster indicator.
    /// @param pos The normalized position (0.0 to 1.0).
    void setIndicatorPos(const QPointF& pos) {
        m_indicatorPos = pos;
        // updateIndicatorPosition() is removed as it's now handled by ViewerState/ClusterIndicator
    }

    /// @brief Get the current indicator position.
    /// @return The normalized position.
    QPointF indicatorPos() const {
        return m_indicatorPos;
    }

    /// @brief Set the indicator rendering state.
    /// @param pos The screen position.
    /// @param color The color to render.
    /// @param visible Whether the indicator should be visible.
    void setIndicator(QPoint pos, QColor color, bool visible);

    /// @brief Update the rendering parameters (zoom, pan, etc.).
    void setRenderParams(const RenderParams& params) override;

    /// @brief Display an image in the viewer.
    /// @param image The image to display.
    void setImage(const QImage& image) override;

    /// @brief Reconstructs a snapshot from a base image and a series of deltas.
    /// @return True if reconstruction was successful, false otherwise.
    bool reconstruct(const ReconstructionSequence& seq) override;

    /// @brief Return the current zoom level as a percentage (100.0 = 1:1).
    double zoomPercentage() const override {
        return m_params.zoom * 100.0;
    }

    /// @brief Get the viewport size.
    /// @return The size of the container widget.
    QSize getViewportSize() const override {
        return m_container ? m_container->size() : QSize(0, 0);
    }

    /// @brief Notify the viewer that the viewport model has changed.
    void notifyViewModelChanged() override;

    /// @brief Clear the current image from the viewer.
    void clear() override;

    /// @brief Set the reconstructor to be used for GPU acceleration.
    /// @param reconstructor The reconstructor instance to use.
    void setReconstructor(std::shared_ptr<VkSnapshotReconstructor> reconstructor) override;

    /// @brief Get the current state of the rendering pipeline.
    RenderState renderState() const override;

    /// @brief Maps a point relative to the image viewport to global screen coordinates.
    QPoint mapViewportToGlobal(const QPoint& pos) const {
        return m_container ? m_container->mapToGlobal(pos) : QPoint();
    }

    /// @brief Set the checked state of the 'Scale with Window' menu option.
    /// @param checked True if scaling with window should be enabled.
    void setScaleWithWindowChecked(bool checked);

    /// @brief Set whether color picking is enabled.
    void setPickingEnabled(bool enabled);

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

    /// @brief Emitted when a color picking is requested at the given screen position.
    void colorPickRequested(QPointF screenPos);

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
    void showEvent(QShowEvent *event) override;

    /// @brief Handles the resize event.
    void resizeEvent(QResizeEvent *event) override;

    /// @brief Filters events for the embedded Vulkan window.
    /// @param obj The object the event is delivered to.
    /// @param event The event to filter.
    /// @return True if the event was handled, false otherwise.
    bool eventFilter(QObject *obj, QEvent *event) override;

    /// @brief Handles the paint event for QSS support.
    void paintEvent(QPaintEvent *event) override;

  public slots:
    /// @brief Handles changes in effects state.
    void onEffectsChanged();

  private:
    QVulkanWindow         *m_vulkanWindow = nullptr;
    VkImageViewerRenderer *m_renderer = nullptr;
    QWidget               *m_container = nullptr;

    bool         m_hasImage = false;
    bool         m_isDragging = false;
    QPoint       m_lastMousePos;
    bool         m_scaleWithWindow = false;
    bool         m_pickingEnabled = false;
    RenderParams m_params;
    QPointF      m_indicatorPos;
    uint32_t     m_generation = 0;
};

using ImageViewer = VkImageViewer;
