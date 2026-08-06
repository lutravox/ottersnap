#include "ui/viewertools/swaptool.h"
#include "controllers/effectscontroller.h"
#include "controllers/viewercontroller.h"

SwapTool::SwapTool() {
}

void SwapTool::onToggled(bool checked) {
    Q_UNUSED(checked);
    if (m_viewerController) {
        m_viewerController->swapPrimaryAndSecondary();
    }
}

void SwapTool::syncState(bool state) {
    Q_UNUSED(state);
}

void SwapTool::setup(EffectsController *effects, ViewerController *viewer) {
    Q_UNUSED(effects);
    m_viewerController = viewer;
}

bool SwapTool::isEnabled() const {
    if (!m_viewerController)
        return false;
    return m_viewerController->canSwap();
}
