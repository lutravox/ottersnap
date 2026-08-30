#pragma once

#include <QImage>
#include <QSize>
#include <memory>
#include "core/vulkan_types.h"

class VkSnapshotReconstructor;

/// @brief Possible states of the viewer's rendering pipeline.
enum class RenderState {
    Empty,          ///< No image is loaded.
    Loading,        ///< Base image is being uploaded to GPU.
    Reconstructing, ///< Snapshot deltas are being applied.
    Ready           ///< Image is fully reconstructed and ready to draw.
};

/// @brief Parameters for rendering a frame.
struct RenderParams {
    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
    float imageWidth = 0.0f;
    float imageHeight = 0.0f;
    float panX = 0.0f;
    float panY = 0.0f;
    float zoom = 1.0f;
    float fitScale = 0.0f;
    bool  grayscale = false;
    bool  mirror = false;
};

/// @brief Interface for an image viewer.
class IViewer {
  public:
    virtual ~IViewer() = default;

    /// @brief Get the current state of the rendering pipeline.
    virtual RenderState renderState() const = 0;

    /// @brief Update the rendering parameters (zoom, pan, etc.).
    virtual void setRenderParams(const RenderParams& params) = 0;

    /// @brief Set the image to be displayed.
    virtual void setImage(const QImage& image) = 0;

    /// @brief Reconstructs a snapshot from a base image and a series of deltas.
    /// @return True if reconstruction was successful, false otherwise.
    virtual bool reconstruct(const ReconstructionSequence& seq) = 0;

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
