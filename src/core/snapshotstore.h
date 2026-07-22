#pragma once

#include <optional>

#include <QCache>
#include <QDateTime>
#include <QImage>
#include <QString>
#include <QVector>

/// @brief Metadata for a single saved image snapshot.
struct ImageSnapshot {
    int       snapshotIndex = 0;
    QString   fileName;
    QDateTime timestamp;
    QString   checksum;
    bool      isBase = true;
};

/// @brief Persists image snapshots on disk, keyed by a hash of the source file path.
class SnapshotStore {
  public:
    enum class SaveStatus { Created, Existing };

    struct SaveResult {
        SaveStatus status;
        int        snapshotIndex;
    };

    /// @brief Return the base directory for all snapshotted images.
    static QString baseDir();

    /// @brief Return the cache directory for thumbnails.
    static QString thumbnailDir();

    /// @brief Compute a SHA-256 based key from a file path.
    /// @param filePath Absolute path of the source image.
    static QString imageKey(const QString& filePath);

    /// @brief Ensure the base directory exists on disk.
    static void ensureDir();

    /// @brief Compute a SHA-256 checksum of an image's raw pixel data.
    /// @param image The image to hash.
    static QString computeChecksum(const QImage& image);

    /// @brief Load all snapshot records for an image file.
    static QVector<ImageSnapshot> loadSnapshots(const QString& filePath);

    /// @brief Delete a specific snapshot for an image file.
    static bool deleteSnapshot(const QString& filePath, int snapshotIndex);

    /// @brief Save a new snapshot of an image, skipping duplicates.
    /// @param filePath Absolute path of the source image.
    /// @param image The image data to save.
    /// @return The result containing status and snapshot index, or std::nullopt on failure.
    static std::optional<SaveResult> saveSnapshot(const QString& filePath, const QImage& image);

    /// @brief Load the pixel data for a specific snapshot.
    /// @param filePath Absolute path of the source image.
    /// @param snapshotIndex The snapshot index to load.
    /// @return The loaded image, or std::nullopt if not found or load failed.
    static std::optional<QImage> loadSnapshotImage(const QString& filePath, int snapshotIndex);

    /// @brief Delete all stored snapshots for an image file.
    static void deleteAllSnapshots(const QString& filePath);

    /// @brief Clear the in-memory snapshot cache.
    static void clearCache();

  private:
    /// @brief In-memory cache of all snapshot records, keyed by image key (hash of filePath).
    static QHash<QString, QVector<ImageSnapshot>> s_snapshotsCache;
};
