#pragma once

#include <QList>
#include <QMainWindow>
#include <QMap>
#include <QStackedWidget>
#include <QStringList>
#include "controllers/appsettingscontroller.h"
#include "controllers/effectscontroller.h"
#include "controllers/imagesessioncontroller.h"
#include "controllers/snapshottimelinecontroller.h"
#include "controllers/viewercontroller.h"
#include "controllers/colorinfocontroller.h"
#include "core/sessionmanager.h"
#include "ui/emptystate.h"
#include "ui/notificationmanager.h"
#include "ui/tabbar.h"
#include "ui/viewermodel.h"

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
    void onManageSnapshots();
    void onSessionInvalidated(const QString& filePath);
    void onOpenSnapshotRequested(const QString& path, const QUuid& uuid);
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
    void onExportHistory();
    void onImportHistory();
    void onUpdateImagePath();
    void onResetEffects();
    void onSettings();
    void onToggleToolbar();
    void onSwap();
    void onAbout();
    void onColorPicked(const QColor& color);
    void onColorInfoToggled(bool checked);
    void onSessionColorClustersChanged();
    void onTabChanged(int index);
    void onSnapshotDeletionRequested(const QUuid& uuid);
    void onMultipleSnapshotsDeletionRequested(const QVector<QUuid>& uuids);
    void onDeleteAllSnapshotsRequested();

  private:
    void      setupUi();
    void      setupMenu();
    void      updateMenuBar();
    void      updateRecentFilesMenu();
    void      setupTabConnections(ImageTab *tab);
    ImageTab *openImageFile(const QString& path, bool setAsCurrent = true);
    ImageTab *currentTab();
    void      applyEffects();
    void      updateTabThumbnail(int index);
    void      setTabThumbnail(int index);
    void      notify(const QString& msg, int timeoutMs = -1);
    void      updateSnapshotTimeline();
    void      updateWindowTitle();
    void      onSnapshotChanged(int index);
    void      syncTimelineSelection();
    void      updateShortcut(const QString& actionId, const QKeySequence& sequence);

    /// @brief Collect file paths from all open tabs, in tab order.
    QStringList collectOpenPaths() const;

    /// @brief Update the viewer when the active tab changes.
    void updateViewer(ImageTab *tab = nullptr);

    /// @brief Sync menu action states and content visibility with the active tab.
    void updateState();

    enum class ContentState { Empty, Viewer };
    void switchContentState(ContentState state);

    TabBar                *m_tabBar;
    QStackedWidget        *m_contentStack;
    NotificationManager   *m_notificationManager;
    ViewerModel           *m_viewerState;
    EmptyState            *m_emptyState;
    QSettings              m_settings;
    SessionManager         m_session;
    EffectsController     *m_effectsController;
    ViewerController      *m_viewerController;
    SnapshotTimelineController *m_snapshotController;
    AppSettingsController *m_settingsController;
    ColorInfoController    *m_colorInfoController;
    QMenu                 *m_fileMenu;
    QMenu                 *m_recentFilesMenu;
    QMenu                 *m_editMenu;
    QMenu                 *m_viewMenu;
    QMenu                 *m_effectsMenu;
    QMenu                 *m_helpMenu;

    ContentState m_currentState = ContentState::Empty;
    bool         m_isRestoringSession = false;

    QAction *m_actionOpen;
    QAction *m_actionSaveSnapshot;
    QAction *m_actionExportSnapshot;
    QAction *m_actionExportHistory;
    QAction *m_actionImportHistory;
    QAction *m_actionUpdatePath;
    QAction *m_actionDeleteSnapshot;
    QAction *m_actionDeleteSelectedSnapshots;
    QAction *m_actionDeleteAllSnapshots;
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

    QMap<QString, QShortcut *> m_toolShortcuts;
    QAction *m_actionSettings;
    QAction *m_actionManageSnapshots;
    QAction *m_actionToggleToolbar;
    QAction *m_actionSwap;
    QAction *m_actionAbout;

    QMap<QString, ImageTab *> m_tabPaths;
    ImageSessionController   *m_sessionController;
};
