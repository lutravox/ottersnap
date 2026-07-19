#pragma once

#include <QDialog>

class QCheckBox;

/// @brief Simple settings dialog for application preferences.
class SettingsDialog : public QDialog {
    Q_OBJECT

  public:
    /// @brief Construct the settings dialog.
    /// @param parent Parent widget.
    explicit SettingsDialog(QWidget *parent = nullptr);

  private:
    QCheckBox *m_cbResizeToFit = nullptr;
    QCheckBox *m_cbRestoreSession = nullptr;
};
