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
    struct ToolWidget {
        std::unique_ptr<IViewerTool> tool;
        QPushButton *button;
    };

    explicit ViewerToolbar(QWidget *parent = nullptr);

    /**
     * @brief Connects the toolbar buttons to the controllers.
     */
    void setup(EffectsController *effects, class ViewerController *viewer);

    /// @brief Updates the enabled/disabled state of all tools.
    void updateToolStates();

    /**
     * @brief Activates or toggles a tool by its name.
     * @param name The name of the tool to activate.
     */
    void activateTool(const QString& name);

    /**
     * @brief Returns the list of tools managed by the toolbar.
     */
    const std::vector<ToolWidget>& tools() const { return m_tools; }

    // IEffectsUI implementation
    void setGrayscaleChecked(bool checked) override;
    void setMirrorChecked(bool checked) override;
    bool grayscaleChecked() const override;
    bool mirrorChecked() const override;
    bool colorPickerChecked() const;

  private:
    std::vector<ToolWidget> m_tools;
};
