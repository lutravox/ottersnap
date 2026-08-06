#include "ui/viewertools/grayscaletool.h"
#include "controllers/effectscontroller.h"

GrayscaleTool::GrayscaleTool() = default;

QString GrayscaleTool::iconPath() const {
    return ":/icons/grayscale.svg";
}

void GrayscaleTool::onToggled(bool checked) {
    if (m_controller) {
        m_controller->setGrayscale(checked);
    }
}

void GrayscaleTool::syncState(bool state) {
    m_state = state;
}

void GrayscaleTool::setup(EffectsController *controller) {
    m_controller = controller;
}
