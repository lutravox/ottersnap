#include "ui/viewertools/mirrortool.h"
#include "controllers/effectscontroller.h"

MirrorTool::MirrorTool() = default;

QString MirrorTool::iconPath() const {
    return ":/icons/mirror.svg";
}

void MirrorTool::onToggled(bool checked) {
    if (m_controller) {
        m_controller->setMirror(checked);
    }
}

void MirrorTool::syncState(bool state) {
    m_state = state;
}

void MirrorTool::setup(EffectsController *controller) {
    m_controller = controller;
}
