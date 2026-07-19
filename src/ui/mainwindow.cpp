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
#include "ui/imagetab.h"
#include "ui/mainmenu.h"
#include "ui/notificationbar.h"
#include "ui/tabbar.h"
#include "ui/thumbnailstrip.h"
#include "ui/viewercontainer.h"
#include "ui/vkimageviewer.h"

static const int c_defaultWidth = 1000;
static const int c_defaultHeight = 700;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_tabBar(nullptr), m_notification(nullptr), m_viewerContainer(nullptr),
      m_mainMenu(nullptr) {
    setupMenu();
    setupUi();
    m_session.load();
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent *event) {
    m_session.save(m_tabBar);
    QMainWindow::closeEvent(event);
}

void MainWindow::setupUi() {
    setWindowTitle(AppSettings::applicationName());
    resize(c_defaultWidth, c_defaultHeight);

    QWidget *central = new QWidget(this);
    auto    *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);

    // Tab bar
    m_tabBar = new TabBar(central);
    connect(m_tabBar, &QTabWidget::tabCloseRequested, this, &MainWindow::onCloseTab);
    connect(m_tabBar, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    centralLayout->addWidget(m_tabBar, 0);

    // Stacked widget
    m_contentStack = new QStackedWidget(central);

    // Viewer container: thumbnail strip + viewer + nav bar
    m_viewerContainer = new ViewerContainer(m_contentStack);
    connect(m_viewerContainer->thumbnailStrip(),
            &ThumbnailStrip::thumbnailSelected,
            this,
            &MainWindow::onThumbnailSelected);

    // Main menu
    m_mainMenu = new MainMenu(m_contentStack);
    connect(m_mainMenu, &MainMenu::openRequested, this, &MainWindow::onFileOpen);

    m_contentStack->addWidget(m_mainMenu);
    m_contentStack->addWidget(m_viewerContainer);
    m_contentStack->setCurrentWidget(m_mainMenu);
    centralLayout->addWidget(m_contentStack, 1);

    m_notification = new NotificationBar(central);
    centralLayout->addWidget(m_notification, 0);

    setCentralWidget(central);

    updateViewer();
}

void MainWindow::setupMenu() {
    auto *fileMenu = menuBar()->addMenu("&File");

    m_actionOpen = fileMenu->addAction("&Open Image...");
    m_actionOpen->setShortcut(QKeySequence::Open);
    connect(m_actionOpen, &QAction::triggered, this, &MainWindow::onFileOpen);

    m_actionCloseTab = fileMenu->addAction("Close &Tab");
    m_actionCloseTab->setShortcut(QKeySequence::Close);
    connect(m_actionCloseTab, &QAction::triggered, this, [this]() {
        onCloseTab(m_tabBar->currentIndex());
    });

    fileMenu->addSeparator();

    m_actionExit = fileMenu->addAction("E&xit");
    m_actionExit->setShortcut(QKeySequence::Quit);
    connect(m_actionExit, &QAction::triggered, this, &QWidget::close);

    // Edit menu
    auto *editMenu = menuBar()->addMenu("&Edit");

    m_actionSettings = editMenu->addAction("&Settings");
    connect(m_actionSettings, &QAction::triggered, this, &MainWindow::onSettings);

    // View menu
    auto *viewMenu = menuBar()->addMenu("&View");

    m_actionFitWindow = viewMenu->addAction("&Fit to Window");
    m_actionFitWindow->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    connect(m_actionFitWindow, &QAction::triggered, this, [this]() {
        auto *tab = currentTab();
        if (tab) {
            m_viewerContainer->viewer()->fitToWindow();
        }
    });

    m_actionResetZoom = viewMenu->addAction("Reset &Zoom");
    m_actionResetZoom->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(m_actionResetZoom, &QAction::triggered, this, [this]() {
        auto *tab = currentTab();
        if (tab) {
            m_viewerContainer->viewer()->resetZoom();
        }
    });

    // Modifiers menu
    auto *modMenu = menuBar()->addMenu("&Modifiers");

    m_actionGrayscale = modMenu->addAction("&Grayscale");
    m_actionGrayscale->setCheckable(true);
    m_actionGrayscale->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(m_actionGrayscale, &QAction::triggered, this, &MainWindow::onToggleGrayscale);

    m_actionMirror = modMenu->addAction("&Mirror");
    m_actionMirror->setCheckable(true);
    m_actionMirror->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    connect(m_actionMirror, &QAction::triggered, this, &MainWindow::onToggleMirror);
}

