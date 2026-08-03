#pragma once

#include <QByteArray>
#include <QCache>
#include <QMutex>
#include <QVector>

/// @brief The decompressed result of a delta file.
struct DecompressedDelta {
    uint32_t          tileW = 0;
    uint32_t          tileH = 0;
    QByteArray        packedPixels;
    QVector<uint32_t> tileIndices;
    QVector<uint32_t> tileOffsets;
};

/// @brief LRU cache for whole decompressed deltas.
class DeltaCache {
  public:
    /// @brief Return a copy of the cached delta, or std::nullopt if not found.
    /// @param deltaId Unique identifier for the delta.
    /// @return The cached delta on hit, std::nullopt on miss.
    static std::optional<DecompressedDelta> get(const QString& deltaId);

    /// @brief Insert a decompressed delta into the LRU cache.
    /// @param deltaId Unique identifier for the delta.
    /// @param data The decompressed delta to cache.
    static void insert(const QString& deltaId, const DecompressedDelta& data);

    /// @brief Sync the cache size with application settings.
    /// @param sizeMB Maximum cache size in megabytes.
    static void updateMaxCost(int sizeMB);

    /// @brief Clear all cached deltas.
    static void clear();

  private:
    static QCache<QString, DecompressedDelta> s_deltaCache;
    static QMutex                             s_mutex;
};
