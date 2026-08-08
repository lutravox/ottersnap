#include "core/viewstate.h"

#include <QDebug>

#include <algorithm>

ViewState::ViewState() = default;

void ViewState::resetState(int width, int height) {
    m_imageWidth = width;
    m_imageHeight = height;
    m_zoom = 1.0f;
    m_fitScale = 0.0f;
    m_zoomRatio = 1.0f;
    m_pan = QPointF(0, 0);
}

void ViewState::updateImageSize(int width, int height) {
    m_imageWidth = width;
    m_imageHeight = height;
}

void ViewState::setViewportSize(int viewWidth, int viewHeight) {
    if (!hasImage()) {
        qDebug() << "[setViewportSize] No image dimensions set";
        return;
    }

    m_viewportWidth = viewWidth;
    m_viewportHeight = viewHeight;

    float newFit = std::min(static_cast<float>(viewWidth) / m_imageWidth,
                            static_cast<float>(viewHeight) / m_imageHeight);

    m_fitScale = newFit;
}

void ViewState::setPercentage(double pct) {
    if (!hasImage()) {
        qDebug() << "[setPercentage] No image dimensions set";
        return;
    }

    m_zoom = std::clamp(static_cast<float>(pct / 100.0), 0.05f, 64.0f);
    updateZoomRatio();
}

void ViewState::applyWheelZoom(bool zoomIn, bool ctrlHeld) {
    if (!hasImage()) {
        qDebug() << "[applyWheelZoom] No image dimensions set";
        return;
    }

    float factor = zoomIn ? 1.1f : 0.9f;
    if (ctrlHeld)
        factor = factor * factor;

    m_zoom *= factor;
    m_zoom = std::clamp(m_zoom, 0.05f, 64.0f);
    updateZoomRatio();
}

void ViewState::applyPanDelta(int dx, int dy) {
    if (!hasImage()) {
        qDebug() << "[applyPanDelta] No image dimensions set";
        return;
    }

    float invImgW = 1.0f / m_imageWidth;
    float invImgH = 1.0f / m_imageHeight;
    m_pan.rx() -= dx * invImgW / m_zoom;
    m_pan.ry() -= dy * invImgH / m_zoom;
}

void ViewState::updateZoomRatio() {
    if (!hasImage()) {
        qDebug() << "[updateZoomRatio] No image dimensions set";
        return;
    }

    if (m_fitScale > 0.0f)
        m_zoomRatio = m_zoom / m_fitScale;
}

QPoint ViewState::screenToPixel(const QPointF& screenPos) const {
    if (!hasImage()) {
        return QPoint(-1, -1);
    }

    float screenX = screenPos.x();
    float screenY = screenPos.y();

    float fitImgW = m_fitScale * m_imageWidth;
    float fitImgH = m_fitScale * m_imageHeight;
    float originX = (m_viewportWidth - fitImgW) * 0.5f;
    float originY = (m_viewportHeight - fitImgH) * 0.5f;

    // Normalise to [0,1] image-space UV, then shift to [-0.5, 0.5].
    float imgUVX = (screenX - originX) / std::max(fitImgW, 0.001f) - 0.5f;
    float imgUVY = (screenY - originY) / std::max(fitImgH, 0.001f) - 0.5f;

    // UV zoom multiplier: fitScale / zoomLevel.
    float zoomFactor = m_fitScale / std::max(m_zoom, 0.001f);
    imgUVX *= zoomFactor;
    imgUVY *= zoomFactor;

    // Apply pan
    imgUVX += m_pan.x();
    imgUVY += m_pan.y();

    // Shift back to [0,1]
    imgUVX += 0.5f;
    imgUVY += 0.5f;

    if (imgUVX < 0.0f || imgUVX >= 1.0f || imgUVY < 0.0f || imgUVY >= 1.0f) {
        return QPoint(-1, -1);
    }

    int px = static_cast<int>(imgUVX * m_imageWidth);
    int py = static_cast<int>(imgUVY * m_imageHeight);

    return QPoint(px, py);
}

void ViewState::fitToWindow() {
    if (!hasImage()) {
        qDebug() << "[fitToWindow] No image dimensions set";
        return;
    }

    m_zoomRatio = 1.0f;
    m_zoom = m_fitScale;
    m_pan = QPointF(0, 0);
}

void ViewState::updateZoomForRelativeScaling() {
    if (!hasImage()) {
        qDebug() << "[updateZoomForRelativeScaling] No image dimensions set";
        return;
    }

    m_zoom = m_fitScale * m_zoomRatio;
    m_zoom = std::clamp(m_zoom, 0.05f, 64.0f);
}
