#include <QDebug>
#include "controllers/viewercontroller.h"
#include "config/appsettings.h"
#include "controllers/appsettingscontroller.h"
#include "core/vulkancontext.h"
#include "ui/vkimageviewer.h"

ViewerController::ViewerController(AppSettingsController *settings, QObject *parent)
    : QObject(parent), m_settings(settings) {
    connect(&VulkanContext::instance(),
            &VulkanContext::deviceChanged,
            this,
            &ViewerController::onDeviceChanged);
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
        // Sync current viewport size to the new session's view state.
        QSize sz = m_viewer->getViewportSize();
        if (!sz.isEmpty()) {
            session->viewModel().setViewportSize(sz.width(), sz.height());
        }

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

    // Connect to effectsChanged to sync rendering params
    QObject::disconnect(m_effectsChangedConnection);
    m_effectsChangedConnection = QObject::connect(
        session, &ImageSession::effectsChanged, this, &ViewerController::syncSessionToViewer);

    // Connect to secondarySnapshotChanged to handle secondary selection resets
    QObject::disconnect(m_secondaryChangedConnection);
    m_secondaryChangedConnection = QObject::connect(
        session, &ImageSession::secondarySnapshotChanged, this, [this](const QString& id) {
            emit secondarySnapshotChanged(id);
            emit stateChanged();
        });

    updateViewerState();
    syncSessionToViewer();

    emit secondarySnapshotChanged(session->secondarySnapshotId());
}

void ViewerController::updateViewerState() {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session || !m_viewer)
        return;

    // Prepare session for rendering
    VulkanHandles handles = VulkanContext::instance().getUIHandles();
    bool          gpuSupported =
        (handles.device != VK_NULL_HANDLE) && !AppSettings::forceCpuReconstruction();

    if (gpuSupported) {
        session->setVkReconstructorHandles(handles);
        m_viewer->setReconstructor(session->vulkanUiReconstructor());
    }

    if (session->isCurrentImageSelected()) {
        m_cpuReconstructionActive = false;
        m_viewer->setImage(session->diskImage());
        return;
    }

    const std::optional<ReconstructionSequence> seq = session->getReconstructionSequence();
    if (!seq)
        return;

    if (gpuSupported && m_viewer->reconstruct(*seq)) {
        m_cpuReconstructionActive = false;
        return;
    }

    if (!m_cpuReconstructionActive) {
        m_cpuReconstructionActive = true;
        if (AppSettings::forceCpuReconstruction()) {
            qDebug() << "[ViewerController] CPU reconstruction forced by setting";
        } else if (gpuSupported) {
            qInfo() << "[ViewerController] GPU reconstruction failed, falling back to CPU";
        } else if (VulkanContext::instance().getUtilityDevice() != VK_NULL_HANDLE) {
            qInfo() << "[ViewerController] GPU UI context not yet initialized, using CPU temporary "
                       "reconstruction";
        } else {
            qInfo() << "[ViewerController] GPU unavailable, using CPU reconstruction";
        }
    }

    if (!performCpuReconstruction(session, *seq)) {
        qCritical() << "[ViewerController] CPU reconstruction failed!";
    }
}

bool ViewerController::performCpuReconstruction(ImageSession *session,
                                                const ReconstructionSequence& seq) {
    QImage cpuImage = session->cpuReconstructor()->reconstructToImage(seq);
    if (!cpuImage.isNull()) {
        m_viewer->setImage(cpuImage);
        return true;
    }
    return false;
}

void ViewerController::onSessionImageChanged() {
    updateViewerState();
    syncSessionToViewer();
    emit stateChanged();
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
        connect(vkViewer,
                &VkImageViewer::colorPickRequested,
                this,
                &ViewerController::handleColorPickRequested);

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

    // Push a read-only snapshot of the session state to the viewer
    RenderParams     params;
    const ViewModel& vm = session->viewModel();
    params.viewportWidth = static_cast<float>(m_viewer->getViewportSize().width());
    params.viewportHeight = static_cast<float>(m_viewer->getViewportSize().height());
    params.imageWidth = static_cast<float>(vm.imageWidth());
    params.imageHeight = static_cast<float>(vm.imageHeight());
    params.panX = vm.pan().x();
    params.panY = vm.pan().y();
    params.zoom = vm.zoom();
    params.fitScale = vm.fitScale();
    params.grayscale = session->grayscaleEnabled();
    params.mirror = session->mirrorEnabled();

    m_viewer->setRenderParams(params);
    m_viewer->notifyViewModelChanged();
}

void ViewerController::fitToWindow() {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    fitToWindow(session);
}

void ViewerController::fitToWindow(ImageSession *session) {
    if (!session || !m_viewer)
        return;

    // Ensure we have the current viewport size before fitting
    QSize      sz = m_viewer->getViewportSize();
    ViewModel& state = session->viewModel();
    state.setViewportSize(sz.width(), sz.height());

    state.fitToWindow();

    if (m_sessionController && session == m_sessionController->activeSession()) {
        syncSessionToViewer();
        m_viewer->notifyViewModelChanged();
    }
}

void ViewerController::setZoomPercentage(double pct) {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session || !m_viewer)
        return;

    ViewModel& state = session->viewModel();
    state.setPercentage(pct);

    syncSessionToViewer();
    m_viewer->notifyViewModelChanged();
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

void ViewerController::handleColorPickRequested(QPointF screenPos) {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session)
        return;

    QPoint pixelPos = session->viewModel().screenToPixel(screenPos);
    if (pixelPos == QPoint(-1, -1))
        return;

    QRgb color = 0;
    if (session->isCurrentImageSelected()) {
        QImage img = session->diskImage();
        if (!img.isNull()) {
            color = img.pixel(pixelPos.x(), pixelPos.y());
        }
    } else if (ISnapshotReconstructor *recon = session->uiReconstructor()) {
        color = recon->samplePixel(pixelPos.x(), pixelPos.y());
    }

    if (color != 0) {
        emit colorPicked(QColor::fromRgba(color));
    }
}

void ViewerController::handleViewportResize(int width, int height) {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session || !m_viewer)
        return;

    ViewModel& state = session->viewModel();

    state.setViewportSize(width, height);

    // Maintain relative zoom if enabled, otherwise maintain absolute zoom
    if (isScaleWithWindowEnabled()) {
        state.updateZoomForRelativeScaling();
    } else {
        state.updateZoomRatio();
    }

    syncSessionToViewer();
    m_viewer->notifyViewModelChanged();
}

void ViewerController::handleZoomRequested(bool zoomIn, bool ctrlHeld) {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session || !m_viewer)
        return;

    ViewModel& state = session->viewModel();
    state.applyWheelZoom(zoomIn, ctrlHeld);

    syncSessionToViewer();
    m_viewer->notifyViewModelChanged();
}

void ViewerController::handlePanRequested(int dx, int dy) {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (!session || !m_viewer)
        return;

    ViewModel& state = session->viewModel();
    state.applyPanDelta(dx, dy);

    syncSessionToViewer();
    m_viewer->notifyViewModelChanged();
}

void ViewerController::onDeviceChanged() {
    updateViewerState();
    syncSessionToViewer();
}
