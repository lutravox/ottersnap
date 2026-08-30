#pragma once

#include <optional>

#include <QCache>
#include <QDateTime>
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QUuid>
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

    /// @brief Normalize a file path for registry lookups.
    /// @param filePath Path to normalize.
    /// @return Canonical path if the file exists, otherwise an absolute path.
    static QString normalizePath(const QString& filePath);

    /// @brief Look up the image key registered for a file path.
    /// @param filePath Absolute path of the source image.
    /// @return The registered image key, or std::nullopt if the path has no
    /// registered snapshots.
    static std::optional<QString> keyForPath(const QString& filePath);

    /// @brief Return a stable cache/directory name for a file path.
    /// @param filePath Absolute path of the source image.
    /// @return The registered image key, or a path hash for unregistered paths.
    static QString cacheKeyForPath(const QString& filePath);

    /// @brief Outcome of an updateImagePath() call.
    enum class UpdatePathResult {
        Ok,                      ///< Path re-pointed successfully (or nothing to re-point).
        TargetAlreadyRegistered, ///< newPath is registered under a different image key.
        Failed                   ///< The database update failed.
    };

    /// @brief Re-point a registered image to a new file path.
    /// @param oldPath Current absolute path of the source image.
    /// @param newPath New absolute path of the source image.
    /// @return The outcome of the update.
    static UpdatePathResult updateImagePath(const QString& oldPath, const QString& newPath);

    /// @brief Ensure the base directory exists on disk.
    static void ensureDir();

    /// @brief Compute a SHA-256 checksum of an image's raw pixel data.
    /// @param image The image to hash.
    /// @return The hexadecimal checksum string.
    static QString computeChecksum(const QImage& image);

    /// @brief Load all snapshot records for an image file.
    /// @param filePath Absolute path of the source image.
    /// @return A vector of metadata for all snapshots associated with this
    /// file, sorted by timestamp ascending.
    static QVector<ImageSnapshot> loadSnapshots(const QString& filePath);

    /// @brief Walk the parent chain of a snapshot up to its base.
    /// @param snapshots The full snapshot list for the image.
    /// @param target The snapshot to build the chain for.
    /// @return The chain ordered from base to target, or std::nullopt if the
    /// target is missing or a parent link is broken.
    static std::optional<QVector<ImageSnapshot>>
    snapshotChain(const QVector<ImageSnapshot>& snapshots, const ImageSnapshot& target);

    /// @brief Delete a specific snapshot for an image file.
    /// @param filePath Absolute path of the source image.
    /// @param uuid The unique identity of the snapshot to remove.
    /// @return The remaining snapshots, or std::nullopt on failure.
    static std::optional<QVector<ImageSnapshot>> deleteSnapshot(const QString& filePath,
                                                                const QUuid&   uuid);

    /// @brief Delete multiple snapshots for an image file in a single operation.
    /// @param filePath Absolute path of the source image.
    /// @param uuids The unique identities of the snapshots to remove.
    /// @return The remaining snapshots, or std::nullopt if none could be deleted.
    static std::optional<QVector<ImageSnapshot>> deleteSnapshots(const QString&        filePath,
                                                                 const QVector<QUuid>& uuids);

    /// @brief Save a new snapshot of an image, skipping duplicates.
    /// @param filePath Absolute path of the source image.
    /// @param image The image data to save.
    /// @param previousImage Full-res state of the latest snapshot, if the
    ///        caller already holds it. Used for the delta when previousUuid
    ///        matches the latest snapshot; otherwise it is ignored.
    /// @param previousUuid Uuid of the snapshot previousImage belongs to.
    /// @return The result containing status and snapshot index, or std::nullopt on failure.
    static std::optional<SaveResult> saveSnapshot(const QString&  filePath,
                                                  const QImage&   image,
                                                  const QImage&   previousImage = {},
                                                  const QUuid&    previousUuid  = {});

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

    /// @brief Build the base image and delta chain required to reconstruct a snapshot.
    /// @param filePath Absolute path of the source image.
    /// @param uuid The unique identity of the target snapshot.
    /// @return The reconstruction sequence ordered from base to target, or
    /// std::nullopt if the target or any link in its chain is missing.
    static std::optional<ReconstructionSequence>
    buildReconstructionSequence(const QString& filePath, const QUuid& uuid);

    /// @brief Reconstruct a snapshot image from base + deltas.
    /// @param filePath Absolute path of the source image.
    /// @param uuid The unique identity of the snapshot to reconstruct.
    /// @param targetSize Desired output size; pass QSize() for full resolution.
    static QImage
    reconstructSnapshot(const QString&                           filePath,
                        const QUuid&                             uuid,
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
    static bool importHistory(const QString& filePath,
                              const QString& bundlePath,
                              int           *duplicatesFound = nullptr);

  private:
    /// @brief Ensures that the base storage and image-specific directories are ready.
    /// @param filePath Absolute path of the source image.
    /// @return The path to the snapshot directory, or an empty string on failure.
    static QString ensureStorageReady(const QString& filePath);

    /// @brief Compute a SHA-256 based name from a file path, used as a cache
    /// name for images without a registered key.
    /// @param filePath Absolute path of the source image.
    static QString hashPath(const QString& filePath);

    /// @brief Sort snapshots by timestamp ascending (stable).
    /// @param snapshots The snapshot list to sort in place.
    static void sortSnapshots(QVector<ImageSnapshot>& snapshots);

    /// @brief Serializes mutating snapshot store operations (save, delete,
    /// import, export). Recursive so operations may call each other.
    /// GUI-thread reads never take this lock.
    static QRecursiveMutex s_opMutex;

    /// @brief Protects the in-memory caches below. Held only briefly; never
    /// while waiting on s_opMutex or doing I/O of any length.
    static QMutex s_cacheMutex;

    /// @brief In-memory cache of all snapshot records, keyed by image key.
    static QHash<QString, QVector<ImageSnapshot>> s_snapshotsCache;

    /// @brief In-memory cache of registered file paths to image keys.
    static QHash<QString, QString> s_pathKeyCache;

  private:
    /// @brief keyForPath() without locking; caller must hold s_cacheMutex.
    static std::optional<QString> keyForPathLocked(const QString& filePath);
};
