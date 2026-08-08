#pragma once

#include "ui/viewertools/viewertool.h"

class GrayscaleTool : public IViewerTool {
  public:
    explicit GrayscaleTool();
    QString name() const override { return "Grayscale"; }
    QString tooltip() const override { return "Grayscale"; }
    QString iconPath() const override;
    QKeySequence shortcut() const override {
        return QKeySequence(Qt::CTRL | Qt::Key_G);
    }
    bool isCheckable() const override { return true; }
    void onToggled(bool checked) override;
    void syncState(bool state) override;
    void setup(class EffectsController *effects, class ViewerController *viewer) override;

  private:
    class EffectsController *m_controller = nullptr;
    bool m_state = false;
};
