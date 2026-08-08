#include "ui/viewerstate.h"

#include <QDebug>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "ui/snapshottimeline.h"
#include "ui/statusbar.h"
#include "ui/viewertoolbar.h"
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
    m_viewerToolbar = new ViewerToolbar(this);

    layout->addWidget(m_snapshotTimeline, 0);

    m_snapshotOnlyLabel = new QLabel(tr("Original image not found. Viewing only snapshots."), this);
    m_snapshotOnlyLabel->setObjectName("snapshotOnlyLabel");
    m_snapshotOnlyLabel->setAlignment(Qt::AlignCenter);
    m_snapshotOnlyLabel->setVisible(false);

    auto *viewerLayout = new QHBoxLayout();
    viewerLayout->setContentsMargins(0, 0, 0, 0);
    viewerLayout->setSpacing(0);
    viewerLayout->addWidget(m_viewerToolbar, 0);

    auto *viewerRightContainer = new QWidget(this);
    auto *viewerRightLayout = new QVBoxLayout(viewerRightContainer);
    viewerRightLayout->setContentsMargins(0, 0, 0, 0);
    viewerRightLayout->setSpacing(0);
    viewerRightLayout->addWidget(m_snapshotOnlyLabel, 0);
    viewerRightLayout->addWidget(m_viewer, 1);

    viewerLayout->addWidget(viewerRightContainer, 1);

    auto *viewerContainer = new QWidget(this);
    viewerContainer->setLayout(viewerLayout);

    layout->addWidget(viewerContainer, 1);
    layout->addWidget(m_statusBar, 0);

    // Wire status bar -> ViewerState signals
    connect(m_statusBar, &StatusBar::zoomChanged, this, &ViewerState::zoomRequested);
    connect(m_statusBar, &StatusBar::fitRequested, this, &ViewerState::fitRequested);

    // Wire viewer zoom -> status bar
    connect(m_viewer, &ImageViewer::zoomChanged, m_statusBar, [this](double pct) {
        m_statusBar->setZoom(pct);
    });
}

void ViewerState::setSnapshotOnlyIndicator(bool visible) {
    if (m_snapshotOnlyLabel) {
        m_snapshotOnlyLabel->setVisible(visible);
    }
}

void ViewerState::setColorInfoVisible(bool visible) {
    if (!m_colorInfo) {
        m_colorInfo = new ColorInfo(m_viewer);
    }
    if (m_colorInfo) {
        m_colorInfo->setVisibleState(visible);
    }
}