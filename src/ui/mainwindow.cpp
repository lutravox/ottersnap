#include "ui/mainwindow.h"
#include "controllers/effectscontroller.h"
#include "controllers/imagesessioncontroller.h"
#include "controllers/viewercontroller.h"
#include "core/imagesession.h"
#include "core/snapshotmanager.h"
#include "core/thumbnailcache.h"
#include "ui/dialogutils.h"

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
#include <QShortcut>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleHints>
#include <QVBoxLayout>
#include <QtConcurrent>

#include "config/appsettings.h"
#include "controllers/effectscontroller.h"
#include "core/vulkancontext.h"
#include "ui/dialogs/aboutdialog.h"
#include "ui/dialogs/settingsdialog.h"
#include "ui/dialogs/snapshotmanagerdialog.h"
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
    m_settingsController = new AppSettingsController(this);
    m_sessionController = new ImageSessionController(m_settingsController, this);
    m_effectsController = new EffectsController(this);
    m_viewerController = new ViewerController(m_settingsController, this);
    m_snapshotController = new SnapshotTimelineController(this);
    m_colorInfoController = new ColorInfoController(this);

    // Link controllers to session coordinator
    m_snapshotController->setSessionController(m_sessionController);
    m_colorInfoController->setSessionController(m_sessionController);
    m_viewerController->setSessionController(m_sessionController);

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

MainWindow::~MainWindow() {
    qDeleteAll(m_toolShortcuts);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    QStringList openPaths;
    if (m_tabBar) {
        for (int i = 0; i < m_tabBar->count(); ++i) {
            if (auto *tab = qobject_cast<ImageTab *>(m_tabBar->widget(i))) {
                openPaths.append(tab->filePath());
            }
        }
    }
    m_session.save(openPaths);

    //  Cleanup tab session resources
    if (m_tabBar) {
        for (int i = 0; i < m_tabBar->count(); ++i) {
            auto *tab = qobject_cast<ImageTab *>(m_tabBar->widget(i));
            if (tab) {
                m_sessionController->closeSession(tab->filePath());
            }
        }
    }

    QMainWindow::closeEvent(event);
}

