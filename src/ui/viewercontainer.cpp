#include "ui/viewercontainer.h"

#include <QVBoxLayout>

#include "ui/statusbar.h"
#include "ui/thumbnailstrip.h"
#include "ui/vkimageviewer.h"

ViewerContainer::ViewerContainer(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_thumbnailStrip = new ThumbnailStrip(this);
    m_viewer = new ImageViewer(this);
    m_statusBar = new StatusBar(this);

    layout->addWidget(m_thumbnailStrip, 0);
    layout->addWidget(m_viewer, 1);
    layout->addWidget(m_statusBar, 0);

    // Wire status bar -> viewer
    connect(m_statusBar, &StatusBar::zoomChanged, m_viewer, &ImageViewer::setZoomPercentage);
    connect(m_statusBar, &StatusBar::fitRequested, m_viewer, &ImageViewer::fitToWindow);

    // Wire viewer zoom -> status bar
    connect(m_viewer, &ImageViewer::zoomChanged, m_statusBar, [this](double pct) {
        m_statusBar->setZoom(pct);
    });
}
