#include "ui/mainwindow.h"
#include "core/imagesession.h"
#include "core/snapshotstore.h"
#include "core/thumbnailcache.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleHints>
#include <QVBoxLayout>
#include <QtConcurrent>

#include "config/appsettings.h"
#include "controllers/effectscontroller.h"
#include "core/vulkancontext.h"
#include "ui/dialogs/settingsdialog.h"
#include "ui/emptystate.h"
#include "ui/imagetab.h"
#include "ui/snapshottimeline.h"
#include "ui/tabbar.h"
#include "ui/viewerstate.h"
#include "ui/vkimageviewer.h"

static const int c_defaultWidth = 1000;
static const int c_defaultHeight = 700;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_tabBar(nullptr), m_viewerState(nullptr), m_emptyState(nullptr),
      m_session(m_settings) {
    m_effectsController = new EffectsController(this);
    m_viewerController = new ViewerController(this);

    connect(m_viewerController,
            &ViewerController::grayscaleToggled,
            m_effectsController,
            &EffectsController::setGrayscale);
    connect(m_viewerController,
            &ViewerController::mirrorToggled,
            m_effectsController,
            &EffectsController::setMirror);

    setupMenu();
    setupUi();
    m_session.load();
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent *event) {
    m_session.save(collectOpenPaths());

    //  Cleanup tab session resources
    if (m_tabBar) {
        for (int i = 0; i < m_tabBar->count(); ++i) {
            auto *tab = qobject_cast<ImageTab *>(m_tabBar->widget(i));
            if (tab) {
                tab->closeImage();
            }
        }
    }

    QMainWindow::closeEvent(event);
}

QStringList MainWindow::collectOpenPaths() const {
    QStringList paths;
    if (!m_tabBar)
        return paths;

    for (int i = 0; i < m_tabBar->count(); ++i) {
        auto *tab = qobject_cast<ImageTab *>(m_tabBar->widget(i));
        if (tab && !tab->filePath().isEmpty())
            paths << tab->filePath();
    }
    return paths;
}

