#include "controllers/effectscontroller.h"
#include "ui/imagetab.h"
#include "ui/vkimageviewer.h"

EffectsController::EffectsController(QObject *parent) : QObject(parent) {
}

void EffectsController::setup(IEffectsRenderer *renderer, IEffectsUI *ui) {
    m_renderer = renderer;
    m_ui = ui;

    if (m_renderer) {
        m_renderer->setNotificationCallback(
            [this](bool grayscale, bool mirror) { this->onEffectsChanged(grayscale, mirror); });
    }
}

void EffectsController::onEffectsChanged(bool grayscale, bool mirror) {
    setGrayscale(grayscale);
    setMirror(mirror);
}

void EffectsController::setTargetState(IEffectsState *state) {
    m_state = state;
    syncFromState();
}

void EffectsController::toggleGrayscale() {
    bool current = false;
    if (m_state) {
        current = m_state->grayscaleEnabled();
    } else if (m_ui) {
        current = m_ui->grayscaleChecked();
    }
    setGrayscale(!current);
}

void EffectsController::toggleMirror() {
    bool current = false;
    if (m_state) {
        current = m_state->mirrorEnabled();
    } else if (m_ui) {
        current = m_ui->mirrorChecked();
    }
    setMirror(!current);
}

void EffectsController::setGrayscale(bool enabled) {
    if (m_state) {
        m_state->setGrayscale(enabled);
    }
    if (m_ui) {
        m_ui->setGrayscaleChecked(enabled);
    }
    if (m_renderer) {
        m_renderer->setGrayscale(enabled);
    }
}

void EffectsController::setMirror(bool enabled) {
    if (m_state) {
        m_state->setMirror(enabled);
    }
    if (m_ui) {
        m_ui->setMirrorChecked(enabled);
    }
    if (m_renderer) {
        m_renderer->setMirror(enabled);
    }
}

void EffectsController::syncFromState() {
    if (!m_state) {
        if (m_ui) {
            m_ui->setGrayscaleChecked(false);
            m_ui->setMirrorChecked(false);
        }
        return;
    }

    bool grayscale = m_state->grayscaleEnabled();
    bool mirror = m_state->mirrorEnabled();

    if (m_ui) {
        m_ui->setGrayscaleChecked(grayscale);
        m_ui->setMirrorChecked(mirror);
    }
    if (m_renderer) {
        m_renderer->setGrayscale(grayscale);
        m_renderer->setMirror(mirror);
    }
}
