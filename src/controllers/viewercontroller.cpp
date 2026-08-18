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

    // Connect to secondarySnapshotChanged to handle secondary selection resets
    QObject::disconnect(m_secondaryChangedConnection);
    m_secondaryChangedConnection = QObject::connect(
        session, &ImageSession::secondarySnapshotChanged, this, [this](const QString& id) {
            emit secondarySnapshotChanged(id);
            emit stateChanged();
        });

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

    emit secondarySnapshotChanged(session->secondarySnapshotId());
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

    syncSessionToViewer();
    emit stateChanged();
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
    QString uuid;
    if (!session) {
        uuid = "";
    } else if (index == session->currentSnapshotIndex()) {
        uuid = session->currentUuid();
    } else if (index == static_cast<int>(session->snapshots().size()) &&
               !session->isSnapshotOnly()) {
        uuid = ImageSession::c_currentId;
    } else if (index >= 0 && index < static_cast<int>(session->snapshots().size())) {
        uuid = session->snapshots()[index].uuid.toString(QUuid::WithoutBraces);
    } else {
        uuid = "";
    }

    requestSessionChange(session, uuid);
}

void ViewerController::requestSessionChange(ImageSession *session, const QUuid& uuid) {
    requestSessionChange(session, uuid.toString(QUuid::WithoutBraces));
}

void ViewerController::requestSessionChange(ImageSession *session, const QString& uuid) {
    ImageSession *currentSession =
        m_sessionController ? m_sessionController->activeSession() : nullptr;

    if (session != currentSession) {
        if (m_viewer) {
            m_viewer->clear();
        }
        if (m_sessionController) {
            m_sessionController->setActiveSession(session);
        }
    }

    if (session && m_viewer) {
        session->selectSnapshot(uuid);
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

void ViewerController::setSecondarySnapshot(const QString& id) {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (session) {
        session->setSecondarySnapshotId(id);
    }
    emit secondarySnapshotChanged(id);
    emit stateChanged();
}

QString ViewerController::secondarySnapshotId() const {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session)
        return QString();
    return session->secondarySnapshotId();
}

bool ViewerController::canSwap() const {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session)
        return false;

    QString primaryId = session->currentUuid();
    QString secondaryId = secondarySnapshotId();

    return !primaryId.isEmpty() && !secondaryId.isEmpty() && primaryId != secondaryId;
}

void ViewerController::swapPrimaryAndSecondary() {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session)
        return;

    QString primaryId = session->currentUuid();
    QString secondaryId = secondarySnapshotId();

    if (secondaryId.isEmpty())
        return; // No secondary snapshot selected

    if (primaryId == secondaryId)
        return;

    // Perform swap
    session->selectSnapshot(secondaryId);
    session->setSecondarySnapshotId(primaryId);

    emit secondarySnapshotChanged(primaryId);
    syncSessionToViewer();
    emit stateChanged();
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
