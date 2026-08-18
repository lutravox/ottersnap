#pragma once

#include <QWidget>

/// @brief No images opened state.
class EmptyState : public QWidget {
    Q_OBJECT

  public:
    /// @brief Construct the main menu widget.
    /// @param parent Optional parent widget.
    explicit EmptyState(QWidget *parent = nullptr);

  signals:
    /// @brief Signal emitted when opening an image is requested.
    void openRequested();
};
