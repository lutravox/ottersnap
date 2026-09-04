#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include "controllers/appsettingscontroller.h"
#include "core/notificationmodel.h"

class QCheckBox;
class QSpinBox;

/// @brief Simple settings dialog for application preferences.
class SettingsDialog : public QDialog {
    Q_OBJECT

  public:
    /// @brief Construct the settings dialog.
    /// @param settings The settings controller.
    /// @param notificationModel The notification model for feedback.
    /// @param parent Parent widget.
    explicit SettingsDialog(AppSettingsController *settings, NotificationModel *notificationModel, QWidget *parent = nullptr);

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
    NotificationModel     *m_notificationModel;
    QTabWidget             *m_tabs;
};
