#include "ui/viewertools/colorpickertool.h"
#include "controllers/viewercontroller.h"
#include "controllers/effectscontroller.h"

ColorPickerTool::ColorPickerTool() {
}

void ColorPickerTool::onToggled(bool checked) {
    m_checked = checked;
    if (m_viewerController) {
        m_viewerController->setPickingEnabled(checked);
    }
}

void ColorPickerTool::syncState(bool state) {
    m_checked = state;
}

void ColorPickerTool::setup(EffectsController *effects, ViewerController *viewer) {
    m_viewerController = viewer;
}