void MainWindow::setupUi() {
    setWindowTitle(AppSettings::applicationName());
    resize(c_defaultWidth, c_defaultHeight);
    setAcceptDrops(true);

    QWidget *central = new QWidget(this);
    auto    *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    // Stacked widget
    m_contentStack = new QStackedWidget(central);

    // Viewer state: thumbnail strip + viewer + nav bar
    m_viewerState = new ViewerState(m_contentStack);

    // Empty state
    m_emptyState = new EmptyState(m_contentStack);

    // Stack states
    m_contentStack->addWidget(m_emptyState);
    m_contentStack->addWidget(m_viewerState);
    m_contentStack->setCurrentWidget(m_emptyState);

    // Tab bar
    m_tabBar = new TabBar(central);
    connect(m_tabBar, &QTabWidget::tabCloseRequested, this, &MainWindow::onCloseTab);
    connect(m_tabBar, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    centralLayout->addWidget(m_tabBar, 0);

    centralLayout->addWidget(m_contentStack, 1);

    connect(m_viewerState->snapshotTimeline(),
            &SnapshotTimeline::snapshotSelected,
            this,
            &MainWindow::onSnapshotSelected);

    connect(m_viewerState, &ViewerState::zoomRequested, this, [this](double pct) {
        if (m_viewerController) {
            m_viewerController->setZoomPercentage(pct);
        }
    });

    connect(m_viewerState, &ViewerState::fitRequested, this, [this]() {
        if (m_viewerController) {
            m_viewerController->fitToWindow();
        }
    });

    connect(m_viewerState->snapshotTimeline(),
            &SnapshotTimeline::createSnapshotRequested,
            this,
            &MainWindow::onSaveSnapshot);

    connect(m_viewerState->snapshotTimeline(),
            &SnapshotTimeline::snapshotDeletionRequested,
            this,
            &MainWindow::onSnapshotDeletionRequested);

    // Controllers
    auto *uiAdapter = new EffectsUIAdapter(m_actionGrayscale, m_actionMirror);
    m_effectsController->setup(m_viewerState->viewer(), uiAdapter);
    m_viewerController->setViewer(m_viewerState->viewer());

    // Connect empty state actions
    connect(m_emptyState, &EmptyState::openRequested, this, &MainWindow::onFileOpen);
    connect(m_emptyState, &EmptyState::settingsRequested, this, &MainWindow::onSettings);

    // Notifications
    m_notificationManager = new NotificationManager(this);

    // Connect viewer resizes to controller
    connect(m_viewerState->viewer(),
            &VkImageViewer::viewportResized,
            m_viewerController,
            &ViewerController::handleViewportResize);

    // Connect image drop and drop on viewer
    connect(m_viewerState->viewer(),
            &VkImageViewer::imageOpenRequested,
            this,
            [this](const QString& path) { openImageFile(path); });

    setCentralWidget(central);

    updateViewer();
}

void MainWindow::setupMenu() {
    m_fileMenu = menuBar()->addMenu(tr("&File"));
    m_actionOpen = m_fileMenu->addAction(tr("&Open Image..."));
    m_actionOpen->setShortcut(QKeySequence::Open);
    connect(m_actionOpen, &QAction::triggered, this, &MainWindow::onFileOpen);

    m_recentFilesMenu = m_fileMenu->addMenu(tr("Recent Files"));

    m_fileMenu->addSeparator();

    m_actionSaveSnapshot = m_fileMenu->addAction(tr("&Save Snapshot of Current"));
    m_actionSaveSnapshot->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    connect(m_actionSaveSnapshot, &QAction::triggered, this, &MainWindow::onSaveSnapshot);

    m_actionExportSnapshot = m_fileMenu->addAction(tr("&Export Snapshot As..."));
    connect(m_actionExportSnapshot, &QAction::triggered, this, &MainWindow::onExportSnapshot);

    m_actionDeleteSnapshot = m_fileMenu->addAction(tr("&Delete Selected Snapshot"));
    m_actionDeleteSnapshot->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(m_actionDeleteSnapshot,
            &QAction::triggered,
            this,
            &MainWindow::onDeleteCurrentSnapshotRequested);

    m_fileMenu->addSeparator();

    m_actionCloseTab = m_fileMenu->addAction(tr("Close &Tab"));
    m_actionCloseTab->setShortcut(QKeySequence::Close);
    connect(m_actionCloseTab, &QAction::triggered, this, &MainWindow::onCloseCurrentTab);

    m_actionCloseAllTabs = m_fileMenu->addAction(tr("Close All &Tabs"));
    connect(m_actionCloseAllTabs, &QAction::triggered, this, &MainWindow::onCloseAllTabs);

    m_fileMenu->addSeparator();

    m_actionExit = m_fileMenu->addAction(tr("E&xit"));
    m_actionExit->setShortcut(QKeySequence::Quit);
    connect(m_actionExit, &QAction::triggered, this, &QWidget::close);

    m_editMenu = menuBar()->addMenu(tr("&Edit"));
    m_actionSettings = m_editMenu->addAction(tr("&Settings"));
    connect(m_actionSettings, &QAction::triggered, this, &MainWindow::onSettings);

    m_viewMenu = menuBar()->addMenu(tr("&View"));
    m_actionScaleWithWindow = m_viewMenu->addAction(tr("&Scale with Window"));
    m_actionScaleWithWindow->setCheckable(true);
    m_actionScaleWithWindow->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    connect(
        m_actionScaleWithWindow, &QAction::triggered, this, &MainWindow::onToggleScaleWithWindow);

    m_actionResetView = m_viewMenu->addAction(tr("Reset &View"));
    m_actionResetView->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(m_actionResetView, &QAction::triggered, this, &MainWindow::onResetView);

    m_actionActualSize = m_viewMenu->addAction(tr("&Actual Size (100%)"));
    m_actionActualSize->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
    connect(m_actionActualSize, &QAction::triggered, this, &MainWindow::onActualSize);

    m_viewMenu->addSeparator();

    m_actionZoomIn = m_viewMenu->addAction(tr("Zoom &In"));
    m_actionZoomIn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus));
    connect(m_actionZoomIn, &QAction::triggered, this, &MainWindow::onZoomIn);

    m_actionZoomOut = m_viewMenu->addAction(tr("Zoom &Out"));
    m_actionZoomOut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    connect(m_actionZoomOut, &QAction::triggered, this, &MainWindow::onZoomOut);

    m_viewMenu->addSeparator();

    m_actionFullScreen = m_viewMenu->addAction(tr("Toggle &Full Screen"));
    m_actionFullScreen->setShortcut(QKeySequence(Qt::Key_F11));
    connect(m_actionFullScreen, &QAction::triggered, this, &MainWindow::onToggleFullScreen);

    m_effectsMenu = menuBar()->addMenu(tr("&Effects"));
    m_actionGrayscale = m_effectsMenu->addAction(tr("&Grayscale"));
    m_actionGrayscale->setCheckable(true);
    m_actionGrayscale->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(m_actionGrayscale, &QAction::triggered, this, [this]() {
        m_effectsController->toggleGrayscale();
    });

    m_actionMirror = m_effectsMenu->addAction(tr("&Mirror"));
    m_actionMirror->setCheckable(true);
    m_actionMirror->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    connect(m_actionMirror, &QAction::triggered, this, [this]() {
        m_effectsController->toggleMirror();
    });

    m_effectsMenu->addSeparator();
    m_actionResetEffects = m_effectsMenu->addAction(tr("Reset All &Effects"));
    connect(m_actionResetEffects, &QAction::triggered, this, &MainWindow::onResetEffects);

    updateMenuBar();
    updateRecentFilesMenu();
}

