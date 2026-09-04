#include "core/viewmodel.h"

#include <QDebug>

#include <algorithm>

ViewModel::ViewModel() = default;

void ViewModel::resetState(int width, int height) {
    m_imageWidth = width;
    m_imageHeight = height;
    m_zoom = 1.0f;
    m_fitScale = 0.0f;
    m_zoomRatio = 1.0f;
    m_pan = QPointF(0, 0);
}

void ViewModel::setPan(const QPointF& pan) {
    m_pan = pan;
}

void ViewModel::updateImageSize(int width, int height) {
    m_imageWidth = width;
    m_imageHeight = height;

    if (m_viewportWidth > 0 || m_viewportHeight > 0) {
        float newFit = std::min(static_cast<float>(m_viewportWidth) / m_imageWidth,
                                static_cast<float>(m_viewportHeight) / m_imageHeight);
        m_fitScale = newFit;
    }
}

void ViewModel::setViewportSize(int viewWidth, int viewHeight) {
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

void ViewModel::setPercentage(double pct) {
    if (!hasImage()) {
        qDebug() << "[setPercentage] No image dimensions set";
        return;
    }

    m_zoom = std::clamp(static_cast<float>(pct / 100.0), 0.05f, 64.0f);
    updateZoomRatio();
}

void ViewModel::applyWheelZoom(bool zoomIn, bool ctrlHeld) {
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

void ViewModel::applyPanDelta(int dx, int dy) {
    if (!hasImage()) {
        qDebug() << "[applyPanDelta] No image dimensions set";
        return;
    }

    float invImgW = 1.0f / m_imageWidth;
    float invImgH = 1.0f / m_imageHeight;
    m_pan.rx() -= dx * invImgW / m_zoom;
    m_pan.ry() -= dy * invImgH / m_zoom;
}

void ViewModel::updateZoomRatio() {
    if (!hasImage()) {
        qDebug() << "[updateZoomRatio] No image dimensions set";
        return;
    }

    if (m_fitScale > 0.0f)
        m_zoomRatio = m_zoom / m_fitScale;
}

QPoint ViewModel::screenToPixel(const QPointF& screenPos) const {
    if (!hasImage()) {
        return QPoint(-1, -1);
    }

    // Offset from viewport center
    float screenOffsetX = screenPos.x() - (m_viewportWidth * 0.5f);
    float screenOffsetY = screenPos.y() - (m_viewportHeight * 0.5f);

    // Normalize by current rendered dimensions
    float renderedW = m_zoom * m_imageWidth;
    float renderedH = m_zoom * m_imageHeight;

    float normX = screenOffsetX / std::max(renderedW, 0.001f);
    float normY = screenOffsetY / std::max(renderedH, 0.001f);

    // Apply pan (pan is normalized)
    normX += m_pan.x();
    normY += m_pan.y();

    // Shift to [0, 1] range
    float imgUVX = normX + 0.5f;
    float imgUVY = normY + 0.5f;

    if (imgUVX < 0.0f || imgUVX >= 1.0f || imgUVY < 0.0f || imgUVY >= 1.0f) {
        return QPoint(-1, -1);
    }

    int px = std::clamp(int(imgUVX * m_imageWidth), 0, m_imageWidth - 1);
    int py = std::clamp(int(imgUVY * m_imageHeight), 0, m_imageHeight - 1);

    return QPoint(px, py);
}

QPointF ViewModel::normalizedToScreen(const QPointF& normPos) const {
    if (!hasImage()) {
        return QPointF(-1, -1);
    }

    // Convert absolute normalized [0, 1] to relative normalized [-0.5, 0.5]
    return relativeToScreen(normPos.x() - 0.5f, normPos.y() - 0.5f);
}

QPointF ViewModel::pixelToScreen(const QPoint& pixelPos) const {
    if (!hasImage()) {
        return QPointF(-1, -1);
    }

    // Normalized offset from center in image space
    float relX = (static_cast<float>(pixelPos.x()) / m_imageWidth) - 0.5f;
    float relY = (static_cast<float>(pixelPos.y()) / m_imageHeight) - 0.5f;

    return relativeToScreen(relX, relY);
}

QPointF ViewModel::relativeToScreen(float relX, float relY) const {
    // Apply pan
    relX -= m_pan.x();
    relY -= m_pan.y();

    // Scale by current rendered dimensions
    float renderedW = m_zoom * m_imageWidth;
    float renderedH = m_zoom * m_imageHeight;

    float screenOffsetX = relX * renderedW;
    float screenOffsetY = relY * renderedH;

    // Position relative to viewport center
    float screenX = (m_viewportWidth * 0.5f) + screenOffsetX;
    float screenY = (m_viewportHeight * 0.5f) + screenOffsetY;

    return QPointF(screenX, screenY);
}

void ViewModel::fitToWindow() {
    if (!hasImage()) {
        qDebug() << "[fitToWindow] No image dimensions set";
        return;
    }

    m_zoomRatio = 1.0f;
    m_zoom = m_fitScale;
    m_pan = QPointF(0, 0);
}

void ViewModel::updateZoomForRelativeScaling() {
    if (!hasImage()) {
        qDebug() << "[updateZoomForRelativeScaling] No image dimensions set";
        return;
    }

    m_zoom = m_fitScale * m_zoomRatio;
    m_zoom = std::clamp(m_zoom, 0.05f, 64.0f);
}