// Remove the now-redundant collectOpenPaths method
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

    m_viewerState->snapshotTimeline()->setController(m_snapshotController);

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

    connect(m_viewerState->snapshotTimeline(),
            &SnapshotTimeline::secondarySnapshotSelected,
            m_viewerController,
            &ViewerController::setSecondarySnapshot);

    // Link controller signals to main window/viewer
    connect(m_snapshotController,
            &SnapshotTimelineController::snapshotDeletionRequested,
            this,
            &MainWindow::onSnapshotDeletionRequested);

    connect(m_snapshotController,
            &SnapshotTimelineController::createSnapshotRequested,
            this,
            &MainWindow::onSaveSnapshot);

    connect(m_snapshotController,
            &SnapshotTimelineController::secondarySnapshotSelected,
            m_viewerController,
            &ViewerController::setSecondarySnapshot);
    auto *uiAdapter = new EffectsUIAdapter(m_actionGrayscale, m_actionMirror);
    m_effectsController->setup(m_viewerState->viewer());
    m_effectsController->addUI(uiAdapter);
    m_effectsController->addUI(m_viewerState->toolbar());
    m_viewerState->toolbar()->setup(m_effectsController, m_viewerController);
    m_viewerController->setViewer(m_viewerState->viewer());
    m_viewerState->setToolbarVisible(m_viewerController->isToolbarVisible());

    // Connect empty state actions
    connect(m_emptyState, &EmptyState::openRequested, this, &MainWindow::onFileOpen);
    connect(m_emptyState, &EmptyState::settingsRequested, this, &MainWindow::onSettings);

    // Notifications
    m_notificationManager = new NotificationManager(this);

    // Connections
    connect(m_sessionController,
            &ImageSessionController::sessionInvalidated,
            this,
            &MainWindow::onSessionInvalidated);

    connect(m_viewerState->viewer(),
            &VkImageViewer::resetViewRequested,
            this,
            &MainWindow::onResetView);

    connect(m_viewerState->viewer(),
            &VkImageViewer::actualSizeRequested,
            this,
            &MainWindow::onActualSize);

    connect(m_viewerState->viewer(), &VkImageViewer::zoomInRequested, this, &MainWindow::onZoomIn);

    connect(
        m_viewerState->viewer(), &VkImageViewer::zoomOutRequested, this, &MainWindow::onZoomOut);

    connect(m_viewerState->viewer(),
            &VkImageViewer::scaleWithWindowToggled,
            this,
            &MainWindow::onToggleScaleWithWindow);

    connect(m_viewerState->viewer(),
            &VkImageViewer::resetEffectsRequested,
            this,
            &MainWindow::onResetEffects);

    connect(m_viewerState->viewer(),
            &VkImageViewer::viewportResized,
            m_viewerController,
            &ViewerController::handleViewportResize);

    connect(m_viewerState->viewer(), &VkImageViewer::colorPicked, this, &MainWindow::onColorPicked);

    connect(m_viewerState->statusBar(),
            &StatusBar::colorInfoToggled,
            this,
            &MainWindow::onColorInfoToggled);

    connect(m_colorInfoController,
            &ColorInfoController::colorSelected,
            this,
            &MainWindow::onColorPicked);

    connect(m_viewerController,
            &ViewerController::toolbarVisibilityToggled,
            m_viewerState,
            &ViewerState::setToolbarVisible);

    connect(m_viewerController,
            &ViewerController::secondarySnapshotChanged,
            m_viewerState,
            &ViewerState::setSecondarySnapshotId);

    connect(m_viewerController,
            &ViewerController::secondarySnapshotChanged,
            m_viewerState->toolbar(),
            &ViewerToolbar::updateToolStates);

    connect(m_viewerController,
            &ViewerController::secondarySnapshotChanged,
            this,
            &MainWindow::updateMenuBar);

    connect(m_viewerController,
            &ViewerController::stateChanged,
            m_viewerState->toolbar(),
            &ViewerToolbar::updateToolStates);

    connect(m_viewerController, &ViewerController::stateChanged, this, &MainWindow::updateMenuBar);

    // Register shortcuts for toolbar tools
    if (m_viewerState && m_viewerState->toolbar()) {
        for (const auto& tw : m_viewerState->toolbar()->tools()) {
            QKeySequence shortcut = tw.tool->shortcut();
            if (!shortcut.isEmpty()) {
                QShortcut *s = new QShortcut(shortcut, this);
                connect(s, &QShortcut::activated, this, [this, name = tw.tool->name()]() {
                    if (m_viewerState && m_viewerState->toolbar()) {
                        m_viewerState->toolbar()->activateTool(name);
                    }
                });
                m_toolShortcuts.append(s);
            }
        }
    }

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

    m_actionExportSnapshot = m_fileMenu->addAction(tr("&Export Snapshot As..."));
    connect(m_actionExportSnapshot, &QAction::triggered, this, &MainWindow::onExportSnapshot);

    m_actionExportHistory = m_fileMenu->addAction(tr("&Export History..."));
    connect(m_actionExportHistory, &QAction::triggered, this, &MainWindow::onExportHistory);

    m_actionImportHistory = m_fileMenu->addAction(tr("&Import History..."));
    connect(m_actionImportHistory, &QAction::triggered, this, &MainWindow::onImportHistory);

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
    m_actionSaveSnapshot = m_editMenu->addAction(tr("&Create Snapshot"));
    m_actionSaveSnapshot->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    connect(m_actionSaveSnapshot, &QAction::triggered, this, &MainWindow::onSaveSnapshot);

    m_editMenu->addSeparator();

    m_actionDeleteSnapshot = m_editMenu->addAction(tr("&Delete Selected Snapshot"));
    m_actionDeleteSnapshot->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(m_actionDeleteSnapshot,
            &QAction::triggered,
            this,
            &MainWindow::onDeleteCurrentSnapshotRequested);

    m_actionDeleteAllSnapshots = m_editMenu->addAction(tr("Delete &All Snapshots"));
    connect(m_actionDeleteAllSnapshots,
            &QAction::triggered,
            this,
            &MainWindow::onDeleteAllSnapshotsRequested);

    m_editMenu->addSeparator();

    m_actionManageSnapshots = m_editMenu->addAction(tr("Manage Snapshots..."));
    connect(m_actionManageSnapshots, &QAction::triggered, this, &MainWindow::onManageSnapshots);

    m_editMenu->addSeparator();

    m_actionSettings = m_editMenu->addAction(tr("&Settings"));
    connect(m_actionSettings, &QAction::triggered, this, &MainWindow::onSettings);

    m_viewMenu = menuBar()->addMenu(tr("&View"));
    m_actionScaleWithWindow = m_viewMenu->addAction(tr("&Scale with Window"));
    m_actionScaleWithWindow->setCheckable(true);
    m_actionScaleWithWindow->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    connect(
        m_actionScaleWithWindow, &QAction::triggered, this, &MainWindow::onToggleScaleWithWindow);

    m_actionToggleToolbar = m_viewMenu->addAction(tr("&Show Toolbar"));
    m_actionToggleToolbar->setCheckable(true);
    m_actionToggleToolbar->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(m_actionToggleToolbar, &QAction::triggered, this, &MainWindow::onToggleToolbar);

    m_actionSwap = m_viewMenu->addAction(tr("&Swap Comparison"));
    connect(m_actionSwap, &QAction::triggered, this, &MainWindow::onSwap);

    m_actionResetView = m_viewMenu->addAction(tr("Reset &View"));
    m_actionResetView->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(
        m_actionResetView, &QAction::triggered, m_viewerController, &ViewerController::fitToWindow);

    m_actionActualSize = m_viewMenu->addAction(tr("&Actual Size (100%)"));
    m_actionActualSize->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
    connect(m_actionActualSize, &QAction::triggered, this, &MainWindow::onActualSize);

    m_viewMenu->addSeparator();

    m_actionZoomIn = m_viewMenu->addAction(tr("Zoom &In"));
    m_actionZoomIn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal));
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
    connect(m_actionGrayscale, &QAction::triggered, this, [this]() {
        m_effectsController->toggleGrayscale();
    });

    m_actionMirror = m_effectsMenu->addAction(tr("&Mirror"));
    m_actionMirror->setCheckable(true);
    connect(m_actionMirror, &QAction::triggered, this, [this]() {
        m_effectsController->toggleMirror();
    });

    m_effectsMenu->addSeparator();
    m_actionResetEffects = m_effectsMenu->addAction(tr("Reset All &Effects"));
    connect(
        m_actionResetEffects, &QAction::triggered, m_effectsController, &EffectsController::reset);

    m_helpMenu = menuBar()->addMenu(tr("&Help"));
    m_actionAbout = m_helpMenu->addAction(tr("&About"));
    connect(m_actionAbout, &QAction::triggered, this, &MainWindow::onAbout);

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

        // Disable "Save Snapshot of Current" if in Snapshot Only mode
        m_actionSaveSnapshot->setEnabled(!tab->session()->isSnapshotOnly());
        m_actionDeleteAllSnapshots->setEnabled(!snapshots.isEmpty());
        m_actionExportHistory->setEnabled(!snapshots.isEmpty());
        m_actionImportHistory->setEnabled(true);
        m_actionManageSnapshots->setEnabled(true);
    } else {
        m_actionDeleteSnapshot->setEnabled(false);
        m_actionSaveSnapshot->setEnabled(false);
        m_actionDeleteAllSnapshots->setEnabled(false);
        m_actionExportHistory->setEnabled(false);
        m_actionImportHistory->setEnabled(false);
        m_actionManageSnapshots->setEnabled(true);
    }

    m_actionToggleToolbar->setChecked(m_viewerController->isToolbarVisible());

    switch (m_currentState) {
        case ContentState::Empty:
            m_actionSaveSnapshot->setVisible(false);
            m_actionExportHistory->setVisible(false);
            m_actionImportHistory->setVisible(false);
            m_actionDeleteSnapshot->setVisible(false);
            m_actionDeleteAllSnapshots->setVisible(false);
            m_actionCloseTab->setVisible(false);
            m_actionCloseAllTabs->setVisible(false);
            m_actionScaleWithWindow->setVisible(false);
            m_actionResetView->setVisible(false);
            m_actionGrayscale->setVisible(false);
            m_actionMirror->setVisible(false);
            m_actionSwap->setVisible(false);

            m_viewMenu->menuAction()->setVisible(false);
            m_effectsMenu->menuAction()->setVisible(false);
            break;

        case ContentState::Viewer:
            m_actionSaveSnapshot->setVisible(true);
            m_actionExportHistory->setVisible(true);
            m_actionImportHistory->setVisible(true);
            m_actionDeleteSnapshot->setVisible(true);
            m_actionDeleteAllSnapshots->setVisible(true);
            m_actionCloseTab->setVisible(true);
            m_actionCloseAllTabs->setVisible(true);
            m_actionScaleWithWindow->setVisible(true);
            m_actionScaleWithWindow->setChecked(m_viewerController->isScaleWithWindowEnabled());
            m_actionResetView->setVisible(true);
            m_actionGrayscale->setVisible(true);
            m_actionMirror->setVisible(true);
            m_actionSwap->setVisible(true);

            m_viewMenu->menuAction()->setVisible(true);
            m_effectsMenu->menuAction()->setVisible(true);
            break;
    }

    if (m_viewerController) {
        m_actionSwap->setEnabled(m_viewerController->canSwap());
    } else {
        m_actionSwap->setEnabled(false);
    }
}

