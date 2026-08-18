#include <vector>
#include "controllers/effectscontroller.h"

EffectsController::EffectsController(QObject *parent) : QObject(parent) {
}

void EffectsController::setup(IEffectsRenderer *renderer) {
    m_renderer = renderer;
}

void EffectsController::addUI(IEffectsUI *ui) {
    if (ui) {
        m_uis.push_back(ui);
        // Sync new UI component with current state
        if (m_state) {
            ui->setGrayscaleChecked(m_state->grayscaleEnabled());
            ui->setMirrorChecked(m_state->mirrorEnabled());
        }
    }
}

void EffectsController::onEffectsChanged(bool grayscale, bool mirror) {
    setGrayscale(grayscale);
    setMirror(mirror);
}

void EffectsController::setTargetState(IEffectsModel *state) {
    m_state = state;
    syncFromState();
}

void EffectsController::toggleGrayscale() {
    bool current = false;
    if (m_state) {
        current = m_state->grayscaleEnabled();
    } else if (!m_uis.empty()) {
        current = m_uis[0]->grayscaleChecked();
    }
    setGrayscale(!current);
}

void EffectsController::toggleMirror() {
    bool current = false;
    if (m_state) {
        current = m_state->mirrorEnabled();
    } else if (!m_uis.empty()) {
        current = m_uis[0]->mirrorChecked();
    }
    setMirror(!current);
}

void EffectsController::setGrayscale(bool enabled) {
    if (m_state) {
        m_state->setGrayscale(enabled);
    }
    for (auto *ui : m_uis) {
        ui->setGrayscaleChecked(enabled);
    }
}

void EffectsController::setMirror(bool enabled) {
    if (m_state) {
        m_state->setMirror(enabled);
    }
    for (auto *ui : m_uis) {
        ui->setMirrorChecked(enabled);
    }
}

void EffectsController::reset() {
    setGrayscale(false);
    setMirror(false);
}

void EffectsController::syncFromState() {
    if (!m_state) {
        for (auto *ui : m_uis) {
            ui->setGrayscaleChecked(false);
            ui->setMirrorChecked(false);
        }
        return;
    }

    bool grayscale = m_state->grayscaleEnabled();
    bool mirror = m_state->mirrorEnabled();

    for (auto *ui : m_uis) {
        ui->setGrayscaleChecked(grayscale);
        ui->setMirrorChecked(mirror);
    }
}
