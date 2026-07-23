#pragma once

#include <QImage>
#include <QSize>
#include <QString>
#include <functional>

namespace DiskUtils {

/// @brief Atomically write a file by writing to a temporary file and then renaming it.
/// @param targetPath The final destination path.
/// @param writeOp A function that performs the actual write to the temporary path provided.
/// @return True if the operation succeeded, false otherwise.
bool atomicWrite(const QString& targetPath, std::function<bool(const QString&)> writeOp);

/// @brief Ensure that a directory exists, creating it if necessary.
/// @return True if the directory exists or was created, false otherwise.
bool ensureDir(const QString& path);

/// @brief Combines a base directory and a key into a path, ensures it exists, and returns it.
/// @param baseDir The root directory for caches/storage.
/// @param key A unique identifier for the sub-directory.
/// @return The resulting absolute path.
QString getAndEnsureDir(const QString& baseDir, const QString& key);

/// @brief Save an image to a file.
/// @param filePath The destination path.
/// @param image The image to save.
/// @return True if the operation succeeded, false otherwise.
bool saveImage(const QString& filePath, const QImage& image);

/// @return The loaded image, or an empty image on failure.
QImage loadImage(const QString& filePath);
} // namespace DiskUtils