void MainWindow::onCloseCurrentTab() {
    onCloseTab(m_tabBar->currentIndex());
}

void MainWindow::onCloseAllTabs() {
    while (m_tabBar->count() > 0) {
        onCloseTab(0);
    }
}

void MainWindow::onToggleScaleWithWindow() {
    bool enabled = !m_viewerController->isScaleWithWindowEnabled();
    m_viewerController->setScaleWithWindowEnabled(enabled);
    m_actionScaleWithWindow->setChecked(enabled);
}

void MainWindow::onToggleToolbar() {
    bool visible = m_actionToggleToolbar->isChecked();
    m_viewerController->setToolbarVisible(visible);
}

void MainWindow::onSwap() {
    if (m_viewerController) {
        m_viewerController->swapPrimaryAndSecondary();
    }
}

void MainWindow::onAbout() {
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::onColorPicked(const QColor& color) {
    m_viewerState->statusBar()->setColor(color);
    if (sender() != m_colorInfoController) {
        m_colorInfoController->setPickedColor(color);
    }
}

void MainWindow::onColorInfoToggled(bool checked) {
    m_colorInfoController->setVisible(checked);
}

void MainWindow::onSessionColorClustersChanged() {
    if (auto *tab = currentTab()) {
        m_colorInfoController->setClusters(tab->session()->colorClusters());
    }
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

void MainWindow::onExportHistory() {
    auto *tab = currentTab();
    if (!tab)
        return;

    QString baseName = QFileInfo(tab->filePath()).baseName();
    QString suggestedName = QString("%1_history.zip").arg(baseName);

    QString path = QFileDialog::getSaveFileName(
        this, tr("Export Snapshot History"), suggestedName, tr("Snapshot History (*.zip)"));
    if (path.isEmpty())
        return;

    if (SnapshotManager::exportHistory(tab->filePath(), path)) {
        notify(tr("Snapshot history exported successfully."));
    } else {
        notify(tr("Failed to export snapshot history."));
    }
}

void MainWindow::onImportHistory() {
    auto *tab = currentTab();
    if (!tab) {
        notify(tr("No image open to import history for."));
        return;
    }

    QString path = QFileDialog::getOpenFileName(
        this, tr("Import Snapshot History"), "", tr("Snapshot History (*.zip)"));
    if (path.isEmpty())
        return;

    int duplicates = 0;
    if (SnapshotManager::importHistory(tab->filePath(), path, &duplicates)) {
        QString msg = tr("Snapshot history imported successfully.");
        if (duplicates > 0) {
            QString dupMsg = (duplicates == 1)
                ? tr("%1 duplicate snapshot was skipped.").arg(duplicates)
                : tr("%1 duplicate snapshots were skipped.").arg(duplicates);
            msg += " " + dupMsg;
        }
        notify(msg);
        tab->session()->rebuildSnapshotList();
        updateSnapshotTimeline();
        updateViewer();
    } else {
        notify(tr("Failed to import snapshot history."));
    }
}

void MainWindow::onExportSnapshot() {
    auto *tab = currentTab();
    if (!tab)
        return;

    QString baseName = QFileInfo(tab->filePath()).baseName();
    int     snapshotIdx = tab->session()->currentSnapshotIndex();
    QString suggestedName;
    if (snapshotIdx >= 0) {
        suggestedName = QString("%1_snapshot_%2").arg(baseName).arg(snapshotIdx + 1);
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
    if (snapshotIdx >= 0) {
        params.isSnapshot = true;
        auto session = tab->session();
        params.seq = session ? session->getReconstructionSequence(snapshotIdx) : std::nullopt;
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

    // Convert index to UUID
    const auto& snapshots = tab->session()->snapshots();
    if (index < 0 || index >= static_cast<int>(snapshots.size()))
        return;

    onSnapshotDeletionRequested(snapshots[index].uuid);
}

void MainWindow::setupTabConnections(ImageTab *tab) {
    connect(tab, &ImageTab::statusMessage, this, [this](const QString& msg, int timeout) {
        notify(msg, timeout);
    });
    connect(tab, &ImageTab::thumbnailUpdated, this, [this, tab](int index, const QPixmap& pixmap) {
        if (tab == currentTab()) {
            m_snapshotController->updateThumbnail(index, pixmap);
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
    connect(tab, &ImageTab::snapshotCreated, this, [this, tab](const QUuid& uuid) {
        if (tab == currentTab()) {
            m_snapshotController->markSnapshotAsNew(uuid);
        }
    });

    connect(tab->session(),
            &ImageSession::colorClustersChanged,
            this,
            &MainWindow::onSessionColorClustersChanged);
}

ImageTab *MainWindow::openImageFile(const QString& path, bool setAsCurrent) {
    ImageSession *session = m_sessionController->openImage(path, false);

    // file wasn't found, open in snapshot only mode
    if (!session) {
        session = m_sessionController->openImage(path, true);
    }

    if (!session)
        return nullptr;

    // Check if already open in a tab
    if (auto *existing = m_tabPaths.value(path)) {
        if (setAsCurrent) {
            m_tabBar->setCurrentWidget(existing);
        }
        return existing;
    }

    auto *tab = new ImageTab(this, session);
    setupTabConnections(tab);

    QString displayName = QFileInfo(path).fileName();
    int     index = m_tabBar->addTab(tab, displayName);
    m_tabBar->setTabToolTip(index, path);
    tab->notifyImageOpened();
    setTabThumbnail(index);
    m_tabPaths.insert(path, tab);

    if (setAsCurrent || m_isRestoringSession) {
        if (setAsCurrent) {
            m_tabBar->setCurrentWidget(tab);
        }
        updateViewer(tab);
        m_viewerController->fitToWindow();
    }

    return tab;
}

void MainWindow::onCloseTab(int index) {
    if (index < 0 || index >= m_tabBar->count())
        return;

    auto *tab = qobject_cast<ImageTab *>(m_tabBar->widget(index));
    if (tab) {
        QString path = tab->filePath();
        m_tabPaths.remove(path);
        m_sessionController->closeSession(path);
        disconnect(tab, &ImageTab::statusMessage, this, nullptr);
        disconnect(tab, &ImageTab::effectsChanged, this, nullptr);
        disconnect(tab, &ImageTab::snapshotChanged, this, nullptr);
        tab->deleteLater();
    }

    m_tabBar->removeTab(index);

    updateState();
    auto *nextTab = currentTab();
    if (nextTab) {
        m_viewerController->requestSessionChange(nextTab->session(),
                                                 nextTab->session()->currentSnapshotIndex());
        updateViewer(nextTab);
    } else {
        m_viewerController->requestSessionChange(nullptr, -1);
        updateViewer();
    }
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

    auto *tab = qobject_cast<ImageTab *>(m_tabBar->widget(index));
    if (tab) {
        m_viewerController->requestSessionChange(tab->session(),
                                                 tab->session()->currentSnapshotIndex());
        updateViewer(tab);
        updateState();
        updateMenuBar();
        updateSnapshotTimeline();
    }

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
    m_snapshotController->selectSnapshot(tab->session()->currentSnapshotIndex());
}

void MainWindow::onSnapshotChanged(int index) {
    auto *tab = currentTab();
    if (!tab)
        return;

    updateViewer(tab);
    syncTimelineSelection();
}

void MainWindow::updateSnapshotTimeline() {
    // No need to update the timeline repeatedly if restoring a bunch of tabs
    if (m_isRestoringSession)
        return;

    auto *tab = currentTab();
    if (!tab) {
        m_sessionController->setActiveSession(nullptr);
        return;
    }

    m_snapshotController->updateModel();
    m_snapshotController->selectSnapshot(tab->session()->currentSnapshotIndex());
    m_viewerState->setSecondarySnapshotId(tab->session()->secondarySnapshotId());
    m_viewerState->snapshotTimeline()->setCreateButtonEnabled(!tab->session()->isSnapshotOnly());
    m_viewerState->setSnapshotOnlyIndicator(tab->session()->isSnapshotOnly());
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
        bool           isLast = (i == paths.size() - 1);
        openImageFile(path, isLast);
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

void MainWindow::onManageSnapshots() {
    SnapshotManagerDialog dialog(this);
    connect(
        &dialog, &SnapshotManagerDialog::snapshotChanged, this, [this](const QString& imagePath) {
            m_sessionController->notifySnapshotChanged(imagePath);
            updateSnapshotTimeline();
        });
    connect(&dialog,
            &SnapshotManagerDialog::openSnapshotRequested,
            this,
            [this, &dialog](const QString& path, const QUuid& uuid) {
                onOpenSnapshotRequested(path, uuid);
                dialog.accept();
            });
    dialog.exec();
}

void MainWindow::onSessionInvalidated(const QString& filePath) {
    if (auto *tab = m_tabPaths.value(filePath)) {
        int index = m_tabBar->indexOf(tab);
        if (index != -1) {
            onCloseTab(index);
        }
    }
}

void MainWindow::onOpenSnapshotRequested(const QString& path, const QUuid& uuid) {
    if (m_tabPaths.contains(path)) {
        ImageTab *tab = m_tabPaths.value(path);
        tab->session()->selectSnapshot(uuid);
        m_tabBar->setCurrentWidget(tab);
        m_viewerController->requestSessionChange(tab->session(), uuid);
        updateViewer(tab);
        m_viewerController->fitToWindow();
        return;
    }

    ImageTab *tab = openImageFile(path, true);
    if (!tab)
        return;

    tab->session()->selectSnapshot(uuid);
    m_viewerController->requestSessionChange(tab->session(), uuid);
    updateViewer(tab);
    m_viewerController->fitToWindow();
}

void MainWindow::updateViewer(ImageTab *tab) {
    if (m_currentState == ContentState::Empty) {
        return;
    }

    if (!tab) {
        tab = currentTab();
    }

    if (!tab) {
        return;
    }

    m_effectsController->setTargetState(tab->session());
    QSize dims = tab->session()->dimensions();
    m_viewerState->statusBar()->setDimensions(dims.width(), dims.height());

    m_colorInfoController->setSessionController(m_sessionController);
    m_colorInfoController->setViewerState(m_viewerState);
    m_colorInfoController->setClusters(tab->session()->colorClusters());

    m_viewerState->statusBar()->setTimestamp(tab->session()->currentImageTimestamp());
    m_viewerState->statusBar()->setZoom(m_viewerState->viewer()->zoomPercentage());
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

void MainWindow::onSnapshotDeletionRequested(const QUuid& uuid) {
    auto *tab = currentTab();
    if (!tab)
        return;

    if (DialogUtils::confirm(
            this, tr("Delete Snapshot"), tr("Delete snapshot?"), tr("Delete"), tr("Cancel"))) {
        tab->deleteSnapshot(uuid);
        updateSnapshotTimeline();
        updateViewer();

        // If it's a snapshot-only session and the last snapshot was deleted, close the tab.
        if (tab->session()->isSnapshotOnly() && tab->session()->snapshots().isEmpty()) {
            onCloseCurrentTab();
        }
    }
}

void MainWindow::onDeleteAllSnapshotsRequested() {
    auto *tab = currentTab();
    if (!tab)
        return;

    const auto& snapshots = tab->session()->snapshots();
    if (snapshots.isEmpty())
        return;

    if (DialogUtils::confirm(
            this,
            tr("Delete All Snapshots"),
            tr("Are you sure you want to delete all %1 snapshots?").arg(snapshots.size()),
            tr("Delete All"),
            tr("Cancel"))) {
        tab->deleteAllSnapshots();
        updateSnapshotTimeline();
        updateViewer();

        // If it's a snapshot-only session and all snapshots were deleted, close the tab.
        if (tab->session()->isSnapshotOnly() && tab->session()->snapshots().isEmpty()) {
            onCloseCurrentTab();
        }
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
