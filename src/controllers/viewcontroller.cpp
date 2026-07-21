#include <QDebug>
#include "controllers/viewcontroller.h"

ViewController::ViewController(QObject *parent) : QObject(parent) {
}

void ViewController::setActiveSession(ImageSession *session) {
    m_session = session;
}

void ViewController::setViewer(IViewer *viewer) {
    m_viewer = viewer;
}

void ViewController::syncSessionToViewer() {
    if (!m_session || !m_viewer)
        return;
    m_viewer->setViewState(m_session->viewState());
}

void ViewController::syncViewerToSession() {
    if (!m_session || !m_viewer)
        return;
    m_session->viewState() = m_viewer->getViewState();
}

void ViewController::fitToWindow() {
    if (!m_session || !m_viewer)
        return;

    // We assume the viewer is already aware of its size,
    // but we ensure the session state is updated.
    ViewState state = m_session->viewState();
    state.fitToWindow();

    m_session->viewState() = state;
    m_viewer->setViewState(state);
}

void ViewController::resetZoom() {
    if (!m_session || !m_viewer)
        return;

    ViewState state = m_session->viewState();
    state.setPercentage(100.0);

    m_session->viewState() = state;
    m_viewer->setViewState(state);
}

void ViewController::handleViewportResize(int width, int height) {
    if (!m_session)
        return;

    // Update the model's understanding of the viewport
    m_session->viewState().setViewportSize(width, height);

    // If we have a viewer, we might need to trigger a re-render or sync
    if (m_viewer) {
        syncSessionToViewer();
    }
}
