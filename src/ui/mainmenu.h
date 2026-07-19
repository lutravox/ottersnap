#pragma once

#include <QWidget>

/// @brief Shown when no image tabs are open.
class MainMenu : public QWidget {
    Q_OBJECT

  public:
    /// @brief Construct the main menu widget.
    /// @param parent Optional parent widget.
    explicit MainMenu(QWidget *parent = nullptr);

  signals:
    /// @brief Emitted when the user clicks the button.
    void openRequested();
};