void MainWindow::updateMenuBar() {
    // Common actions always enabled and visible
    m_actionOpen->setEnabled(true);
    m_actionSettings->setEnabled(true);
    m_actionExit->setEnabled(true);
    m_fileMenu->menuAction()->setVisible(true);
    m_editMenu->menuAction()->setVisible(true);

    // Update action states based on current selection
    auto *tab = currentTab();
    if (tab) {
        int         index = tab->session()->currentSnapshotIndex();
        const auto& snapshots = tab->session()->snapshots();
        // Disable if no snapshot is selected or if it's the current image ('C')
        m_actionDeleteSnapshot->setEnabled(index >= 0 && !tab->session()->isCurrentImage(index));
    } else {
        m_actionDeleteSnapshot->setEnabled(false);
    }

    switch (m_currentState) {
        case ContentState::Empty:
            m_actionSaveSnapshot->setVisible(false);
            m_actionDeleteSnapshot->setVisible(false);
            m_actionCloseTab->setVisible(false);
            m_actionCloseAllTabs->setVisible(false);
            m_actionScaleWithWindow->setVisible(false);
            m_actionResetView->setVisible(false);
            m_actionGrayscale->setVisible(false);
            m_actionMirror->setVisible(false);

            m_viewMenu->menuAction()->setVisible(false);
            m_effectsMenu->menuAction()->setVisible(false);
            break;

        case ContentState::Viewer:
            m_actionSaveSnapshot->setVisible(true);
            m_actionDeleteSnapshot->setVisible(true);
            m_actionCloseTab->setVisible(true);
            m_actionCloseAllTabs->setVisible(true);
            m_actionScaleWithWindow->setVisible(true);
            m_actionScaleWithWindow->setChecked(m_viewerController->isScaleWithWindowEnabled());
            m_actionResetView->setVisible(true);
            m_actionGrayscale->setVisible(true);
            m_actionMirror->setVisible(true);

            m_viewMenu->menuAction()->setVisible(true);
            m_effectsMenu->menuAction()->setVisible(true);
            break;
    }
}

void MainWindow::onCloseCurrentTab() {
    onCloseTab(m_tabBar->currentIndex());
}

void MainWindow::onCloseAllTabs() {
    while (m_tabBar->count() > 0) {
        onCloseTab(0);
    }
    m_currentVersionInView = -1;
}

void MainWindow::onToggleScaleWithWindow() {
    bool enabled = !m_viewerController->isScaleWithWindowEnabled();
    m_viewerController->setScaleWithWindowEnabled(enabled);
    m_actionScaleWithWindow->setChecked(enabled);
}

void MainWindow::onResetView() {
    m_viewerController->fitToWindow();
}

void MainWindow::onFileOpen() {
    QString path =
        QFileDialog::getOpenFileName(this, tr("Open Image"), "", AppSettings::fileFilter());
    if (path.isEmpty())
        return;
    openImageFile(path);

    // Track recent files
    QStringList recent = m_settings.value("recentFiles").toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 10)
        recent.removeLast();
    m_settings.setValue("recentFiles", recent);
    updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu() {
    m_recentFilesMenu->clear();
    QStringList recent = m_settings.value("recentFiles").toStringList();
    for (const QString& path : recent) {
        QAction *action = m_recentFilesMenu->addAction(QFileInfo(path).fileName());
        connect(action, &QAction::triggered, this, [this, path]() { openImageFile(path); });
    }
}

void MainWindow::onResetEffects() {
    if (m_effectsController) {
        m_effectsController->setGrayscale(false);
        m_effectsController->setMirror(false);
    }
}

