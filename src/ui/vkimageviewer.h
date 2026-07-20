#pragma once

#include <QImage>
#include <QWidget>

#include <QVulkanInstance>
#include <QVulkanWindow>

#include "core/viewstate.h"

class VkImageViewerRenderer;

/// @brief Vulkan-accelerated image viewer with GPU mipmapping.
class VkImageViewer : public QWidget {
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
    void setImage(const QImage& image, bool preserveView = false);

    /// @brief Fit the image to the window, adjusting zoom and pan.
    void fitToWindow();

    /// @brief Reset zoom to 1:1 pixel scale and center the image.
    void resetZoom();

    /// @brief Enable or disable grayscale rendering in the fragment shader.
    void setGrayscale(bool enabled);

    /// @brief Enable or disable horizontal mirroring in the fragment shader.
    void setMirror(bool enabled);

    /// @brief Get the current zoom level as a percentage (100.0 = 1:1).
    /// @return The zoom percentage.
    double ZoomPercentage() const;

    /// @brief Set the zoom level by percentage (100.0 = 1:1).
    /// @param pct The desired zoom percentage.
    void setZoomPercentage(double pct);

    /// @brief Clear the current image from the viewer.
    void clear();

    /// @brief Get the current view state.
    ViewState getViewState() const {
        return m_viewState;
    }

    /// @brief Set the view state.
    /// @param state The new view state.
    void setViewState(const ViewState& state) {
        m_viewState = state;
    }

  signals:
    /// @brief Emitted when the user clicks the image.
    void imageClicked();

    /// @brief Emitted whenever the zoom level changes.
    /// @param percentage The new zoom percentage (100.0 = 1:1).
    void zoomChanged(double percentage);

  protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

  private:
    QVulkanInstance       *m_vulkanInstance = nullptr;
    QVulkanWindow         *m_vulkanWindow = nullptr;
    VkImageViewerRenderer *m_renderer = nullptr;
    QWidget               *m_container = nullptr;

    bool      m_hasImage = false;
    ViewState m_viewState;
    bool      m_isDragging = false;
    QPoint    m_lastMousePos;
};

using ImageViewer = VkImageViewer;
