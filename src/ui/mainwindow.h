#pragma once

#include <QList>
#include <QMainWindow>
#include <QMap>
#include <QStackedWidget>
#include <QStringList>
#include "controllers/effectscontroller.h"
#include "controllers/viewcontroller.h"
#include "core/sessionmanager.h"
#include "ui/emptystate.h"
#include "ui/notificationmanager.h"
#include "ui/tabbar.h"
#include "ui/viewerstate.h"

class QAction;

class ImageTab;
class Notification;
class NotificationManager;

/// @brief Main application window. Manages tabs, shared viewer, zoom
/// navigation, modifiers, and session persistence.
class MainWindow : public QMainWindow {
    Q_OBJECT

    friend class MainWindowTestWrapper;

  public:
    /// @brief Construct the main window.
    /// @param parent Optional parent widget.
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /// @brief ImageViewer instance.
    ImageViewer *viewer() {
        return m_viewerState->viewer();
    }

  protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

  private slots:
    void onFileOpen();
    void onSaveSnapshot();
    void onDeleteCurrentSnapshotRequested();
    void onCloseTab(int index);
    void onCloseCurrentTab();
    void onCloseAllTabs();
    void onToggleScaleWithWindow();
    void onResetView();
    void onZoomIn();
    void onZoomOut();
    void onActualSize();
    void onToggleFullScreen();
    void onExportSnapshot();
    void onResetEffects();
    void onSettings();
    void onTabChanged(int index);
    void onSnapshotSelected(int index);
    void onSnapshotDeletionRequested(int index);

  private:
    void      setupUi();
    void      setupMenu();
    void      updateMenuBar();
    void      updateRecentFilesMenu();
    void      openImageFile(const QString& path, bool setAsCurrent = true);
    ImageTab *currentTab();
    void      applyEffects();
    void      updateTabThumbnail(int index);
    void      setTabThumbnail(int index);
    void      notify(const QString& msg, int timeoutMs = -1);
    void      updateSnapshotTimeline();

    /// @brief Collect file paths from all open tabs.
    QStringList collectOpenPaths() const;

    /// @brief Update the viewer when the active tab changes.
    void updateViewer();

    /// @brief Sync menu action states and content visibility with the active tab.
    void updateState();

    enum class ContentState { Empty, Viewer };
    void switchContentState(ContentState state);

    TabBar              *m_tabBar;
    QStackedWidget      *m_contentStack;
    NotificationManager *m_notificationManager;
    ViewerState         *m_viewerState;
    EmptyState          *m_emptyState;
    QSettings            m_settings;
    SessionManager       m_session;
    EffectsController   *m_effectsController;
    ViewController      *m_viewController;
    QMenu               *m_fileMenu;
    QMenu               *m_recentFilesMenu;
    QMenu               *m_editMenu;
    QMenu               *m_viewMenu;
    QMenu               *m_effectsMenu;

    ContentState m_currentState = ContentState::Empty;
    bool         m_isRestoringSession = false;

    QAction *m_actionOpen;
    QAction *m_actionSaveSnapshot;
    QAction *m_actionExportSnapshot;
    QAction *m_actionDeleteSnapshot;
    QAction *m_actionCloseTab;
    QAction *m_actionCloseAllTabs;
    QAction *m_actionExit;
    QAction *m_actionScaleWithWindow;
    QAction *m_actionResetView;
    QAction *m_actionZoomIn;
    QAction *m_actionZoomOut;
    QAction *m_actionActualSize;
    QAction *m_actionFullScreen;
    QAction *m_actionGrayscale;
    QAction *m_actionMirror;
    QAction *m_actionResetEffects;
    QAction *m_actionSettings;

    ImageTab *m_currentTabInView = nullptr;
    int       m_currentVersionInView = -1;
    int       m_lastBaseIdx = -1;

    QMap<QString, ImageTab *> m_tabPaths;
};
