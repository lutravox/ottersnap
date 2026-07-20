#pragma once

#include <QSettings>

/// @brief Global application settings persisted via QSettings.
class AppSettings {
    Q_DISABLE_COPY(AppSettings)

  public:
    /// @brief Return whether window resize should keep the image fitted to the viewport.
    /// @return True to fit on resize, false to keep zoom percentage constant.
    static bool resizeToFit();
    /// @brief Set the resize-to-fit preference.
    /// @param value The new value.
    static void setResizeToFit(bool value);

    /// @brief Return whether the previous session should be restored on startup.
    static bool restoreSession();
    /// @brief Set the session restore preference.
    /// @param value The new value.
    static void setRestoreSession(bool value);

    /// @brief Return the application name.
    static const char *applicationName();
    /// @brief Return the organization name.
    static const char *organizationName();
    /// @brief Return the organization domain.
    static const char *organizationDomain();

    /// @brief Return the QFileDialog filter string for supported image types.
    static QString fileFilter();

    /// @brief Return whether snapshots should be automatically saved when the file changes.
    static bool autosaveSnapshots();
    /// @brief Set the autosave snapshots preference.
    static void setAutosaveSnapshots(bool value);

    /// @brief Return the maximum memory allowed for the snapshot cache in MB.
    /// @return The cache size in MB.
    static int maxSnapshotCacheSizeMB();
    /// @brief Set the maximum memory allowed for the snapshot cache.
    /// @param value The size in MB.
    static void setMaxSnapshotCacheSizeMB(int value);

  private:
    AppSettings() = delete;
    ~AppSettings() = delete;

    /// @brief Return a reference to the singleton QSettings instance.
    static QSettings& settings();
};
