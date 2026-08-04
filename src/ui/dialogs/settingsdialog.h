#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QRadioButton>
#include <QSpinBox>
#include "controllers/appsettingscontroller.h"

class QCheckBox;
class QSpinBox;

/// @brief Simple settings dialog for application preferences.
class SettingsDialog : public QDialog {
    Q_OBJECT

  public:
    /// @brief Construct the settings dialog.
    /// @param parent Parent widget.
    explicit SettingsDialog(QWidget *parent = nullptr);

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
    AppSettingsController m_settings;
};
