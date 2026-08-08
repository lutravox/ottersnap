#pragma once

#include "ui/viewertools/viewertool.h"

/**
 * @brief Tool for picking colors from the viewer.
 */
class ColorPickerTool : public IViewerTool {
    Q_OBJECT

  public:
    explicit ColorPickerTool();

    QString name() const override {
        return tr("Color Picker");
    }
    QString tooltip() const override {
        return tr("Color Picker");
    }
    QString iconPath() const override {
        return QString(":/icons/colorpicker.svg");
    }
    QKeySequence shortcut() const override {
        return QKeySequence(Qt::CTRL | Qt::Key_I);
    }
    bool isCheckable() const override {
        return true;
    }

    void onToggled(bool checked) override;
    void syncState(bool state) override;
    void setup(class EffectsController *effects, class ViewerController *viewer) override;

  private:
    class ViewerController *m_viewerController = nullptr;
    bool                    m_checked = false;
};
