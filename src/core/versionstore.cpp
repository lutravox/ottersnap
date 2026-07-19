#include "core/versionstore.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <functional>

QHash<QString, QVector<ImageVersion>> VersionStore::s_versionsCache;

static const QString c_baseSubDir = QLatin1String("versions");
static const QString c_indexFile = QLatin1String("index.json");
static const QString c_tmp = QLatin1String(".tmp");

static bool atomicWrite(const QString& targetPath, std::function<bool(const QString&)> writeOp) {
    QString tmp = targetPath + c_tmp;
    if (!writeOp(tmp)) {
        return false;
    }
    if (QFile::exists(targetPath)) {
        QFile::remove(targetPath);
    }
    if (!QFile::rename(tmp, targetPath)) {
        qWarning() << "[VersionStore] failed to rename temp file:" << tmp << "->" << targetPath;
        QFile::remove(tmp);
        return false;
    }
    return true;
}

QString VersionStore::baseDir() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + '/' + c_baseSubDir;
}

QString VersionStore::imageKey(const QString& filePath) {
    QByteArray hash = QCryptographicHash::hash(filePath.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromUtf8(hash.left(16).toHex());
}

QString VersionStore::versionDirPath(const QString& key) {
    return baseDir() + '/' + key;
}

QString VersionStore::indexPath(const QString& versionDir) {
    return versionDir + '/' + c_indexFile;
}

void VersionStore::ensureDir() {
    QString bd = baseDir();
    if (!QDir().mkpath(bd)) {
        qWarning() << "[VersionStore] failed to create base directory:" << bd;
    }
}

QString VersionStore::computeChecksum(const QImage& image) {
    QByteArray ba;
    QBuffer    buffer(&ba);
    if (!image.save(&buffer, "PNG")) {
        qWarning() << "[VersionStore] failed to save image to buffer for checksum";
        return {};
    }
    return QString::fromUtf8(QCryptographicHash::hash(ba, QCryptographicHash::Sha256).toHex());
}

QJsonObject VersionStore::toJsonObject(const ImageVersion& v) {
    QJsonObject obj;
    obj["version"] = v.version;
    obj["fileName"] = v.fileName;
    obj["timestamp"] = v.timestamp.toString(Qt::ISODateWithMs);
    obj["checksum"] = v.checksum;
    return obj;
}

ImageVersion VersionStore::fromJsonObject(const QJsonObject& obj) {
    ImageVersion v;
    v.version = obj["version"].toInt();
    v.fileName = obj["fileName"].toString();
    v.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODateWithMs);
    v.checksum = obj["checksum"].toString();
    return v;
}

bool VersionStore::saveImageFile(const QString& targetPath, const QImage& image) {
    return atomicWrite(targetPath, [&image](const QString& tmp) {
        if (!image.save(tmp, "PNG")) {
            qWarning() << "[VersionStore] failed to save image:" << tmp;
            return false;
        }
        return true;
    });
}

bool VersionStore::saveFile(const QString& targetPath, const QByteArray& data) {
    return atomicWrite(targetPath, [&data](const QString& tmp) {
        QFile f(tmp);
        if (!f.open(QFile::WriteOnly | QFile::Truncate)) {
            qWarning() << "[VersionStore] failed to open temp file for writing:" << tmp;
            return false;
        }
        f.write(data);
        f.close();
        return true;
    });
}

QVector<ImageVersion> VersionStore::loadVersions(const QString& filePath) {
    QString key = imageKey(filePath);
    if (s_versionsCache.contains(key)) {
        return s_versionsCache.value(key);
    }

    // Lazy load index
    QString vd = versionDirPath(key);
    QString idxPath = indexPath(vd);
    if (!QFile::exists(idxPath)) {
        return {};
    }

    QFile f(idxPath);
    if (!f.open(QFile::ReadOnly)) {
        qWarning() << "[VersionStore] failed to open index file:" << idxPath;
        return {};
    }

    QJsonParseError err;
    QJsonDocument   doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        qWarning() << "[VersionStore] failed to parse index file:" << idxPath;
        return {};
    }

    QVector<ImageVersion> versions;
    versions.reserve(doc.array().size());
    for (const QJsonValue& val : doc.array()) {
        versions.append(fromJsonObject(val.toObject()));
    }
    s_versionsCache[key] = versions;
    return versions;
}

std::optional<int> VersionStore::saveVersion(const QString& filePath, const QImage& image) {
    QString checksum = computeChecksum(image);
    if (checksum.isEmpty()) {
        qWarning() << "[VersionStore] refused to save: image has null pixel data or save failed";
        return std::nullopt;
    }

    QString               key = imageKey(filePath);
    QVector<ImageVersion> versions = loadVersions(filePath);

    // Skip if identical version already exists
    for (const ImageVersion& v : versions) {
        if (v.checksum == checksum) {
            qDebug() << "[VersionStore] duplicate image skipped for" << filePath
                     << "(existing version" << v.version << ")";
            return v.version;
        }
    }

    ensureDir();

    int     nextVer = versions.isEmpty() ? 1 : versions.last().version + 1;
    QString vd = versionDirPath(key);
    if (!QDir().mkpath(vd)) {
        qWarning() << "[VersionStore] failed to create version directory: " << vd;
        return std::nullopt;
    }

    ImageVersion v;
    v.version = nextVer;
    v.fileName = QString::asprintf("v%04d.png", nextVer);
    v.timestamp = QDateTime::currentDateTimeUtc();
    v.checksum = checksum;

    // Save image to disk atomically
    QString imgPath = vd + '/' + v.fileName;
    if (!saveImageFile(imgPath, image)) {
        qWarning() << "[VersionStore] failed to create version file: " << imgPath;
        return std::nullopt;
    }

    // Update index atomically
    versions.append(v);
    QJsonArray arr;
    for (const ImageVersion& ev : versions) {
        arr.append(toJsonObject(ev));
    }

    QString idxPath = indexPath(vd);
    if (!saveFile(idxPath, QJsonDocument(arr).toJson(QJsonDocument::Compact))) {
        qWarning() << "[VersionStore] failed to create index file: " << idxPath;
        // Clean up the orphan image file to avoid disk clutter
        QFile::remove(imgPath);
        return std::nullopt;
    }

    qDebug() << "[VersionStore] saved version" << v.version << "for" << filePath;
    s_versionsCache[key] = versions;
    return v.version;
}

std::optional<QImage> VersionStore::loadVersionImage(const QString& filePath, int versionIndex) {
    QVector<ImageVersion> versions = loadVersions(filePath);
    QString               key = imageKey(filePath);
    for (const ImageVersion& v : versions) {
        if (v.version == versionIndex) {
            QString vd = versionDirPath(key);
            QString path = vd + '/' + v.fileName;
            QImage  img;
            if (img.load(path)) {
                qDebug() << "[VersionStore] Loaded version:" << path;
                return img;
            }

            qWarning() << "[VersionStore] failed to load image file:" << path;
            return std::nullopt;
        }
    }
    qWarning() << "[VersionStore] version" << versionIndex << "not found for" << filePath;
    return std::nullopt;
}

void VersionStore::deleteAllVersions(const QString& filePath) {
    QString key = imageKey(filePath);
    QString vd = versionDirPath(key);
    QDir    dir(vd);
    if (dir.exists()) {
        if (!dir.removeRecursively()) {
            qWarning() << "[VersionStore] failed to delete version directory:" << vd;
        } else {
            qDebug() << "[VersionStore] deleted all versions for" << filePath;
        }
    }
    s_versionsCache.remove(key);
}
