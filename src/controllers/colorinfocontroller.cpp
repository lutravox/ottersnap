#include <QDebug>
#include <QTimer>
#include "controllers/colorinfocontroller.h"
#include "core/viewstate.h"
#include "ui/vkimageviewer.h"

ColorInfoController::ColorInfoController(QObject *parent) : QObject(parent) {
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
    resetClusterSelection();
    if (session) {
        onSessionColorClustersChanged();
    }
}

void ColorInfoController::resetClusterSelection() {
    m_clusterSelected = false;
    if (m_colorInfo) {
        m_colorInfo->resetSelection();
    }
    updateIndicatorPosition();
}

void ColorInfoController::setViewerState(ViewerState *state) {
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
    if (!m_viewerState)
        return;

    m_visible = visible;

    if (!m_colorInfo) {
        m_colorInfo = new ColorInfo(m_viewerState->viewer());
        connect(m_colorInfo,
                &ColorInfo::clusterSelected,
                this,
                &ColorInfoController::onClusterSelected);

        // Synchronize clusters immediately upon creation
        onSessionColorClustersChanged();
    }

    m_colorInfo->setVisibleState(visible);
    updateIndicatorPosition();
}

void ColorInfoController::setPickedColor(const QColor& color) {
    if (m_colorInfo) {
        m_colorInfo->setPickedColor(color);
        m_colorInfo->resetSelection();
    }
    m_clusterSelected = false;
    updateIndicatorPos();
}

void ColorInfoController::setClusters(const QList<ColorAnalyzer::ColorCluster>& clusters) {
    if (m_colorInfo) {
        m_colorInfo->setClusters(clusters);
    }
}

void ColorInfoController::onSessionColorClustersChanged() {
    ImageSession *session = m_sessionController ? m_sessionController->activeSession() : nullptr;
    if (session && m_colorInfo) {
        m_colorInfo->setClusters(session->colorClusters());
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

    if (m_colorInfo) {
        m_colorInfo->setPickedColor(m_currentClusterColor);
    }

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

    if (!m_visible || !m_clusterSelected) {
        viewer->setIndicator(QPoint(), Qt::transparent, false);
        return;
    }

    QPointF pos = m_currentIndicatorPos;
    if (m_sessionController && m_sessionController->isMirrorEnabled()) {
        pos.setX(1.0f - pos.x());
    }

    QPointF screenPos = viewer->getViewState().normalizedToScreen(pos);

    if (screenPos.x() < 0 || screenPos.y() < 0) {
        viewer->setIndicator(QPoint(), Qt::transparent, false);
    } else {
        QPoint roundedScreenPos(qRound(screenPos.x()), qRound(screenPos.y()));

        viewer->setIndicator(roundedScreenPos, m_currentClusterColor, true);
    }
}
