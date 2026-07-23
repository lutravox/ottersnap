#include <QDebug>
#include "controllers/viewcontroller.h"
#include "config/appsettings.h"
#include "ui/vkimageviewer.h"

ViewController::ViewController(QObject *parent) : QObject(parent) {
}

void ViewController::setActiveSession(ImageSession *session) {
    m_session = session;
}

void ViewController::setViewer(IViewer *viewer) {
    m_viewer = viewer;
    if (auto *vkViewer = dynamic_cast<VkImageViewer *>(m_viewer)) {
        connect(
            vkViewer, &VkImageViewer::zoomRequested, this, &ViewController::handleZoomRequested);
        connect(vkViewer, &VkImageViewer::panRequested, this, &ViewController::handlePanRequested);
    }
}

void ViewController::syncSessionToViewer() {
    if (!m_session || !m_viewer)
        return;
    m_viewer->setViewState(m_session->viewState());
    m_viewer->update();
}

void ViewController::syncViewerToSession() {
    if (!m_session || !m_viewer)
        return;
    m_session->viewState() = m_viewer->getViewState();
}

void ViewController::fitToWindow() {
    if (!m_session || !m_viewer)
        return;

    // Ensure we have the current viewport size before fitting
    QSize      sz = m_viewer->getViewportSize();
    ViewState& state = m_session->viewState();
    state.setViewportSize(sz.width(), sz.height());

    state.fitToWindow();

    m_viewer->setViewState(state);
    m_viewer->update();
}

void ViewController::setZoomPercentage(double pct) {
    if (!m_session || !m_viewer)
        return;

    ViewState& state = m_session->viewState();
    state.setPercentage(pct);

    m_viewer->setViewState(state);
    m_viewer->update();
}

void ViewController::setScaleWithWindowEnabled(bool enabled) {
    AppSettings::setScaleWithWindow(enabled);
}

bool ViewController::isScaleWithWindowEnabled() const {
    return AppSettings::scaleWithWindow();
}

void ViewController::handleViewportResize(int width, int height) {
    if (!m_session || !m_viewer)
        return;

    ViewState& state = m_session->viewState();

    state.setViewportSize(width, height);

    // Maintain relative zoom if enabled, otherwise maintain absolute zoom
    if (isScaleWithWindowEnabled()) {
        state.updateZoomForRelativeScaling();
    } else {
        state.updateZoomRatio();
    }

    m_viewer->setViewState(state);
    m_viewer->update();
}

void ViewController::handleZoomRequested(bool zoomIn, bool ctrlHeld) {
    if (!m_session || !m_viewer)
        return;

    ViewState& state = m_session->viewState();
    state.applyWheelZoom(zoomIn, ctrlHeld);

    m_viewer->setViewState(state);
    m_viewer->update();
}

void ViewController::handlePanRequested(int dx, int dy) {
    if (!m_session || !m_viewer)
        return;

    ViewState& state = m_session->viewState();
    state.applyPanDelta(dx, dy);

    m_viewer->setViewState(state);
    m_viewer->update();
}
