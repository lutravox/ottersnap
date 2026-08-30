#pragma once

#include <QCache>
#include <QImage>
#include <QMutex>
#include <QString>
#include <QThreadPool>

namespace ThumbnailConstants {
constexpr int StorageSize = 256;
constexpr int StandardSize = 48;
const QString Extension = ".webp";
const QString Format = "WEBP";
const QString CurrentVersion = "current";
} // namespace ThumbnailConstants

/// @brief Bounded pool for thumbnail reconstructions.
class ThumbnailThreadPool : public QThreadPool {
  public:
    static QThreadPool *instance();

  private:
    ThumbnailThreadPool();
};

/// @brief Cache for thumbnails.
class ThumbnailCache {
  public:
    /// @brief Return a pointer to a cached thumbnail, or nullptr if not found.
    /// Note: QCache returns a pointer that is managed by the cache.
    static QImage *get(const QString& key);

    /// @brief Insert a thumbnail into the LRU cache.
    static void insert(const QString& key, QImage *image, int cost);

    /// @brief Sync the cache size with application settings.
    static void updateMaxCost(int sizeMB);

    /// @brief Clear all in-memory cached thumbnails.
    static void clear();

    /// @brief Remove a specific thumbnail from the cache.
    static void remove(const QString& key);

    /// @brief Invalidate a thumbnail from both memory and disk cache.
    static void invalidate(const QString& imageKey, const QString& version);

    /// @brief Build the canonical cache key for a stored thumbnail.
    /// @param imageKey The image identifier.
    /// @param version The snapshot identity (or the current-version marker).
    /// @return The key used by both the memory cache and any writer of entries.
    static QString keyFor(const QString& imageKey, const QString& version);

    /// @brief Retrieves a thumbnail from the disk cache.
    static QImage loadThumbnail(const QString& imageKey, const QString& version, const QSize& size);

  private:
    static QCache<QString, QImage> s_thumbnailCache;
    static QMutex                  s_mutex;
};
