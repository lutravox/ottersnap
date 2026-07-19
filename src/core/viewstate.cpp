#include "core/viewstate.h"

#include <QDebug>

#include <algorithm>

ViewState::ViewState() = default;

void ViewState::setImageSize(int width, int height) {
    m_imageWidth = width;
    m_imageHeight = height;
    m_zoom = 1.0f;
    m_fitScale = 0.0f;
    m_zoomRatio = 1.0f;
    m_pan = QPointF(0, 0);
}

void ViewState::setViewportSize(int viewWidth, int viewHeight) {
    if (!hasImage()) {
        qDebug() << "[setViewportSize] no image dimensions set";
        return;
    }

    float newFit = std::min(static_cast<float>(viewWidth) / m_imageWidth,
                            static_cast<float>(viewHeight) / m_imageHeight);

    updateZoomRatio();
    m_fitScale = newFit;
    m_zoom = m_fitScale * m_zoomRatio;
}

void ViewState::setPercentage(double pct) {
    if (!hasImage()) {
        qDebug() << "[setPercentage] no image dimensions set";
        return;
    }

    m_zoom = std::clamp(static_cast<float>(pct / 100.0), 0.05f, 64.0f);
    updateZoomRatio();
}

void ViewState::applyWheelZoom(bool zoomIn, bool ctrlHeld) {
    if (!hasImage()) {
        qDebug() << "[applyWheelZoom] no image dimensions set";
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
        qDebug() << "[applyPanDelta] no image dimensions set";
        return;
    }

    float invImgW = 1.0f / m_imageWidth;
    float invImgH = 1.0f / m_imageHeight;
    m_pan.rx() -= dx * invImgW / m_zoom;
    m_pan.ry() -= dy * invImgH / m_zoom;
}

void ViewState::updateZoomRatio() {
    if (!hasImage()) {
        qDebug() << "[updateZoomRatio] no image dimensions set";
        return;
    }

    if (m_fitScale > 0.0f)
        m_zoomRatio = m_zoom / m_fitScale;
}

void ViewState::fitToWindow() {
    if (!hasImage()) {
        qDebug() << "[fitToWindow] no image dimensions set";
        return;
    }

    m_zoomRatio = 1.0f;
    m_zoom = m_fitScale;
    m_pan = QPointF(0, 0);
}
