#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QSpinBox>

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
    QCheckBox *m_cbRestoreSession;
    QCheckBox *m_cbAutosave;
    QSpinBox  *m_sbThumbCacheSize;
};
