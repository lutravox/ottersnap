#pragma once

#include "ui/viewertools/viewertool.h"

class MirrorTool : public IViewerTool {
  public:
    explicit MirrorTool();
    QString name() const override { return "Mirror"; }
    QString actionId() const override { return "tool.mirror"; }
    QString tooltip() const override { return "Mirror"; }
    QString iconPath() const override;
    QKeySequence shortcut() const override {
        return QKeySequence(Qt::CTRL | Qt::Key_M);
    }
    bool isCheckable() const override { return true; }
    void onToggled(bool checked) override;
    void syncState(bool state) override;
    void setup(class EffectsController *effects, class ViewerController *viewer) override;

  private:
    class EffectsController *m_controller = nullptr;
    bool m_state = false;
};