void MainWindow::onZoomIn() {
    if (!m_viewerController)
        return;
    m_viewerController->handleZoomRequested(true, false);
}

void MainWindow::onZoomOut() {
    if (!m_viewerController)
        return;
    m_viewerController->handleZoomRequested(false, false);
}

void MainWindow::onActualSize() {
    if (!m_viewerController)
        return;
    m_viewerController->setZoomPercentage(100.0);
}

void MainWindow::onToggleFullScreen() {
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void MainWindow::onExportSnapshot() {
    auto *tab = currentTab();
    if (!tab)
        return;

    QString baseName = QFileInfo(tab->filePath()).baseName();
    QString suggestedName;
    if (m_currentVersionInView >= 0) {
        suggestedName = QString("%1_snapshot_%2").arg(baseName).arg(m_currentVersionInView + 1);
    } else {
        suggestedName = baseName;
    }

    QString path = QFileDialog::getSaveFileName(
        this, tr("Export Snapshot"), suggestedName, AppSettings::fileFilter());
    if (path.isEmpty())
        return;

    struct ExportParams {
        QString                               path;
        std::optional<ReconstructionSequence> seq;
        QImage                                diskImage;
        bool                                  isSnapshot;
    };

    ExportParams params;
    params.path = path;
    if (m_currentVersionInView >= 0) {
        params.isSnapshot = true;
        auto session = tab->session();
        params.seq =
            session ? session->getReconstructionSequence(m_currentVersionInView) : std::nullopt;
        if (!params.seq) {
            notify(tr("Failed to export snapshot."));
            qDebug() << "[MainWindow] Failed to retrieve reconstruction sequence for export.";
            return;
        }
    } else {
        params.isSnapshot = false;
        params.diskImage = tab->diskImage();
    }

    auto watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, [this, watcher]() {
        if (watcher->result()) {
            notify(tr("Snapshot exported successfully."));
        } else {
            notify(tr("Failed to export snapshot."));
        }
        watcher->deleteLater();
    });

    watcher->setFuture(QtConcurrent::run([params]() {
        QImage img;
        if (params.isSnapshot) {
            img = VulkanContext::instance().getUtilityReconstructor()->reconstructToImage(
                *params.seq);
        } else {
            img = params.diskImage;
        }

        if (img.isNull())
            return false;

        return img.save(params.path);
    }));
}

void MainWindow::onSaveSnapshot() {
    auto *tab = currentTab();
    if (tab) {
        tab->saveSnapshot();
    }
}

void MainWindow::onDeleteCurrentSnapshotRequested() {
    auto *tab = currentTab();
    if (!tab)
        return;

    int index = tab->session()->currentSnapshotIndex();

    // The current disk image ('C') cannot be deleted.
    if (index < 0 || tab->session()->isCurrentImage(index)) {
        return;
    }

    onSnapshotDeletionRequested(index);
}

