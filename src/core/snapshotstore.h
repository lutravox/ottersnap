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

    struct BaseImage {
        QImage  image;
        QString checksum;
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

    /// @brief Load a pre-computed thumbnail for a specific snapshot.
    /// @param filePath Absolute path of the source image.
    /// @param snapshotIndex The snapshot index to load.
    /// @param size The desired size of the thumbnail (max width/height).
    /// @return The thumbnail image, or a null image if not found.
    static QImage loadThumbnail(const QString& filePath, int snapshotIndex, QSize size);

    /// @brief Load a base image from the snapshot store.
    static std::optional<BaseImage> loadBaseImage(const QString& filePath, int snapshotIndex);

    /// @brief Load a delta buffer from the snapshot store.
    static std::optional<QByteArray> loadDelta(const QString& filePath, int snapshotIndex);

    /// @brief Delete all stored snapshots for an image file.
    static void deleteAllSnapshots(const QString& filePath);

    /// @brief Reconstruct a snapshot image on the CPU.
    /// @warning This is a slow operation and should not be called on the UI thread during
    /// rendering.
    static std::optional<QImage> reconstruct(const QString& filePath, int snapshotIndex);

    /// @brief Clear the in-memory snapshot cache.
    static void clearCache();

    /// @brief Save a thumbnail image to disk.
    static void saveThumbnail(const QString& filePath, int snapshotIndex, const QImage& image);

    /// @brief Apply a delta buffer to an image.
    static void applyDelta(QImage& image, const QByteArray& delta);

    /// @brief In-memory cache of all snapshot records, keyed by image key (hash of filePath).
    static QHash<QString, QVector<ImageSnapshot>> s_snapshotsCache;
};
