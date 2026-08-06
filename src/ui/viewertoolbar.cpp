#include "ui/viewertoolbar.h"
#include "controllers/effectscontroller.h"
#include "ui/viewertools/grayscaletool.h"
#include "ui/viewertools/mirrortool.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QStyle>
#include <QVBoxLayout>

static QIcon themedIcon(const QString& path) {
    QIcon rawIcon(path);
    if (rawIcon.isNull())
        return QIcon();

    QPixmap pixmap = rawIcon.pixmap(QSize(32, 32));
    if (pixmap.isNull())
        return QIcon();

    QPixmap  tinted = pixmap;
    QPainter painter(&tinted);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.setPen(QApplication::palette().color(QPalette::WindowText));
    painter.setBrush(QApplication::palette().color(QPalette::WindowText));
    painter.drawRect(QRect(0, 0, tinted.width(), tinted.height()));
    painter.end();

    return QIcon(tinted);
}

ViewerToolbar::ViewerToolbar(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(4);
    layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    // Define tools to load
    std::vector<std::unique_ptr<IViewerTool>> tools;
    tools.push_back(std::make_unique<GrayscaleTool>());
    tools.push_back(std::make_unique<MirrorTool>());

    for (auto& tool : tools) {
        QPushButton *btn = new QPushButton(this);
        btn->setCheckable(tool->isCheckable());
        btn->setFixedSize(32, 32);
        btn->setToolTip(tool->tooltip());

        // Use themed icon
        btn->setIcon(themedIcon(tool->iconPath()));
        btn->setIconSize(QSize(22, 22));

        btn->setObjectName("toolbarButton");

        connect(btn, &QPushButton::toggled, [tool = tool.get()](bool checked) {
            tool->onToggled(checked);
        });

        layout->addWidget(btn);
        m_tools.push_back({std::move(tool), btn});
    }

    this->setFixedWidth(36);
}

void ViewerToolbar::setup(EffectsController *controller) {
    if (!controller)
        return;

    for (auto& tw : m_tools) {
        tw.tool->setup(controller);
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
