#pragma once

#include <QCache>
#include <QImage>
#include <QSize>
#include <QString>
#include <functional>
#include <optional>

/// @brief Cache for images and thumbnails.
class ImageCache {
  public:
    /// @brief Return a pointer to a cached image, or nullptr if not found.
    /// Note: QCache returns a pointer that is managed by the cache.
    static QImage *get(const QString& key);

    /// @brief Insert an image into the LRU cache.
    static void insert(const QString& key, QImage *image, int cost);

    /// @brief Sync the cache size with application settings.
    static void updateMaxCost(int sizeMB);

    /// @brief Clear all in-memory cached images.
    static void clear();

    /// @brief Remove a specific image from the cache.
    static void remove(const QString& key);

    /// @brief Formats a thumbnail of the given image, centered on a transparent canvas.
    static QImage formatThumbnail(const QImage& image, int size);

    /// @brief Retrieves a thumbnail from the disk cache or generates it if missing.
    /// @param imageKey The unique identifier for the image.
    /// @param version The version number.
    /// @param size The desired thumbnail size.
    /// @param loadFullImage Callback to load the full image if the thumbnail is missing.
    static QImage loadThumbnail(const QString&          imageKey,
                                int                     version,
                                const QSize&            size,
                                std::function<QImage()> loadFullImage);

  private:
    static QCache<QString, QImage> s_imageCache;
};
