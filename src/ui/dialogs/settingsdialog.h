#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include "controllers/appsettingscontroller.h"
#include "ui/notificationmanager.h"

class QCheckBox;
class QSpinBox;

/// @brief Simple settings dialog for application preferences.
class SettingsDialog : public QDialog {
    Q_OBJECT

  public:
    /// @brief Construct the settings dialog.
    /// @param settings The settings controller.
    /// @param notificationManager The notification manager for feedback.
    /// @param parent Parent widget.
    explicit SettingsDialog(AppSettingsController *settings, NotificationManager *notificationManager, QWidget *parent = nullptr);

  private:
    void resetAllSettings();

    QCheckBox            *m_cbRestoreSession;
    QCheckBox            *m_cbAutoreload;
    QRadioButton         *m_rbNone;
    QRadioButton         *m_rbAutosave;
    QRadioButton         *m_rbSnapshotOnReopen;
    QSpinBox             *m_sbThumbCacheSize;
    QSpinBox             *m_sbDeltaCacheSize;
    QPushButton          *m_btnBackgroundColor;
    QColor                m_bgColor;
    AppSettingsController *m_settings;
    NotificationManager   *m_notificationManager;
    QTabWidget             *m_tabs;
};
