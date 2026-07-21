#include "ui/mainwindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleHints>
#include <QVBoxLayout>

#include "config/appsettings.h"
#include "ui/dialogs/settingsdialog.h"
#include "ui/effectscontroller.h"
#include "ui/emptystate.h"
#include "ui/imagetab.h"
#include "ui/notificationbar.h"
#include "ui/snapshottimeline.h"
#include "ui/tabbar.h"
#include "ui/viewerstate.h"
#include "ui/vkimageviewer.h"

static const int c_defaultWidth = 1000;
static const int c_defaultHeight = 700;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_tabBar(nullptr), m_notification(nullptr), m_viewerState(nullptr),
      m_emptyState(nullptr), m_session(m_settings) {
    m_effectsController = new EffectsController(this);
    setupMenu();
    setupUi();
    m_session.load();
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent *event) {
    m_session.save(collectOpenPaths());
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

    QWidget *central = new QWidget(this);
    auto    *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    // Tab bar
    m_tabBar = new TabBar(central);
    connect(m_tabBar, &QTabWidget::tabCloseRequested, this, &MainWindow::onCloseTab);
    connect(m_tabBar, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    centralLayout->addWidget(m_tabBar, 0);

    // Stacked widget
    m_contentStack = new QStackedWidget(central);

    // Viewer state: thumbnail strip + viewer + nav bar
    m_viewerState = new ViewerState(m_contentStack);
    connect(m_viewerState->snapshotTimeline(),
            &SnapshotTimeline::snapshotSelected,
            this,
            &MainWindow::onSnapshotSelected);

    // Empty state
    m_emptyState = new EmptyState(m_contentStack);

    // Stack states
    m_contentStack->addWidget(m_emptyState);
    m_contentStack->addWidget(m_viewerState);
    m_contentStack->setCurrentWidget(m_emptyState);
    centralLayout->addWidget(m_contentStack, 1);

    // Controllers
    auto *uiAdapter = new EffectsUIAdapter(m_actionGrayscale, m_actionMirror);
    m_effectsController->setup(m_viewerState->viewer(), uiAdapter);

    // Connect empty state actions
    connect(m_emptyState, &EmptyState::openRequested, this, &MainWindow::onFileOpen);
    connect(m_emptyState, &EmptyState::settingsRequested, this, &MainWindow::onSettings);

    // Notification bar
    m_notification = new NotificationBar(central);
    centralLayout->addWidget(m_notification, 0);

    setCentralWidget(central);

    updateViewer();
}

void MainWindow::setupMenu() {
    m_fileMenu = menuBar()->addMenu("&File");
    m_actionOpen = m_fileMenu->addAction("&Open Image...");
    m_actionOpen->setShortcut(QKeySequence::Open);
    connect(m_actionOpen, &QAction::triggered, this, &MainWindow::onFileOpen);

    m_actionSaveSnapshot = m_fileMenu->addAction("&Save Snapshot");
    m_actionSaveSnapshot->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    connect(m_actionSaveSnapshot, &QAction::triggered, this, &MainWindow::onSaveSnapshot);

    m_actionCloseTab = m_fileMenu->addAction("Close &Tab");
    m_actionCloseTab->setShortcut(QKeySequence::Close);
    connect(m_actionCloseTab, &QAction::triggered, this, &MainWindow::onCloseCurrentTab);

    m_fileMenu->addSeparator();

    m_actionExit = m_fileMenu->addAction("E&xit");
    m_actionExit->setShortcut(QKeySequence::Quit);
    connect(m_actionExit, &QAction::triggered, this, &QWidget::close);

    m_editMenu = menuBar()->addMenu("&Edit");
    m_actionSettings = m_editMenu->addAction("&Settings");
    connect(m_actionSettings, &QAction::triggered, this, &MainWindow::onSettings);

    m_viewMenu = menuBar()->addMenu("&View");
    m_actionFitWindow = m_viewMenu->addAction("&Fit to Window");
    m_actionFitWindow->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    connect(m_actionFitWindow, &QAction::triggered, this, &MainWindow::onFitToWindow);

    m_actionResetZoom = m_viewMenu->addAction("Reset &Zoom");
    m_actionResetZoom->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(m_actionResetZoom, &QAction::triggered, this, &MainWindow::onResetZoom);

    m_effectsMenu = menuBar()->addMenu("&Effects");
    m_actionGrayscale = m_effectsMenu->addAction("&Grayscale");
    m_actionGrayscale->setCheckable(true);
    m_actionGrayscale->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(m_actionGrayscale, &QAction::triggered, this, [this]() {
        m_effectsController->toggleGrayscale();
    });

    m_actionMirror = m_effectsMenu->addAction("&Mirror");
    m_actionMirror->setCheckable(true);
    m_actionMirror->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    connect(m_actionMirror, &QAction::triggered, this, [this]() {
        m_effectsController->toggleMirror();
    });

    updateMenuBar();
}

void MainWindow::updateMenuBar() {
    // Common actions always enabled and visible
    m_actionOpen->setEnabled(true);
    m_actionSettings->setEnabled(true);
    m_actionExit->setEnabled(true);
    m_fileMenu->menuAction()->setVisible(true);
    m_editMenu->menuAction()->setVisible(true);

    switch (m_currentState) {
        case ContentState::Empty:
            m_actionSaveSnapshot->setVisible(false);
            m_actionCloseTab->setVisible(false);
            m_actionFitWindow->setVisible(false);
            m_actionResetZoom->setVisible(false);
            m_actionGrayscale->setVisible(false);
            m_actionMirror->setVisible(false);

            m_viewMenu->menuAction()->setVisible(false);
            m_effectsMenu->menuAction()->setVisible(false);
            break;

        case ContentState::Viewer:
            m_actionSaveSnapshot->setVisible(true);
            m_actionCloseTab->setVisible(true);
            m_actionFitWindow->setVisible(true);
            m_actionResetZoom->setVisible(true);
            m_actionGrayscale->setVisible(true);
            m_actionMirror->setVisible(true);

            m_viewMenu->menuAction()->setVisible(true);
            m_effectsMenu->menuAction()->setVisible(true);
            break;
    }

    // Clear the "Tools" menu as we now use dedicated menus
    // (Removed m_contextMenu as it is no longer used)
}

void MainWindow::onCloseCurrentTab() {
    onCloseTab(m_tabBar->currentIndex());
}

void MainWindow::onFitToWindow() {
    viewer()->fitToWindow();
}

void MainWindow::onResetZoom() {
    viewer()->resetZoom();
}

void MainWindow::onFileOpen() {
    QString path =
        QFileDialog::getOpenFileName(this, tr("Open Image"), "", AppSettings::fileFilter());
    if (path.isEmpty())
        return;
    openImageFile(path);
}

void MainWindow::onSaveSnapshot() {
    auto *tab = currentTab();
    if (tab) {
        tab->saveSnapshot();
    }
}

void MainWindow::openImageFile(const QString& path) {
    // Check if already open
    if (auto *existing = m_tabPaths.value(path)) {
        m_tabBar->setCurrentWidget(existing);
        return;
    }

    auto *tab = new ImageTab(this);
    connect(tab, &ImageTab::statusMessage, this, [this](const QString& msg, int timeout) {
        notify(msg, timeout);
    });
    connect(tab, &ImageTab::snapshotChanged, this, [this, tab](int /*index*/) {
        if (m_tabBar->currentWidget() == tab) {
            updateViewer();
        }
    });
    connect(tab, &ImageTab::snapshotsChanged, this, [this, tab]() {
        if (m_tabBar->currentWidget() == tab) {
            updateSnapshotTimeline();
        }
    });

    QString displayName = QFileInfo(path).fileName();
    int     index = m_tabBar->addTab(tab, displayName);
    tab->openImage(path);
    setTabThumbnail(index);
    m_tabPaths.insert(path, tab);

    m_tabBar->setCurrentWidget(tab);
    updateSnapshotTimeline();
    updateViewer();
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

    m_effectsController->setTargetState(tab);
}

void MainWindow::onTabChanged(int index) {
    if (index < 0) {
        qDebug() << "[MainWindow] index passed was negative";
        return;
    }

    updateSnapshotTimeline();
    updateViewer();
    updateState();
    setTabThumbnail(index);
}

void MainWindow::setTabThumbnail(int index) {
    if (index < 0 || index >= m_tabBar->count()) {
        qDebug() << "[MainWindow] index" << index << "out of bounds (count:" << m_tabBar->count()
                 << ")";
        return;
    }

    auto *tab = qobject_cast<ImageTab *>(m_tabBar->widget(index));

    QPixmap thumb = tab->thumbnail(40);
    if (!thumb.isNull())
        m_tabBar->setTabIcon(
            index, QIcon(thumb.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
}

ImageTab *MainWindow::currentTab() {
    return qobject_cast<ImageTab *>(m_tabBar->currentWidget());
}

void MainWindow::notify(const QString& msg, int timeoutMs) {
    if (timeoutMs == -1) {
        m_notification->notify(msg);
    } else {
        m_notification->notify(msg, timeoutMs);
    }
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);

    if (!AppSettings::restoreSession())
        return;

    // Defer session restore until showEvent() so that the layout is fully
    // settled and QWindowContainer has its real size for proper rendering.
    for (const QString& path : m_session.restorePaths()) {
        if (QFile::exists(path))
            openImageFile(path);
    }
}

void MainWindow::onSettings() {
    SettingsDialog dialog(this);
    dialog.exec();
}

void MainWindow::updateViewer() {
    if (m_currentState == ContentState::Empty) {
        // Clear the viewer so there's no stale content
        m_viewerState->viewer()->clear();
        m_currentTabInView = nullptr;
        return;
    }

    auto *tab = currentTab();
    if (!tab)
        return;

    // Save state of the current tab
    if (m_currentTabInView) {
        m_currentTabInView->setViewState(m_viewerState->viewer()->getViewState());
    }

    const auto& image = tab->currentImage();
    if (!image.isNull()) {
        // Restore state for the new tab
        m_viewerState->viewer()->setViewState(tab->viewState());
        m_viewerState->viewer()->setImage(image, true);
        m_effectsController->setTargetState(tab);
    }

    m_viewerState->statusBar()->setZoom(m_viewerState->viewer()->ZoomPercentage());
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
    m_viewerState->snapshotTimeline()->setSelectedIndex(tab->currentSnapshotIndex());
    updateViewer();
}

void MainWindow::updateSnapshotTimeline() {
    auto *tab = currentTab();
    if (!tab) {
        return;
    }

    auto [thumbs, labels] = tab->snapshotThumbnails(48);
    m_viewerState->snapshotTimeline()->setThumbnails(thumbs, labels);
    m_viewerState->snapshotTimeline()->setSelectedIndex(tab->currentSnapshotIndex());
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