void MainWindow::onFileOpen() {
    QString path =
        QFileDialog::getOpenFileName(this, tr("Open Image"), "", AppSettings::fileFilter());
    if (path.isEmpty())
        return;
    openImageFile(path);
}

void MainWindow::openImageFile(const QString& path) {
    // Check if already open
    if (auto *existing = m_tabPaths.value(path)) {
        m_tabBar->setCurrentWidget(existing);
        return;
    }

    auto *tab = new ImageTab(this);
    connect(tab, &ImageTab::statusMessage, this, [this](const QString& msg) { notify(msg); });
    connect(tab, &ImageTab::modifiersChanged, this, [this, tab](bool g, bool m) {
        if (m_tabBar->currentWidget() == tab) {
            m_actionGrayscale->setChecked(g);
            m_actionMirror->setChecked(m);
            m_viewerContainer->viewer()->setGrayscale(g);
            m_viewerContainer->viewer()->setMirror(m);
        }
    });
    connect(tab, &ImageTab::versionChanged, this, [this, tab](int /*index*/) {
        if (m_tabBar->currentWidget() == tab) {
            updateViewer();
        }
    });

    QString displayName = QFileInfo(path).fileName();
    int     index = m_tabBar->addTab(tab, displayName);
    tab->openImage(path);
    setTabThumbnail(index);
    m_tabPaths.insert(path, tab);

    m_tabBar->setCurrentWidget(tab);
    updateThumbnailStrip();
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
        disconnect(tab, &ImageTab::modifiersChanged, this, nullptr);
        disconnect(tab, &ImageTab::versionChanged, this, nullptr);
    }

    m_tabBar->removeTab(index);

    updateThumbnailStrip();
    updateViewer();
    updateState();
}

void MainWindow::onToggleGrayscale() {
    applyModifiers();
}

void MainWindow::onToggleMirror() {
    applyModifiers();
}

void MainWindow::applyModifiers() {
    auto *tab = currentTab();
    if (!tab)
        return;

    tab->setGrayscale(m_actionGrayscale->isChecked());
    tab->setMirror(m_actionMirror->isChecked());
}

void MainWindow::onTabChanged(int index) {
    if (index < 0) {
        qDebug() << "[MainWindow] index passed was negative";
        return;
    }

    updateThumbnailStrip();
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

void MainWindow::notify(const QString& msg) {
    m_notification->notify(msg);
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
    if (m_tabBar->count() == 0) {
        // Clear the viewer so there's no stale content
        m_viewerContainer->viewer()->clear();
        m_currentTabInView = nullptr;
        return;
    }

    auto *tab = currentTab();
    if (!tab)
        return;

    const auto& image = tab->currentImage();
    if (!image.isNull()) {
        m_viewerContainer->viewer()->setImage(image, false);
        m_viewerContainer->viewer()->setGrayscale(tab->grayscaleEnabled());
        m_viewerContainer->viewer()->setMirror(tab->mirrorEnabled());
    }

    m_viewerContainer->statusBar()->setZoom(m_viewerContainer->viewer()->ZoomPercentage());
    m_currentTabInView = tab;
}

void MainWindow::updateState() {
    if (m_tabBar->count() == 0) {
        m_actionGrayscale->setChecked(false);
        m_actionMirror->setChecked(false);
        switchContentState(ContentState::Empty);
        return;
    }

    switchContentState(ContentState::Viewer);

    auto *tab = currentTab();
    if (!tab)
        return;

    m_actionGrayscale->setChecked(tab->grayscaleEnabled());
    m_actionMirror->setChecked(tab->mirrorEnabled());
}

void MainWindow::onThumbnailSelected(int index) {
    auto *tab = currentTab();
    if (!tab)
        return;
    tab->selectVersion(index);
    m_viewerContainer->thumbnailStrip()->setSelectedIndex(tab->currentVersionIndex());
    updateViewer();
}

void MainWindow::updateThumbnailStrip() {
    auto *tab = currentTab();
    if (!tab) {
        return;
    }

    auto [thumbs, labels] = tab->versionThumbnails(48);
    m_viewerContainer->thumbnailStrip()->setThumbnails(thumbs, labels);
    m_viewerContainer->thumbnailStrip()->setSelectedIndex(tab->currentVersionIndex());
}

void MainWindow::switchContentState(ContentState state) {
    switch (state) {
        case ContentState::Empty:
            m_contentStack->setCurrentWidget(m_mainMenu);
            break;
        case ContentState::Viewer:
            m_contentStack->setCurrentWidget(m_viewerContainer);
            break;
    }
}
