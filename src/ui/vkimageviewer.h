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

    QSize getViewportSize() const override {
        return m_container ? m_container->size() : QSize(0, 0);
    }

    /// @brief Clear the current image from the viewer.
    void clear();

    /// @brief Get the current view state.
    ViewState getViewState() const override {
        return m_currentViewState;
    }

    /// @brief Set the view state.
    /// @param state The new view state.
    void setViewState(const ViewState& state) override;

    /// @brief Set the reconstructor to be used for GPU acceleration.
    void setReconstructor(std::shared_ptr<VkSnapshotReconstructor> reconstructor) override;

    /// @brief Set the session associated with this viewer.
    void setSession(class ImageSession *session);

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
    void viewportResized(int width, int height);

    /// @brief Emitted when the user requests a zoom change via the wheel.
    void zoomRequested(bool zoomIn, bool ctrlHeld);

    /// @brief Emitted when the user requests a pan change via dragging.
    void panRequested(int dx, int dy);

    /// @brief Emitted whenever the zoom level changes.
    void zoomChanged(double percentage);

    /// @brief Emitted when grayscale mode is toggled.
    void grayscaleToggled(bool enabled);

    /// @brief Emitted when mirroring mode is toggled.
    void mirrorToggled(bool enabled);
    void imageOpenRequested(const QString& path);

  protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

  public slots:
    void onEffectsChanged();

  private:
    QVulkanWindow         *m_vulkanWindow = nullptr;
    VkImageViewerRenderer *m_renderer = nullptr;
    QWidget               *m_container = nullptr;

    bool      m_hasImage = false;
    ViewState m_currentViewState;
    bool      m_isDragging = false;
    QPoint    m_lastMousePos;
};

using ImageViewer = VkImageViewer;
