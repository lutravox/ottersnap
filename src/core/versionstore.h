#pragma once

#include <optional>

#include <QCache>
#include <QDateTime>
#include <QImage>
#include <QString>
#include <QVector>

/// @brief Metadata for a single saved image version.
struct ImageVersion {
    int       version = 0;
    QString   fileName;
    QDateTime timestamp;
    QString   checksum;
    bool      isBase = true;
};

/// @brief Persists image versions on disk, keyed by a hash of the source file path.
class VersionStore {
  public:
    enum class SaveStatus { Created, Existing };

    struct SaveResult {
        SaveStatus status;
        int        version;
    };

    /// @brief Return the base directory for all versioned images.
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

    /// @brief Load all version records for an image file.
    /// @param filePath Absolute path of the source image.
    static QVector<ImageVersion> loadVersions(const QString& filePath);

    /// @brief Save a new version of an image, skipping duplicates.
    /// @param filePath Absolute path of the source image.
    /// @param image The image data to save.
    /// @return The result containing status and version number, or std::nullopt on failure.
    static std::optional<SaveResult> saveVersion(const QString& filePath, const QImage& image);

    /// @brief Load the pixel data for a specific version.
    /// @param filePath Absolute path of the source image.
    /// @param versionIndex The version number to load.
    /// @return The loaded image, or std::nullopt if not found or load failed.
    static std::optional<QImage> loadVersionImage(const QString& filePath, int versionIndex);

    /// @brief Clear the image data cache.
    static void clearImageCache();

    /// @brief Delete all stored versions for an image file.
    static void deleteAllVersions(const QString& filePath);

  private:
    /// @brief In-memory cache of all version records, keyed by image key (hash of filePath).
    static QHash<QString, QVector<ImageVersion>> s_versionsCache;

    /// @brief LRU cache for reconstructed images, keyed by "imageKey:version".
    static QCache<QString, QImage> s_imageCache;
};
