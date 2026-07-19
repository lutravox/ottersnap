#include "core/imageloader.h"

#include <QDebug>

QImage loadImage(const QString& filePath) {
    QImage image;
    if (image.load(filePath)) {
        qDebug() << "[loadImage] loaded image:" << filePath;
        return image;
    }

    qWarning() << "[loadImage] failed to load image:" << filePath;
    return {};
}
