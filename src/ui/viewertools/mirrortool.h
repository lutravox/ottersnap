#pragma once

#include "ui/viewertools/viewertool.h"

class MirrorTool : public IViewerTool {
  public:
    explicit MirrorTool();
    QString name() const override { return "Mirror"; }
    QString tooltip() const override { return "Mirror"; }
    QString iconPath() const override;
    bool isCheckable() const override { return true; }
    void onToggled(bool checked) override;
    void syncState(bool state) override;
    void setup(class EffectsController *effects, class ViewerController *viewer) override;

  private:
    class EffectsController *m_controller = nullptr;
    bool m_state = false;
};
