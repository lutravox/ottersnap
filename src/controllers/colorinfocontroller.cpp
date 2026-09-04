#include <QDebug>
#include <QTimer>
#include "controllers/colorinfocontroller.h"
#include "core/clusterindicatormodel.h"
#include "core/viewmodel.h"
#include "ui/vkimageviewer.h"

ColorInfoController::ColorInfoController(QObject *parent) : QObject(parent) {
    m_indicatorModel = new ClusterIndicatorModel(this);
    m_colorInfoModel = new ColorInfoModel(this);
    connect(m_colorInfoModel,
            &ColorInfoModel::clusterSelected,
            this,
            &ColorInfoController::onClusterSelected);
}

void ColorInfoController::setSessionController(ImageSessionController *controller) {
    if (m_sessionController == controller)
        return;

    if (m_sessionController) {
        disconnect(m_sessionController,
                   &ImageSessionController::activeSessionEffectsChanged,
                   this,
                   &ColorInfoController::onActiveSessionEffectsChanged);
        disconnect(m_sessionController,
                   &ImageSessionController::activeSessionChanged,
                   this,
                   &ColorInfoController::onActiveSessionChanged);
        disconnect(m_sessionController,
                   &ImageSessionController::activeSessionColorClustersChanged,
                   this,
                   &ColorInfoController::onSessionColorClustersChanged);
    }

    m_sessionController = controller;

    if (m_sessionController) {
        connect(m_sessionController,
                &ImageSessionController::activeSessionEffectsChanged,
                this,
                &ColorInfoController::onActiveSessionEffectsChanged);
        connect(m_sessionController,
                &ImageSessionController::activeSessionChanged,
                this,
                &ColorInfoController::onActiveSessionChanged);
        connect(m_sessionController,
                &ImageSessionController::activeSessionColorClustersChanged,
                this,
                &ColorInfoController::onSessionColorClustersChanged);
    }
}

void ColorInfoController::onActiveSessionChanged(ImageSession *session) {
    QObject::disconnect(m_sessionImageConnection);
    resetClusterSelection();
    if (session) {
        m_sessionImageConnection = QObject::connect(session,
                                                    &ImageSession::imageChanged,
                                                    this,
                                                    &ColorInfoController::resetClusterSelection);
        onSessionColorClustersChanged();
    }
}

void ColorInfoController::resetClusterSelection() {
    m_clusterSelected = false;
    m_colorInfoModel->resetSelection();
    updateIndicatorPosition();
}

void ColorInfoController::setViewerModel(ViewerModel *state) {
    if (m_viewerState == state)
        return;

    m_viewerState = state;

    if (m_viewerState && m_viewerState->viewer()) {
        ImageViewer *viewer = m_viewerState->viewer();
        connect(
            viewer, &ImageViewer::zoomChanged, this, &ColorInfoController::updateIndicatorPosition);
        connect(viewer,
                &ImageViewer::viewportResized,
                this,
                &ColorInfoController::updateIndicatorPosition);
        connect(viewer,
                &ImageViewer::panRequested,
                this,
                &ColorInfoController::updateIndicatorPosition);
    }
}

void ColorInfoController::updateIndicatorPosition() {
    // Use a timer to ensure the viewer has finished updating its state
    QTimer::singleShot(0, this, &ColorInfoController::updateIndicatorPos);
}

void ColorInfoController::setVisible(bool visible) {
    if (!m_viewerState) {
        return;
    }

    m_visible = visible;
    m_colorInfoModel->setVisible(visible);

    if (visible) {
        // Synchronize the current cluster distribution when the overlay shows.
        onSessionColorClustersChanged();
    }

    updateIndicatorPosition();
}

void ColorInfoController::setPickedColor(const QColor& color) {
    m_colorInfoModel->setPickedColor(color);
    m_colorInfoModel->resetSelection();
    m_clusterSelected = false;
    updateIndicatorPos();
}

void ColorInfoController::setClusters(const QList<ColorAnalyzer::ColorCluster>& clusters) {
    m_colorInfoModel->setClusters(clusters);
}

void ColorInfoController::onSessionColorClustersChanged() {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (session) {
        m_colorInfoModel->setClusters(session->colorClusters());
    }
}

void ColorInfoController::onActiveSessionEffectsChanged() {
    updateIndicatorPosition();
}

void ColorInfoController::onClusterSelected(const QVariantMap& data) {
    if (!m_viewerState)
        return;

    QPointF normPos = data["samplePos"].toPointF();
    QColor  color = data["color"].value<QColor>();

    if (m_clusterSelected && m_currentIndicatorPos == normPos && m_currentClusterColor == color) {
        resetClusterSelection();
        return;
    }

    m_currentClusterColor = color;

    m_colorInfoModel->setPickedColor(m_currentClusterColor);

    emit colorSelected(m_currentClusterColor);

    m_currentIndicatorPos = normPos;
    m_clusterSelected = true;
    updateIndicatorPos();
}

void ColorInfoController::updateIndicatorPos() {
    if (!m_viewerState)
        return;

    ImageViewer *viewer = m_viewerState->viewer();
    if (!viewer)
        return;

    ClusterIndicatorModel *indicator = m_indicatorModel;
    if (!indicator)
        return;

    if (!m_visible || !m_clusterSelected) {
        indicator->setVisible(false);
        return;
    }

    QPointF       pos = m_currentIndicatorPos;
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (session && session->mirrorEnabled()) {
        pos.setX(1.0f - pos.x());
    }

    if (!session)
        return;

    QPointF screenPos = session->viewModel().normalizedToScreen(pos);

    if (screenPos.x() < 0 || screenPos.y() < 0) {
        indicator->setVisible(false);
    } else {
        indicator->setPosition(screenPos);
        indicator->setColor(m_currentClusterColor);
        indicator->setVisible(true);
    }
}

ColorInfoController::~ColorInfoController() {
    // The viewer's QML scene binds to the models owned by this controller.
    // Destroy the scene before the base destructor destroys the models, so
    // no binding re-evaluates against a destroyed model during teardown.
    if (m_viewerState) {
        m_viewerState->viewer()->destroyQmlScene();
    }
}
