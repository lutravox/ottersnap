#include "ui/viewerstate.h"

#include <QDebug>
#include <QFile>
#include <QLabel>
#include <QVBoxLayout>

#include "ui/snapshottimeline.h"
#include "ui/statusbar.h"
#include "ui/vkimageviewer.h"

ViewerState::ViewerState(QWidget *parent) : QWidget(parent) {
    {
        QFile qss(":/qss/vkimageviewer.qss");
        if (qss.open(QIODevice::ReadOnly | QIODevice::Text))
            setStyleSheet(QString::fromUtf8(qss.readAll()));
    }

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_snapshotTimeline = new SnapshotTimeline(this);
    m_viewer = new ImageViewer(this);
    m_statusBar = new StatusBar(this);

    layout->addWidget(m_snapshotTimeline, 0);
    layout->addWidget(m_viewer, 1);
    layout->addWidget(m_statusBar, 0);

    // Wire status bar -> ViewerState signals
    connect(m_statusBar, &StatusBar::zoomChanged, this, &ViewerState::zoomRequested);
    connect(m_statusBar, &StatusBar::fitRequested, this, &ViewerState::fitRequested);

    // Wire viewer zoom -> status bar
    connect(m_viewer, &ImageViewer::zoomChanged, m_statusBar, [this](double pct) {
        m_statusBar->setZoom(pct);
    });
}
