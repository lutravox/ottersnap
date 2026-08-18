#pragma once

#include <QPointF>

/// @brief Tracks zoom, pan, and fit state for an image viewer.
/// Zoom model:
///   - `zoom` = an absolute scale factor (1.0 = 1:1 pixels, 100%).
///   - `fitScale` = viewport-to-image ratio at the current viewport size.
///   - `zoomRatio` = zoom / fitScale (1.0 = perfectly fitted).  Stays
///     constant across window resizes so the user's manual zoom offset is preserved.
class ViewModel {
  public:
    /// @brief Construct a ViewModel with default values.
    ViewModel();

    /// @brief Set the image dimensions and reset zoom/pan to defaults.
    /// @param width Image width in pixels.
    /// @param height Image height in pixels.
    void resetState(int width, int height);

    /// @brief Update the image dimensions.
    /// @param width Image width in pixels.
    /// @param height Image height in pixels.
    void updateImageSize(int width, int height);

    /// @brief Update the viewport size and recompute fit scale.
    /// Keeps zoomRatio constant across resizes.
    /// @param viewWidth Viewport width in pixels.
    /// @param viewHeight Viewport height in pixels.
    void setViewportSize(int viewWidth, int viewHeight);

    /// @brief Return true if an image size has been set.
    bool hasImage() const {
        return m_imageWidth > 0 && m_imageHeight > 0;
    }
    /// @brief Return the image width in pixels.
    int imageWidth() const {
        return m_imageWidth;
    }
    /// @brief Return the image height in pixels.
    int imageHeight() const {
        return m_imageHeight;
    }

    /// @brief Return the current viewport width in pixels.
    int viewportWidth() const {
        return m_viewportWidth;
    }

    /// @brief Return the current viewport height in pixels.
    int viewportHeight() const {
        return m_viewportHeight;
    }

    /// @brief Return the current absolute zoom scale factor.
    float zoom() const {
        return m_zoom;
    }
    /// @brief Return the current pan offset.
    QPointF pan() const {
        return m_pan;
    }
    /// @brief Return the fit scale (viewport-to-image ratio).
    float fitScale() const {
        return m_fitScale;
    }
    /// @brief Return the zoom ratio (1.0 = fitted).
    float zoomRatio() const {
        return m_zoomRatio;
    }
    /// @brief Return the zoom as a percentage (100% = 1:1).
    double percentage() const {
        return m_zoom * 100.0;
    }

    /// @brief Set zoom to an absolute percentage (clamped 5%–6400%).
    /// @param pct The zoom percentage.
    void setPercentage(double pct);

    /// @brief Apply a wheel zoom step.
    /// @param zoomIn True to zoom in, false to zoom out.
    /// @param ctrlHeld True if Ctrl is held (squared factor).
    void applyWheelZoom(bool zoomIn, bool ctrlHeld = false);

    /// @brief Apply a screen-pixel drag delta to the pan offset.
    /// @param dx Horizontal drag in screen pixels.
    /// @param dy Vertical drag in screen pixels.
    void applyPanDelta(int dx, int dy);

    /// @brief Fit image to window: zoomRatio -> 1.0, pan -> (0,0).
    /// Requires setViewportSize() to have been called.
    void fitToWindow();

    /// @brief Update absolute zoom to maintain the current zoom ratio.
    /// Used for relative scaling during viewport resizes.
    void updateZoomForRelativeScaling();

    /// @brief Update the zoom ratio to match the current absolute zoom.
    /// Used for absolute scaling during viewport resizes.
    void updateZoomRatio();

    /// @brief Maps a normalized image position (0.0 to 1.0) to a viewport-relative screen position.
    QPointF normalizedToScreen(const QPointF& normPos) const;

    /// @brief Maps a screen position to a pixel coordinate in the image.
    /// @param screenPos The screen position in logical pixels.
    /// @return The pixel coordinate in the image, or QPoint(-1, -1) if out of bounds.
    QPoint screenToPixel(const QPointF& screenPos) const;

    /// @brief Maps a pixel coordinate in the image to a screen position.
    /// @param pixelPos The pixel coordinate in the image.
    /// @return The screen position in logical pixels.
    QPointF pixelToScreen(const QPoint& pixelPos) const;

  private:
    /// @brief Maps a relative normalized position ([-0.5, 0.5]) to a screen position.
    QPointF relativeToScreen(float relX, float relY) const;

    float   m_zoom = 1.0f;
    float   m_fitScale = 0.0f;
    float   m_zoomRatio = 1.0f;
    QPointF m_pan;
    int     m_imageWidth = 0;
    int     m_imageHeight = 0;
    int     m_viewportWidth = 0;
    int     m_viewportHeight = 0;
};
