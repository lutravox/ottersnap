#pragma once

#include <QSettings>
#include <QStringList>

class QTabWidget;

/// @brief Persists and restores recently opened image file paths via QSettings.
class SessionManager {
  public:
    /// @brief Construct the session manager.
    SessionManager();

    /// @brief Collect paths from tabBar and persist to settings.
    /// @param tabBar The tab widget whose tabs contain the open images.
    /// @param settings QSettings instance to write to (defaults to user settings).
    void save(QTabWidget *tabBar, QSettings settings = QSettings{});

    /// @brief Load persisted paths from settings into the internal buffer.
    /// @param settings QSettings instance to read from (defaults to user settings).
    void load(QSettings settings = QSettings{});

    /// @brief Return and clear the restore buffer.
    /// @return Previously loaded paths.
    QStringList restorePaths();

    /// @brief Collect filePath() from every ImageTab in the given tab widget.
    /// @param tabBar The tab widget to scan.
    /// @return List of absolute file paths.
    static QStringList collectPaths(QTabWidget *tabBar);

  private:
    QStringList m_restorePaths;
};
