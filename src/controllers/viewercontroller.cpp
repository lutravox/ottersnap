#include <QDebug>
#include "controllers/viewercontroller.h"
#include "controllers/appsettingscontroller.h"
#include "core/vulkancontext.h"
#include "ui/vkimageviewer.h"

ViewerController::ViewerController(AppSettingsController *settings, QObject *parent)
    : QObject(parent), m_settings(settings) {
}

void ViewerController::setSessionController(ImageSessionController *controller) {
    if (m_sessionController == controller)
        return;

    if (m_sessionController) {
        disconnect(m_sessionController,
                   &ImageSessionController::activeSessionChanged,
                   this,
                   &ViewerController::onActiveSessionChanged);
    }

    m_sessionController = controller;

    if (m_sessionController) {
        connect(m_sessionController,
                &ImageSessionController::activeSessionChanged,
                this,
                &ViewerController::onActiveSessionChanged);
    }
}

void ViewerController::onActiveSessionChanged(ImageSession *session) {
    if (!session) {
        if (m_viewer) {
            m_viewer->clear();
        }
        return;
    }

    if (m_viewer) {
        // Sync current viewport size to the new session's view state
        QSize sz = m_viewer->getViewportSize();
        session->viewState().setViewportSize(sz.width(), sz.height());
        session->viewState().updateZoomRatio();

        if (auto *vkViewer = dynamic_cast<VkImageViewer *>(m_viewer)) {
            QObject::disconnect(m_effectsConnection);
            m_effectsConnection = QObject::connect(
                session, &ImageSession::effectsChanged, vkViewer, &VkImageViewer::onEffectsChanged);
        }
    }

    // Connect to imageChanged to handle snapshot selection
    QObject::disconnect(m_imageChangedConnection);
    m_imageChangedConnection = QObject::connect(
        session, &ImageSession::imageChanged, this, &ViewerController::onSessionImageChanged);

    if (session && m_viewer) {
        // Ensure the session is initialized with Vulkan handles before using its reconstructor
        session->setUIReconstructorHandles(VulkanContext::instance().getUIHandles());
        m_viewer->setReconstructor(session->uiReconstructor());

        if (session->isCurrentImageSelected()) {
            m_viewer->setImage(session->diskImage());
        } else if (auto seq = session->getReconstructionSequence()) {
            m_viewer->reconstruct(*seq);
        }
    }

    syncSessionToViewer();

    emit secondarySnapshotChanged(session->secondarySnapshotIndex());
}

void ViewerController::onSessionImageChanged() {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session || !m_viewer)
        return;

    if (session->isCurrentImageSelected()) {
        m_viewer->setImage(session->diskImage());
    } else if (auto seq = session->getReconstructionSequence()) {
        m_viewer->reconstruct(*seq);
    }
}

void ViewerController::setViewer(IViewer *viewer) {
    m_viewer = viewer;
    if (m_viewer) {
        m_viewer->setSessionController(m_sessionController);
    }
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

void ViewerController::requestSessionChange(ImageSession *session, int index) {
    // Check if we are already viewing this session and snapshot
    ImageSession *currentSession = m_sessionController ? m_sessionController->activeSession() : nullptr;
    int currentIdx = currentSession ? currentSession->currentSnapshotIndex() : -1;

    if (session == currentSession && index == currentIdx) {
        return; // No change needed
    }

    if (session != currentSession) {
        if (m_viewer) {
            m_viewer->clear();
        }
        if (m_sessionController) {
            m_sessionController->setActiveSession(session);
        }
    }

    // If the session is now active, we might need to trigger a refresh if the index changed
    // or if it was just set for the first time.
    if (session && m_viewer) {
        // We use the session's index to ensure we are loading the requested version
        session->selectSnapshot(index);
        onSessionImageChanged();
    }

    syncSessionToViewer();
}

void ViewerController::syncSessionToViewer() {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session || !m_viewer)
        return;

    m_viewer->notifyViewStateChanged();
}

void ViewerController::fitToWindow() {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session || !m_viewer)
        return;

    // Ensure we have the current viewport size before fitting
    QSize      sz = m_viewer->getViewportSize();
    ViewState& state = session->viewState();
    state.setViewportSize(sz.width(), sz.height());

    state.fitToWindow();

    m_viewer->notifyViewStateChanged();
}

