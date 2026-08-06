#pragma once

#include <QObject>

/// @brief Controller that manages application settings.
class AppSettingsController : public QObject {
    Q_OBJECT

  public:
    explicit AppSettingsController(QObject *parent = nullptr);

    /// @brief Return whether a snapshot should be saved when opening an image that is already open.
    bool shouldSaveSnapshotOnReopen() const;

    /// @brief Return whether images should be automatically reloaded when they change on disk.
    bool shouldAutoreloadImages() const;

    /// @brief Return whether snapshots should be automatically saved when the file changes.
    bool shouldAutosaveSnapshots() const;

    /// @brief Return whether the "Autosave snapshots" setting is enabled.
    bool isAutosaveSnapshotsEnabled() const;

    /// @brief Set the autosave snapshots preference.
    /// If enabled, it disables "Save snapshot on reopen" to avoid redundancy.
    void setAutosaveSnapshots(bool value);

    /// @brief Set the snapshot-on-reopen preference.
    void setSnapshotOnReopen(bool value);

    /// @brief Set the autoreload images preference.
    void setAutoreloadImages(bool value);

    /// @brief Return whether images should scale with window on resize.
    bool scaleWithWindow() const;

    /// @brief Set whether images should scale with window on resize.
    void setScaleWithWindow(bool value);

    /// @brief Return whether the viewer toolbar should be visible.
    bool toolbarVisible() const;

    /// @brief Set whether the viewer toolbar should be visible.
    void setToolbarVisible(bool value);

    /// @brief Set the restore session preference.
    void setRestoreSession(bool value);

    /// @brief Set the background color.
    void setBackgroundColor(const QColor& color);

    /// @brief Set the max thumbnail cache size.
    void setMaxThumbnailCacheSizeMB(int size);

    /// @brief Set the max delta cache size.
    void setMaxDeltaCacheSizeMB(int size);
};
