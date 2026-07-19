#pragma once

#include <QImage>

/// @brief Load an image from the given path.
/// @param filePath The path to load.
/// @return The loaded image, or an empty image on failure.
QImage loadImage(const QString& filePath);
