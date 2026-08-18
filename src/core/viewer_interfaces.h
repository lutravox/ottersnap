#pragma once

#include <QImage>
#include <QSize>
#include <memory>
#include "core/vulkan_types.h"

class VkSnapshotReconstructor;

/// @brief Possible states of the viewer's rendering pipeline.
enum class RenderState {
    Empty,           ///< No image is loaded.
    Loading,         ///< Base image is being uploaded to GPU.
    Reconstructing,  ///< Snapshot deltas are being applied.
    Ready           ///< Image is fully reconstructed and ready to draw.
};

/// @brief Interface for an image viewer.
class IViewer {
  public:
    virtual ~IViewer() = default;

    /// @brief Get the current state of the rendering pipeline.
    virtual RenderState renderState() const = 0;

    /// @brief Set the session coordinator associated with this viewer.
    virtual void setSessionController(class ImageSessionController *controller) = 0;

    /// @brief Set the image to be displayed.
    virtual void setImage(const QImage& image) = 0;

    /// @brief Reconstructs a snapshot from a base image and a series of deltas.
    virtual void reconstruct(const ReconstructionSequence& seq) = 0;

    /// @brief Update the viewer's viewport state.
    virtual void notifyViewModelChanged() = 0;

    /// @brief Trigger a redraw of the viewer.
    virtual void update() = 0;

    /// @brief Clear the current image from the viewer.
    virtual void clear() = 0;

    /// @brief Retrieve the current viewport size.
    virtual QSize getViewportSize() const = 0;

    /// @brief Set the reconstructor to be used for GPU acceleration.
    virtual void setReconstructor(std::shared_ptr<VkSnapshotReconstructor> reconstructor) = 0;

    /// @brief Return the current zoom percentage for UI display.
    virtual double zoomPercentage() const = 0;
};
