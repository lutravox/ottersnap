#pragma once

#include <QFrame>
#include <QPushButton>
#include <memory>
#include <vector>
#include "core/effects_interfaces.h"
#include "ui/viewertools/viewertool.h"

class EffectsController;

/**
 * @class ViewerToolbar
 * @brief A vertical toolbar for quick access to image effects.
 */
class ViewerToolbar : public QFrame, public IEffectsUI {
    Q_OBJECT

  public:
    explicit ViewerToolbar(QWidget *parent = nullptr);

    /**
     * @brief Connects the toolbar buttons to the controllers.
     */
    void setup(EffectsController *effects, class ViewerController *viewer);

    /// @brief Updates the enabled/disabled state of all tools.
    void updateToolStates();

    // IEffectsUI implementation
    void setGrayscaleChecked(bool checked) override;
    void setMirrorChecked(bool checked) override;
    bool grayscaleChecked() const override;
    bool mirrorChecked() const override;

  private:
    struct ToolWidget {
        std::unique_ptr<IViewerTool> tool;
        QPushButton *button;
    };

    std::vector<ToolWidget> m_tools;
};
