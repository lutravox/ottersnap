#pragma once

#include <QByteArray>
#include <cstddef>

/// @brief Thin wrapper around the zstd compression library.
namespace ZstdUtils {
/// @brief Default compression level (speed/fsize balance).
constexpr int kDefaultLevel = 3;

/// @brief Compresses data with zstd.
/// @param data Input bytes.
/// @param level Compression level (1-19).
/// @return The zstd frame, or an empty byte array on failure.
QByteArray compress(const QByteArray& data, int level = kDefaultLevel);

/// @brief Decompresses a zstd frame.
/// @param data The zstd frame.
/// @param expectedSize Known output size; used when the frame does not
/// store its content size.
/// @return The decompressed bytes, or an empty byte array on failure.
QByteArray decompress(const QByteArray& data, size_t expectedSize = 0);
} // namespace ZstdUtils
