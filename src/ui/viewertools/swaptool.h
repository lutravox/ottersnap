#pragma once

#include "ui/viewertools/viewertool.h"

/**
 * @class SwapTool
 * @brief A tool that swaps the primary and secondary snapshots for comparison.
 */
class SwapTool : public IViewerTool {
    Q_OBJECT

  public:
    explicit SwapTool();

    QString name() const override {
        return tr("Swap");
    }
    QString actionId() const override {
        return "tool.swap";
    }
    QString tooltip() const override {
        return tr("Swap Comparison");
    }
    QString iconPath() const override {
        return QString(":/icons/swap.svg");
    }
    QKeySequence shortcut() const override {
        return QKeySequence(Qt::CTRL | Qt::Key_P);
    }
    bool isCheckable() const override {
        return false;
    }

    void onToggled(bool checked) override;
    void syncState(bool state) override;
    void setup(EffectsController *effects, ViewerController *viewer) override;
    bool isEnabled() const override;

  private:
    class ViewerController *m_viewerController = nullptr;
};
