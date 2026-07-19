#pragma once

#include <QMainWindow>
#include <QMap>
#include <QStackedWidget>
#include <QStringList>
#include "core/sessionmanager.h"
#include "ui/mainmenu.h"
#include "ui/tabbar.h"
#include "ui/viewercontainer.h"

class QAction;

class ImageTab;
class NotificationBar;

/// @brief Main application window. Manages tabs, shared viewer, zoom
/// navigation, modifiers, and session persistence.
class MainWindow : public QMainWindow {
    Q_OBJECT

  public:
    /// @brief Construct the main window.
    /// @param parent Optional parent widget.
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /// @brief ImageViewer instance.
    ImageViewer *viewer() {
        return m_viewerContainer->viewer();
    }

  protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

  private slots:
    void onFileOpen();
    void onCloseTab(int index);
    void onToggleGrayscale();
    void onToggleMirror();
    void onSettings();
    void onTabChanged(int index);
    void onThumbnailSelected(int index);

  private:
    void      setupUi();
    void      setupMenu();
    void      openImageFile(const QString& path);
    ImageTab *currentTab();
    void      applyModifiers();
    void      updateTabThumbnail(int index);
    void      setTabThumbnail(int index);
    void      notify(const QString& msg);
    void      updateThumbnailStrip();

    /// @brief Update the viewer when the active tab changes.
    void updateViewer();

    /// @brief Sync menu action states and content visibility with the active tab.
    void updateState();

    enum class ContentState { Empty, Viewer };
    void switchContentState(ContentState state);

    TabBar          *m_tabBar;
    QStackedWidget  *m_contentStack;
    NotificationBar *m_notification;
    ViewerContainer *m_viewerContainer;
    MainMenu        *m_mainMenu;
    SessionManager   m_session;

    QAction *m_actionOpen;
    QAction *m_actionCloseTab;
    QAction *m_actionExit;
    QAction *m_actionFitWindow;
    QAction *m_actionResetZoom;
    QAction *m_actionGrayscale;
    QAction *m_actionMirror;
    QAction *m_actionSettings;

    ImageTab *m_currentTabInView = nullptr;
    int       m_currentVersionInView = -1;

    QMap<QString, ImageTab *> m_tabPaths;
};
