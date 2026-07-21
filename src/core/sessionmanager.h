#pragma once

#include <QSettings>
#include <QStringList>

/// @brief Persists and restores recently opened image file paths via QSettings.
class SessionManager {
  public:
    /// @brief Construct the session manager.
    /// @param settings The settings backend to use for persistence.
    explicit SessionManager(QSettings& settings);

    /// @brief Persist paths to settings.
    void save(const QStringList& paths);

    /// @brief Load persisted paths from settings into the internal buffer.
    void load();

    /// @brief Return and clear the restore buffer.
    /// @return Previously loaded paths.
    QStringList restorePaths();

  private:
    QSettings&  m_settings;
    QStringList m_restorePaths;
};
