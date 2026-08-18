#pragma once

#include <optional>

#include <QCache>
#include <QDateTime>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QUuid>
#include <QString>
#include <QVector>

#include "core/vksnapshotreconstructor.h"

/// @brief Metadata for a single saved image snapshot.
struct ImageSnapshot {
    QUuid     uuid;
    QUuid     parentUuid;
    QString   fileName;
    QDateTime timestamp;
    QString   checksum;
    bool      isBase = true;
};

/// @brief Manager for handling snapshots.
class SnapshotManager {
  public:
    enum class SaveStatus { Created, Existing };

    struct SaveResult {
        SaveStatus status;
        QUuid      uuid;
    };

    struct BaseImage {
        QImage  image;
        QString checksum;
    };

    /// @brief Return the base directory for all snapshotted images.
    /// @return Absolute path to the snapshot storage directory.
    static QString baseDir();

    /// @brief Return the cache directory for thumbnails.
    /// @return Absolute path to the thumbnail cache directory.
    static QString thumbnailDir();

    /// @brief Compute a SHA-256 based key from a file path.
    /// @param filePath Absolute path of the source image.
    static QString imageKey(const QString& filePath);

    /// @brief Ensure the base directory exists on disk.
    static void ensureDir();

    /// @brief Compute a SHA-256 checksum of an image's raw pixel data.
    /// @param image The image to hash.
    /// @return The hexadecimal checksum string.
    static QString computeChecksum(const QImage& image);

    /// @brief Load all snapshot records for an image file.
    /// @param filePath Absolute path of the source image.
    /// @return A vector of metadata for all snapshots associated with this file.
    static QVector<ImageSnapshot> loadSnapshots(const QString& filePath);

    /// @brief Delete a specific snapshot for an image file.
    /// @param filePath Absolute path of the source image.
    /// @param uuid The unique identity of the snapshot to remove.
    /// @return True if the snapshot was successfully deleted, false otherwise.
    static bool deleteSnapshot(const QString& filePath, const QUuid& uuid);

    /// @brief Save a new snapshot of an image, skipping duplicates.
    /// @param filePath Absolute path of the source image.
    /// @param image The image data to save.
    /// @return The result containing status and snapshot index, or std::nullopt on failure.
    static std::optional<SaveResult> saveSnapshot(const QString& filePath, const QImage& image);

    /// @brief Load a base image from the snapshot store.
    /// @param filePath Absolute path of the source image.
    /// @param s The snapshot metadata.
    /// @return The base image and its checksum, or std::nullopt if not found.
    static std::optional<BaseImage> loadBaseImage(const QString& filePath, const ImageSnapshot& s);

    /// @brief Load a delta buffer from the snapshot store.
    /// @param filePath Absolute path of the source image.
    /// @param s The snapshot metadata.
    /// @return The binary delta data, or std::nullopt if not found.
    static std::optional<QByteArray> loadDelta(const QString& filePath, const ImageSnapshot& s);

    /// @brief Delete all stored snapshots for an image file.
    /// @param filePath Absolute path of the source image.
    static void deleteAllSnapshots(const QString& filePath);

    /// @brief Return all images that have associated snapshots.
    /// @return A list of file paths for images that have been snapshotted.
    static QVector<QString> getAllSnapshottedImages();

    /// @brief Return the total storage space used by snapshots for an image file.
    /// @param filePath Absolute path of the source image.
    /// @return Total size in bytes.
    static qint64 calculateStorageUsage(const QString& filePath);

    /// @brief Reconstruct a snapshot image from base + deltas.
    /// @param filePath Absolute path of the source image.
    /// @param uuid The unique identity of the snapshot to reconstruct.
    /// @param targetSize Desired output size; pass QSize() for full resolution.
    static QImage
    reconstructSnapshot(const QString&                           filePath,
                        const QUuid&                              uuid,
                        QSize                                    targetSize = {},
                        std::shared_ptr<VkSnapshotReconstructor> reconstructor = nullptr);

    /// @brief Resize an image using GPU acceleration.
    /// @param image The source image to resize.
    /// @param targetSize The desired output size.
    /// @return The resized image, or a null image if the GPU reconstructor is unavailable.
    static QImage resizeImage(const QImage& image, QSize targetSize);

    /// @brief Clear the in-memory snapshot cache.
    static void clearCache();

    /// @brief Export the snapshot history for an image to a single bundle file.
    /// @param filePath Absolute path of the source image.
    /// @param bundlePath Path to the destination .zip file.
    /// @return True if export was successful, false otherwise.
    static bool exportHistory(const QString& filePath, const QString& bundlePath);

    /// @brief Import snapshot history from a bundle file.
    /// @param filePath Absolute path of the source image to associate history with.
    /// @param bundlePath Path to the .zip bundle file.
    /// @param duplicatesFound Optional pointer to store the number of skipped duplicate snapshots.
    /// @return True if import was successful, false otherwise.
    static bool importHistory(const QString& filePath, const QString& bundlePath, int* duplicatesFound = nullptr);

private:
    /// @brief Ensures that the base storage and image-specific directories are ready.
    /// @param filePath Absolute path of the source image.
    /// @return The path to the snapshot directory, or an empty string on failure.
    static QString ensureStorageReady(const QString& filePath);

    /// @brief Global recursive lock for snapshot store operations to ensure
    /// thread safety. Recursive so loadSnapshots() can safely acquire it
    /// both directly and from within saveSnapshot().
    static QRecursiveMutex s_mutex;

    /// @brief In-memory cache of all snapshot records, keyed by image key (hash of filePath).
    static QHash<QString, QVector<ImageSnapshot>> s_snapshotsCache;
};
