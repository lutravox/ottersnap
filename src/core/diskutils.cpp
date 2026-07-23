#include "core/diskutils.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QStandardPaths>

namespace DiskUtils {

static const QString c_tmp = QLatin1String(".tmp");

bool atomicWrite(const QString& targetPath, std::function<bool(const QString&)> writeOp) {
    QString tmp = targetPath + c_tmp;
    if (!writeOp(tmp)) {
        qWarning() << "[DiskUtils] Failed to write temp file:" << targetPath;
        return false;
    }
    if (QFile::exists(targetPath)) {
        QFile::remove(targetPath);
    }
    if (!QFile::rename(tmp, targetPath)) {
        qWarning() << "[DiskUtils] Failed to rename temp file:" << tmp << "->" << targetPath;
        QFile::remove(tmp);
        return false;
    }
    return true;
}

bool ensureDir(const QString& path) {
    if (QDir().mkpath(path)) {
        return true;
    }
    qWarning() << "[DiskUtils] Failed to create directory:" << path;
    return false;
}

QString getAndEnsureDir(const QString& baseDir, const QString& key) {
    QString path = baseDir + '/' + key;
    ensureDir(path);
    return path;
}

QImage loadImage(const QString& filePath) {
    QImage image;
    if (image.load(filePath)) {
        qDebug() << "[DiskUtils] loaded image:" << filePath;
        return image;
    }

    qWarning() << "[DiskUtils] failed to load image:" << filePath;
    return {};
}

bool saveImage(const QString& filePath, const QImage& image) {
    if (!image.save(filePath)) {
        qWarning() << "[DiskUtils] Failed to save image:" << filePath;
        return false;
    }
    return true;
}
} // namespace DiskUtils
