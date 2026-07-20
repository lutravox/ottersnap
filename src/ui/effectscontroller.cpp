#include "ui/effectscontroller.h"
#include "ui/imagetab.h"
#include "ui/vkimageviewer.h"

EffectsController::EffectsController(QObject *parent) : QObject(parent) {
}

void EffectsController::setup(VkImageViewer *viewer,
                              QAction       *grayscaleAction,
                              QAction       *mirrorAction) {
    m_viewer = viewer;
    m_grayscaleAction = grayscaleAction;
    m_mirrorAction = mirrorAction;

    if (m_viewer) {
        connect(m_viewer, &VkImageViewer::grayscaleToggled, this, &EffectsController::setGrayscale);
        connect(m_viewer, &VkImageViewer::mirrorToggled, this, &EffectsController::setMirror);
    }
}

void EffectsController::setTargetTab(ImageTab *tab) {
    m_currentTab = tab;
    syncFromTab();
}

void EffectsController::toggleGrayscale() {
    bool current = false;
    if (m_currentTab) {
        current = m_currentTab->grayscaleEnabled();
    } else if (m_grayscaleAction) {
        current = m_grayscaleAction->isChecked();
    }
    setGrayscale(!current);
}

void EffectsController::toggleMirror() {
    bool current = false;
    if (m_currentTab) {
        current = m_currentTab->mirrorEnabled();
    } else if (m_mirrorAction) {
        current = m_mirrorAction->isChecked();
    }
    setMirror(!current);
}

void EffectsController::setGrayscale(bool enabled) {
    if (m_currentTab) {
        m_currentTab->setGrayscale(enabled);
    }
    if (m_grayscaleAction) {
        m_grayscaleAction->setChecked(enabled);
    }
    if (m_viewer) {
        m_viewer->setGrayscale(enabled);
    }
}

void EffectsController::setMirror(bool enabled) {
    if (m_currentTab) {
        m_currentTab->setMirror(enabled);
    }
    if (m_mirrorAction) {
        m_mirrorAction->setChecked(enabled);
    }
    if (m_viewer) {
        m_viewer->setMirror(enabled);
    }
}

void EffectsController::syncFromTab() {
    if (!m_currentTab) {
        if (m_grayscaleAction)
            m_grayscaleAction->setChecked(false);
        if (m_mirrorAction)
            m_mirrorAction->setChecked(false);
        return;
    }

    bool grayscale = m_currentTab->grayscaleEnabled();
    bool mirror = m_currentTab->mirrorEnabled();

    if (m_grayscaleAction)
        m_grayscaleAction->setChecked(grayscale);
    if (m_mirrorAction)
        m_mirrorAction->setChecked(mirror);
    if (m_viewer) {
        m_viewer->setGrayscale(grayscale);
        m_viewer->setMirror(mirror);
    }
}
