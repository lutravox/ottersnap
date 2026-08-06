#include <QDebug>
#include "controllers/viewercontroller.h"
#include "controllers/appsettingscontroller.h"
#include "ui/vkimageviewer.h"

ViewerController::ViewerController(AppSettingsController *settings, QObject *parent)
    : QObject(parent), m_settings(settings) {
}

void ViewerController::setActiveSession(ImageSession *session) {
    if (m_session) {
        if (auto *vkViewer = dynamic_cast<VkImageViewer *>(m_viewer)) {
            disconnect(m_session,
                       &ImageSession::effectsChanged,
                       vkViewer,
                       &VkImageViewer::onEffectsChanged);
        }
    }

    m_session = session;

    if (m_session && m_viewer) {
        m_viewer->setSession(m_session);

        if (auto *vkViewer = dynamic_cast<VkImageViewer *>(m_viewer)) {
            connect(m_session,
                    &ImageSession::effectsChanged,
                    vkViewer,
                    &VkImageViewer::onEffectsChanged);
        }

        if (m_session->isCurrentImageSelected()) {
            // It's the disk image - use regular upload
            m_viewer->setImage(m_session->diskImage(), false);
        } else if (auto seq = m_session->getReconstructionSequence()) {
            // It's a snapshot - use reconstruction path
            m_viewer->reconstruct(*seq);
        }
    }

    emit secondarySnapshotChanged(m_session ? m_session->secondarySnapshotIndex() : ImageSession::SecondaryNone);
}

void ViewerController::setViewer(IViewer *viewer) {
    m_viewer = viewer;
    if (auto *vkViewer = dynamic_cast<VkImageViewer *>(m_viewer)) {
        connect(
            vkViewer, &VkImageViewer::zoomRequested, this, &ViewerController::handleZoomRequested);
        connect(
            vkViewer, &VkImageViewer::panRequested, this, &ViewerController::handlePanRequested);
        connect(
            vkViewer, &VkImageViewer::grayscaleToggled, this, &ViewerController::grayscaleToggled);
        connect(vkViewer, &VkImageViewer::mirrorToggled, this, &ViewerController::mirrorToggled);

        // Sync initial state
        vkViewer->setScaleWithWindowChecked(isScaleWithWindowEnabled());
    }
}

void ViewerController::syncSessionToViewer() {
    if (!m_session || !m_viewer)
        return;

    m_viewer->setViewState(m_session->viewState());
    m_viewer->update();
}

void ViewerController::syncViewerToSession() {
    if (!m_session || !m_viewer)
        return;
    m_session->viewState() = m_viewer->getViewState();
}

void ViewerController::fitToWindow() {
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

void ViewerController::setZoomPercentage(double pct) {
    if (!m_session || !m_viewer)
        return;

    ViewState& state = m_session->viewState();
    state.setPercentage(pct);

    m_viewer->setViewState(state);
    m_viewer->update();
}

void ViewerController::setScaleWithWindowEnabled(bool enabled) {
    m_settings->setScaleWithWindow(enabled);

    if (auto *vkViewer = dynamic_cast<VkImageViewer *>(m_viewer)) {
        vkViewer->setScaleWithWindowChecked(enabled);
    }
}

bool ViewerController::isScaleWithWindowEnabled() const {
    return m_settings->scaleWithWindow();
}

void ViewerController::setToolbarVisible(bool visible) {
    m_settings->setToolbarVisible(visible);
    emit toolbarVisibilityToggled(visible);
}

bool ViewerController::isToolbarVisible() const {
    return m_settings->toolbarVisible();
}

void ViewerController::setSecondarySnapshot(int index) {
    if (m_session) {
        m_session->setSecondarySnapshotIndex(index);
    }
    emit secondarySnapshotChanged(index);
}

int ViewerController::secondarySnapshotIndex() const {
    return m_session ? m_session->secondarySnapshotIndex() : ImageSession::SecondaryNone;
}

bool ViewerController::canSwap() const {
    if (!m_session)
        return false;

    int secondary = m_session->secondarySnapshotIndex();
    if (secondary == ImageSession::SecondaryNone)
        return false;

    int primaryRow = m_session->currentSnapshotIndex();
    int primaryDbId = -1;
    const auto& snapshots = m_session->snapshots();

    if (primaryRow >= 0 && primaryRow < static_cast<int>(snapshots.size())) {
        primaryDbId = snapshots[primaryRow].snapshotIndex;
    } else if (primaryRow == static_cast<int>(snapshots.size()) && !m_session->isSnapshotOnly()) {
        primaryDbId = ImageSession::SecondaryCurrent;
    }

    return primaryDbId != secondary;
}

void ViewerController::swapPrimaryAndSecondary() {
    if (!m_session)
        return;

    int primaryRow = m_session->currentSnapshotIndex();
    int secondaryDbId = m_session->secondarySnapshotIndex();

    if (secondaryDbId == ImageSession::SecondaryNone)
        return; // No secondary snapshot selected

    if (secondaryDbId == ImageSession::SecondaryCurrent && m_session->isSnapshotOnly())
        return; // Secondary cannot be 'current' in snapshot-only mode

    // Determine the row of the secondary snapshot
    int secondaryRow = -1;
    if (secondaryDbId == ImageSession::SecondaryCurrent) {
        // Secondary is the current disk image
        secondaryRow = static_cast<int>(m_session->snapshots().size());
    } else {
        int relVer = m_session->getRelativeVersion(secondaryDbId);
        if (relVer != -1) {
            secondaryRow = relVer - 1;
        }
    }

    if (secondaryRow < 0 || secondaryRow > m_session->maxValidIndex())
        return;

    // Determine the database ID of the current primary
    int primaryDbId = -1;
    const auto& snapshots = m_session->snapshots();
    if (primaryRow >= 0 && primaryRow < static_cast<int>(snapshots.size())) {
        primaryDbId = snapshots[primaryRow].snapshotIndex;
    } else if (primaryRow == static_cast<int>(snapshots.size()) && !m_session->isSnapshotOnly()) {
        primaryDbId = -1; // Primary is the current image
    }

    // Perform swap
    m_session->selectSnapshot(secondaryRow);
    m_session->setSecondarySnapshotIndex(primaryDbId);

    emit secondarySnapshotChanged(primaryDbId);
    syncSessionToViewer();
}

void ViewerController::handleViewportResize(int width, int height) {
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

void ViewerController::handleZoomRequested(bool zoomIn, bool ctrlHeld) {
    if (!m_session || !m_viewer)
        return;

    ViewState& state = m_session->viewState();
    state.applyWheelZoom(zoomIn, ctrlHeld);

    m_viewer->setViewState(state);
    m_viewer->update();
}

void ViewerController::handlePanRequested(int dx, int dy) {
    if (!m_session || !m_viewer)
        return;

    ViewState& state = m_session->viewState();
    state.applyPanDelta(dx, dy);

    m_viewer->setViewState(state);
    m_viewer->update();
}
