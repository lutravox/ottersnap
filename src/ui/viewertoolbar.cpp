#include "ui/viewertoolbar.h"
#include "controllers/effectscontroller.h"
#include "controllers/viewercontroller.h"
#include "ui/viewertools/colorpickertool.h"
#include "ui/viewertools/grayscaletool.h"
#include "ui/viewertools/mirrortool.h"
#include "ui/viewertools/swaptool.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QStyle>
#include <QVBoxLayout>

static QPixmap createTintedPixmap(const QString& path, const QColor& color) {
    QIcon rawIcon(path);
    if (rawIcon.isNull())
        return QPixmap();

    QPixmap pixmap = rawIcon.pixmap(QSize(32, 32));
    if (pixmap.isNull())
        return QPixmap();

    QPixmap  tinted = pixmap;
    QPainter painter(&tinted);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.setPen(color);
    painter.setBrush(color);
    painter.drawRect(QRect(0, 0, tinted.width(), tinted.height()));
    painter.end();

    return tinted;
}

static QIcon themedIcon(const QString& path) {
    QIcon icon;
    icon.addPixmap(createTintedPixmap(path, QApplication::palette().color(QPalette::WindowText)),
                   QIcon::Normal,
                   QIcon::Off);
    icon.addPixmap(
        createTintedPixmap(path,
                           QApplication::palette().color(QPalette::Disabled, QPalette::WindowText)),
        QIcon::Disabled,
        QIcon::Off);
    return icon;
}

ViewerToolbar::ViewerToolbar(QWidget *parent) : QFrame(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(4);
    layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    // Define tools to load
    std::vector<std::unique_ptr<IViewerTool>> tools;
    tools.push_back(std::make_unique<SwapTool>());
    tools.push_back(std::make_unique<GrayscaleTool>());
    tools.push_back(std::make_unique<MirrorTool>());
    tools.push_back(std::make_unique<ColorPickerTool>());

    for (auto& tool : tools) {
        if (tool->name() == "Color Picker") {
            QWidget *separator = new QWidget(this);
            separator->setFixedHeight(1);
            separator->setObjectName("toolbarSeparator");
            layout->addWidget(separator);
        }

        QPushButton *btn = new QPushButton(this);
        btn->setCheckable(tool->isCheckable());
        btn->setFixedSize(32, 32);
        btn->setToolTip(tool->fullTooltip());

        // Use themed icon
        btn->setIcon(themedIcon(tool->iconPath()));
        btn->setIconSize(QSize(22, 22));

        btn->setObjectName("toolbarButton");
        btn->setFocusPolicy(Qt::NoFocus);

        if (btn->isCheckable()) {
            connect(btn, &QPushButton::toggled, [tool = tool.get()](bool checked) {
                tool->onToggled(checked);
            });
        } else {
            connect(btn, &QPushButton::clicked, [tool = tool.get()]() { tool->onToggled(false); });
        }

        layout->addWidget(btn);

        if (tool->name() == "Swap") {
            QWidget *separator = new QWidget(this);
            separator->setFixedHeight(1);
            separator->setObjectName("toolbarSeparator");
            layout->addWidget(separator);
        }

        m_tools.push_back({std::move(tool), btn});
    }

    this->setFixedWidth(36);
    this->setObjectName("viewerToolbar");
}

void ViewerToolbar::activateTool(const QString& name) {
    for (auto& tw : m_tools) {
        if (tw.tool->name() == name) {
            bool newState = !tw.button->isChecked();
            tw.button->setChecked(newState);
            tw.tool->onToggled(newState);
            return;
        }
    }
}

void ViewerToolbar::setup(EffectsController *effects, ViewerController *viewer) {
    if (!effects || !viewer)
        return;

    for (auto& tw : m_tools) {
        tw.tool->setup(effects, viewer);
    }
    updateToolStates();
}

void ViewerToolbar::updateToolStates() {
    for (auto& tw : m_tools) {
        tw.button->setEnabled(tw.tool->isEnabled());
    }
}

void ViewerToolbar::setGrayscaleChecked(bool checked) {
    for (auto& tw : m_tools) {
        if (tw.tool->name() == "Grayscale") {
            tw.tool->syncState(checked);
            tw.button->setChecked(checked);
            break;
        }
    }
}

void ViewerToolbar::setMirrorChecked(bool checked) {
    for (auto& tw : m_tools) {
        if (tw.tool->name() == "Mirror") {
            tw.tool->syncState(checked);
            tw.button->setChecked(checked);
            break;
        }
    }
}

bool ViewerToolbar::grayscaleChecked() const {
    for (auto& tw : m_tools) {
        if (tw.tool->name() == "Grayscale") {
            return tw.button->isChecked();
        }
    }
    return false;
}

bool ViewerToolbar::mirrorChecked() const {
    for (auto& tw : m_tools) {
        if (tw.tool->name() == "Mirror") {
            return tw.button->isChecked();
        }
    }
    return false;
}

bool ViewerToolbar::colorPickerChecked() const {
    for (auto& tw : m_tools) {
        if (tw.tool->name() == "Color Picker") {
            return tw.button->isChecked();
        }
    }
    return false;
}

void ViewerToolbar::updateShortcuts(ShortcutManager* manager) {
    if (!manager)
        return;

    for (auto& tw : m_tools) {
        QString actionId = tw.tool->actionId();
        QKeySequence seq = manager->shortcutFor(actionId);
        
        QString tooltipText = tw.tool->tooltip();
        if (!seq.isEmpty()) {
            QString shortcutStr = seq.toString();
            shortcutStr.replace("+", " + ");
            tooltipText += QString("\n-\n%1").arg(shortcutStr);
        }
        tw.button->setToolTip(tooltipText);
    }
}
