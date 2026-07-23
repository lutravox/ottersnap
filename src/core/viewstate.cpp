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
