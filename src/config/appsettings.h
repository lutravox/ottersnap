#pragma once

#include <QSettings>
#include <string_view>

// Defaults
constexpr bool c_defaultRestoreSession = false;
constexpr int  c_defaultThumbnailCacheMB = 128;
constexpr int  c_defaultDeltaCacheMB = 1024;
constexpr int  c_defaultBaseInterval = 100;
constexpr bool c_defaultAutosaveSnapshots = true;
constexpr bool c_defaultAutoreloadImages = true;
constexpr bool c_defaultSnapshotOnReopen = false;
// UI range
constexpr int c_minCacheSizeMB = 16;
// Setting keys
constexpr std::string_view c_keyResizeToFit = "General/resizeToFit";
constexpr std::string_view c_keyRestoreSession = "General/restoreSession";
constexpr std::string_view c_keyAutosaveSnapshots = "General/autosaveSnapshots";
constexpr std::string_view c_keyAutoreloadImages = "General/autoreloadImages";
constexpr std::string_view c_keySnapshotOnReopen = "General/snapshotOnReopen";
constexpr std::string_view c_keyBaseInterval = "General/baseInterval";
constexpr std::string_view c_keyThumbnailCacheSizeMB = "General/maxThumbnailCacheSizeMB";
constexpr std::string_view c_keyDeltaCacheSizeMB = "General/maxDeltaCacheSizeMB";
constexpr std::string_view c_keyBackgroundColor = "General/backgroundColor";
constexpr const char      *c_defaultBackgroundColor = "#1f1f1f";

/// @brief Global application settings persisted via QSettings.
class AppSettings {
    Q_DISABLE_COPY(AppSettings)

  public:
    /// @brief Return whether window resize should keep the image scaled with the viewport.
    /// @return True to scale on resize, false to keep zoom percentage constant.
    static bool scaleWithWindow();
    /// @brief Set the scale-with-window preference.
    /// @param value The new value.
    static void setScaleWithWindow(bool value);

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

    /// @brief Return whether the image should be automatically reloaded when the file changes.
    static bool autoreloadImages();
    /// @brief Set the autoreload images preference.
    static void setAutoreloadImages(bool value);

    /// @brief Return whether a snapshot should be saved when opening an image that is already open.
    static bool snapshotOnReopen();
    /// @brief Set the snapshot-on-reopen preference.
    static void setSnapshotOnReopen(bool value);

    /// @brief Return the interval between base snapshots.
    static int baseInterval();
    /// @brief Set the interval between base snapshots.
    static void setBaseInterval(int value);

    /// @brief Return the maximum memory allowed for the thumbnail cache in MB.
    /// @return The cache size in MB.
    static int maxThumbnailCacheSizeMB();
    /// @brief Set the maximum memory allowed for the thumbnail cache.
    /// @param value The size in MB.
    static void setMaxThumbnailCacheSizeMB(int value);

    /// @brief Return the maximum memory allowed for the delta cache in MB.
    static int maxDeltaCacheSizeMB();
    /// @brief Set the maximum memory allowed for the delta cache.
    /// @param value The size in MB.
    static void setMaxDeltaCacheSizeMB(int value);

    /// @brief Return the background color of the image viewer.
    static QColor backgroundColor();
    /// @brief Set the background color of the image viewer.
    static void setBackgroundColor(const QColor& color);
    /// @brief Reset the background color to the default.
    static void resetBackgroundColor();

    /// @brief Return whether snapshots should be automatically saved when the file changes.

  private:
    AppSettings() = delete;
    ~AppSettings() = delete;

    /// @brief Return a reference to the singleton QSettings instance.
    static QSettings& settings();
};
