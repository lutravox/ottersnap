#pragma once

#include <QTabWidget>

/// @brief Configured QTabWidget: closable, movable, document mode.
class TabBar : public QTabWidget {
    Q_OBJECT

  public:
    /// @brief Construct the tab bar with all options pre-configured.
    /// @param parent Optional parent widget.
    explicit TabBar(QWidget *parent = nullptr);
};
