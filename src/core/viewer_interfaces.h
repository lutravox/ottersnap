#pragma once

#include <QImage>
#include "core/imagesession.h"

/// @brief Interface for an image viewer.
class IViewer {
  public:
    virtual ~IViewer() = default;

    /// @brief Set the session associated with this viewer.
    virtual void setSession(ImageSession *session) = 0;

    /// @brief Set the image to be displayed.
    virtual void setImage(const QImage& image, bool resetState) = 0;

    /// @brief Reconstructs a snapshot from a base image and a series of deltas.
    virtual void reconstruct(const ReconstructionSequence& seq) = 0;

    /// @brief Update the viewer's viewport state.
    virtual void setViewState(const ViewState& state) = 0;

    /// @brief Trigger a redraw of the viewer.
    virtual void update() = 0;

    /// @brief Retrieve the current viewport size.
    virtual QSize getViewportSize() const = 0;

    /// @brief Retrieve the current viewport state from the viewer.
    virtual ViewState getViewState() const = 0;

    /// @brief Set the reconstructor to be used for GPU acceleration.
    virtual void setReconstructor(std::shared_ptr<VkSnapshotReconstructor> reconstructor) = 0;

    /// @brief Return the current zoom percentage for UI display.
    virtual double zoomPercentage() const = 0;
};