void ViewerController::setZoomPercentage(double pct) {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session || !m_viewer)
        return;

    ViewState& state = session->viewState();
    state.setPercentage(pct);

    m_viewer->notifyViewStateChanged();
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

void ViewerController::setPickingEnabled(bool enabled) {
    m_pickingEnabled = enabled;
    if (auto *vkViewer = dynamic_cast<VkImageViewer *>(m_viewer)) {
        vkViewer->setPickingEnabled(enabled);
    }
}

bool ViewerController::isPickingEnabled() const {
    return m_pickingEnabled;
}

void ViewerController::setSecondarySnapshot(int index) {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (session) {
        session->setSecondarySnapshotIndex(index);
    }
    emit secondarySnapshotChanged(index);
}

int ViewerController::secondarySnapshotIndex() const {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    return session ? session->secondarySnapshotIndex() : ImageSession::SecondaryNone;
}

bool ViewerController::canSwap() const {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session)
        return false;

    int secondary = session->secondarySnapshotIndex();
    if (secondary == ImageSession::SecondaryNone)
        return false;

    int         primaryRow = session->currentSnapshotIndex();
    int         primaryDbId = -1;
    const auto& snapshots = session->snapshots();

    if (primaryRow >= 0 && primaryRow < static_cast<int>(snapshots.size())) {
        primaryDbId = snapshots[primaryRow].snapshotIndex;
    } else if (primaryRow == static_cast<int>(snapshots.size()) && !session->isSnapshotOnly()) {
        primaryDbId = ImageSession::SecondaryCurrent;
    }

    return primaryDbId != secondary;
}

void ViewerController::swapPrimaryAndSecondary() {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session)
        return;

    int primaryRow = session->currentSnapshotIndex();
    int secondaryDbId = session->secondarySnapshotIndex();

    if (secondaryDbId == ImageSession::SecondaryNone)
        return; // No secondary snapshot selected

    if (secondaryDbId == ImageSession::SecondaryCurrent && session->isSnapshotOnly())
        return; // Secondary cannot be 'current' in snapshot-only mode

    // Determine the row of the secondary snapshot
    int secondaryRow = -1;
    if (secondaryDbId == ImageSession::SecondaryCurrent) {
        // Secondary is the current disk image
        secondaryRow = static_cast<int>(session->snapshots().size());
    } else {
        int relVer = session->getRelativeVersion(secondaryDbId);
        if (relVer != -1) {
            secondaryRow = relVer - 1;
        }
    }

    if (secondaryRow < 0 || secondaryRow > session->maxValidIndex())
        return;

    // Determine the database ID of the current primary
    int         primaryDbId = -1;
    const auto& snapshots = session->snapshots();
    if (primaryRow >= 0 && primaryRow < static_cast<int>(snapshots.size())) {
        primaryDbId = snapshots[primaryRow].snapshotIndex;
    } else if (primaryRow == static_cast<int>(snapshots.size()) && !session->isSnapshotOnly()) {
        primaryDbId = -1; // Primary is the current image
    }

    // Perform swap
    session->selectSnapshot(secondaryRow);
    session->setSecondarySnapshotIndex(primaryDbId);

    emit secondarySnapshotChanged(primaryDbId);
    syncSessionToViewer();
}

void ViewerController::handleViewportResize(int width, int height) {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session || !m_viewer)
        return;

    ViewState& state = session->viewState();

    state.setViewportSize(width, height);

    // Maintain relative zoom if enabled, otherwise maintain absolute zoom
    if (isScaleWithWindowEnabled()) {
        state.updateZoomForRelativeScaling();
    } else {
        state.updateZoomRatio();
    }

    m_viewer->notifyViewStateChanged();
}

void ViewerController::handleZoomRequested(bool zoomIn, bool ctrlHeld) {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session || !m_viewer)
        return;

    ViewState& state = session->viewState();
    state.applyWheelZoom(zoomIn, ctrlHeld);

    m_viewer->notifyViewStateChanged();
}

void ViewerController::handlePanRequested(int dx, int dy) {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session || !m_viewer)
        return;

    ViewState& state = session->viewState();
    state.applyPanDelta(dx, dy);

    m_viewer->notifyViewStateChanged();
}