void MainWindow::openImageFile(const QString& path, bool setAsCurrent) {
    // Check if already open
    if (auto *existing = m_tabPaths.value(path)) {
        if (setAsCurrent) {
            m_tabBar->setCurrentWidget(existing);
        }
        return;
    }

    auto *tab = new ImageTab(this);
    connect(tab, &ImageTab::statusMessage, this, [this](const QString& msg, int timeout) {
        notify(msg, timeout);
    });
    connect(tab, &ImageTab::thumbnailUpdated, this, [this, tab](int index, const QPixmap& pixmap) {
        if (tab == currentTab()) {
            m_viewerState->snapshotTimeline()->updateThumbnail(index, pixmap);
        }
    });
    connect(tab, &ImageTab::tabIconChanged, this, [this, tab](const QPixmap& pixmap) {
        int index = m_tabBar->indexOf(tab);
        if (index != -1) {
            m_tabBar->setTabIcon(
                index, QIcon(pixmap.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        }
    });
    connect(tab, &ImageTab::snapshotsChanged, this, &MainWindow::syncTimelineSelection);
    connect(tab, &ImageTab::snapshotChanged, this, &MainWindow::onSnapshotChanged);
    connect(tab, &ImageTab::snapshotCreated, this, [this, tab](int snapshotIdx) {
        if (tab == currentTab()) {
            m_viewerState->snapshotTimeline()->markSnapshotAsNew(snapshotIdx);
        }
    });

    QString displayName = QFileInfo(path).fileName();
    int     index = m_tabBar->addTab(tab, displayName);
    tab->openImage(path);
    setTabThumbnail(index);
    m_tabPaths.insert(path, tab);

    if (setAsCurrent || m_isRestoringSession) {
        if (setAsCurrent) {
            m_tabBar->setCurrentWidget(tab);
        }
        updateViewer(tab);
        m_viewerController->fitToWindow();
    }
}

void MainWindow::onCloseTab(int index) {
    if (index < 0 || index >= m_tabBar->count())
        return;

    auto *tab = qobject_cast<ImageTab *>(m_tabBar->widget(index));
    if (tab) {
        m_tabPaths.remove(tab->filePath());
        tab->closeImage();
        disconnect(tab, &ImageTab::statusMessage, this, nullptr);
        disconnect(tab, &ImageTab::effectsChanged, this, nullptr);
        disconnect(tab, &ImageTab::snapshotChanged, this, nullptr);
        tab->deleteLater();
    }

    m_tabBar->removeTab(index);

    updateSnapshotTimeline();
    updateViewer();
    updateState();
}

void MainWindow::applyEffects() {
    auto *tab = currentTab();
    if (!tab)
        return;

    m_effectsController->setTargetState(tab->session());
}

void MainWindow::onTabChanged(int index) {
    if (index < 0) {
        qDebug() << "[MainWindow] index passed was negative";
        return;
    }

    m_currentVersionInView = -1;
    updateSnapshotTimeline();
    updateViewer();
    updateState();
    updateMenuBar();
    setTabThumbnail(index);
}

void MainWindow::setTabThumbnail(int index) {
    if (index < 0 || index >= m_tabBar->count()) {
        qDebug() << "[MainWindow] index" << index << "out of bounds (count:" << m_tabBar->count()
                 << ")";
        return;
    }

    auto *tab = qobject_cast<ImageTab *>(m_tabBar->widget(index));
    if (!tab)
        return;

    QImage img = tab->session()->thumbnail(ThumbnailConstants::StandardSize);
    if (!img.isNull()) {
        QPixmap thumb = QPixmap::fromImage(img);
        m_tabBar->setTabIcon(
            index, QIcon(thumb.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    }
}

ImageTab *MainWindow::currentTab() {
    if (!m_tabBar)
        return nullptr;
    return qobject_cast<ImageTab *>(m_tabBar->currentWidget());
}

void MainWindow::notify(const QString& msg, int timeoutMs) {
    m_notificationManager->notify(msg, timeoutMs);
}

void MainWindow::syncTimelineSelection() {
    auto *tab = currentTab();
    if (!tab)
        return;
    updateSnapshotTimeline();
    m_viewerState->snapshotTimeline()->setSelectedIndex(tab->session()->currentSnapshotIndex());
}

void MainWindow::onSnapshotChanged(int index) {
    auto *tab = currentTab();
    if (!tab)
        return;

    updateViewer(tab);
}

void MainWindow::updateSnapshotTimeline() {
    // No need to update the timeline repeatedly if restoring a bunch of tabs
    if (m_isRestoringSession)
        return;

    auto *tab = currentTab();
    if (!tab) {
        return;
    }

    auto [images, labels, indices] =
        tab->session()->snapshotTimelineThumbnails(ThumbnailConstants::StandardSize);
    QVector<QPixmap> thumbs;
    thumbs.reserve(images.size());

    for (const auto& img : images) {
        if (img.isNull()) {
            thumbs.append(QPixmap());
        } else {
            thumbs.append(QPixmap::fromImage(img));
        }
    }

    m_viewerState->snapshotTimeline()->setThumbnails(thumbs, labels, indices);
    m_viewerState->snapshotTimeline()->setSelectedIndex(tab->session()->currentSnapshotIndex());
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);

    if (!AppSettings::restoreSession())
        return;

    // Defer session restore until showEvent() so that the layout is fully
    // settled and QWindowContainer has its real size for proper rendering.
    m_isRestoringSession = true;
    QStringList paths = m_session.restorePaths();
    for (size_t i = 0; i < paths.size(); ++i) {
        const QString& path = paths.at(i);
        if (QFile::exists(path)) {
            bool isLast = (i == paths.size() - 1);
            openImageFile(path, isLast);
        }
    }
    m_isRestoringSession = false;
    updateSnapshotTimeline();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    const QMimeData *mimeData = event->mimeData();
    if (mimeData && mimeData->hasUrls()) {
        for (const QUrl& url : mimeData->urls()) {
            QString path = url.toLocalFile();
            if (!path.isEmpty()) {
                openImageFile(path);
            }
        }
        event->acceptProposedAction();
    }
}

void MainWindow::onSettings() {
    SettingsDialog dialog(this);
    dialog.exec();
}

void MainWindow::updateViewer(ImageTab *tab) {
    if (m_currentState == ContentState::Empty) {
        // Clear the viewer so there's no stale content
        m_viewerState->viewer()->clear();
        m_currentTabInView = nullptr;
        return;
    }

    if (!tab) {
        tab = currentTab();
    }

    if (!tab)
        return;

    int snapshotIdx = tab->session()->currentSnapshotIndex();
    if (m_currentTabInView == tab && m_currentVersionInView == snapshotIdx) {
        return; // Already rendering this snapshot
    }

    // Save state of the current tab
    if (m_currentTabInView) {
        m_viewerController->syncViewerToSession();
    }

    if (m_currentTabInView != tab) {
        m_viewerState->viewer()->clear();
    }

    // Restore state for the new tab
    m_viewerController->setActiveSession(tab->session());
    m_viewerController->syncSessionToViewer();

    bool isSnapshot = false;
    if (!tab->session()->isCurrentImageSelected()) {
        if (auto seq = tab->session()->getReconstructionSequence()) {
            m_lastBaseIdx = seq->baseIdx;
            isSnapshot = true;
        }
    }

    if (!isSnapshot) {
        const auto& image = tab->diskImage();
        if (!image.isNull()) {
            m_viewerState->viewer()->setImage(image, true);
        }
        m_lastBaseIdx = -1;
    }

    m_effectsController->setTargetState(tab->session());
    QSize dims = tab->session()->dimensions();
    m_viewerState->statusBar()->setDimensions(dims.width(), dims.height());

    // Update timestamp in status bar
    int idx = tab->session()->currentSnapshotIndex();
    if (idx >= 0 && !tab->session()->isCurrentImage(idx)) {
        const auto& snapshots = tab->session()->snapshots();
        m_viewerState->statusBar()->setTimestamp(
            snapshots[idx].timestamp.toString("MMMM d, yyyy h:mm:ss AP"));
    } else {
        m_viewerState->statusBar()->setTimestamp(tr("Current"));
    }

    m_viewerState->statusBar()->setZoom(m_viewerState->viewer()->zoomPercentage());
    m_currentTabInView = tab;
}

void MainWindow::updateState() {
    ContentState newState = (m_tabBar->count() > 0) ? ContentState::Viewer : ContentState::Empty;

    if (newState != m_currentState) {
        m_currentState = newState;
        switchContentState(m_currentState);

        if (m_currentState == ContentState::Empty) {
            m_effectsController->setTargetState(nullptr);
        }
        updateMenuBar();
    }
}

void MainWindow::onSnapshotSelected(int index) {
    auto *tab = currentTab();
    if (!tab)
        return;
    tab->selectSnapshot(index);

    // Update the version tracker so updateViewer knows something changed
    if (index == static_cast<int>(tab->session()->snapshots().size())) {
        m_currentVersionInView = -1;
    } else {
        m_currentVersionInView = index;
    }

    m_viewerState->snapshotTimeline()->setSelectedIndex(tab->session()->currentSnapshotIndex());
    updateViewer();
    updateMenuBar();
}

void MainWindow::onSnapshotDeletionRequested(int index) {
    auto *tab = currentTab();
    if (!tab)
        return;

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Delete Snapshot"));
    msgBox.setText(tr("Delete snapshot %1?").arg(index + 1));
    msgBox.setIcon(QMessageBox::Warning);

    QFile qssFile(":/qss/messagebox.qss");
    if (qssFile.open(QIODevice::ReadOnly)) {
        msgBox.setStyleSheet(qssFile.readAll());
    }

    QPushButton *deleteButton = msgBox.addButton(tr("Delete"), QMessageBox::AcceptRole);
    QPushButton *cancelButton = msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);

    msgBox.setDefaultButton(cancelButton);
    msgBox.exec();

    if (msgBox.clickedButton() == deleteButton) {
        tab->deleteSnapshot(index);
        updateSnapshotTimeline();
        updateViewer();
    }
}

void MainWindow::switchContentState(ContentState state) {
    switch (state) {
        case ContentState::Empty:
            m_contentStack->setCurrentWidget(m_emptyState);
            break;
        case ContentState::Viewer:
            m_contentStack->setCurrentWidget(m_viewerState);
            break;
    }
}
