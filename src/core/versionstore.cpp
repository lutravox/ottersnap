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
static const int     c_baseInterval = 100;

static bool atomicWrite(const QString& targetPath, std::function<bool(const QString&)> writeOp) {
    QString tmp = targetPath + c_tmp;
    if (!writeOp(tmp)) {
        qWarning() << "[VersionStore] failed to write temp file: " << targetPath;
        return false;
    }
    if (QFile::exists(targetPath)) {
        QFile::remove(targetPath);
    }
    if (!QFile::rename(tmp, targetPath)) {
        qWarning() << "[VersionStore] failed to rename temp file: " << tmp << "->" << targetPath;
        QFile::remove(tmp);
        return false;
    }
    return true;
}

static QString versionDirPath(const QString& key) {
    return VersionStore::baseDir() + '/' + key;
}

static QString indexPath(const QString& versionDir) {
    return versionDir + '/' + c_indexFile;
}

static QJsonObject toJsonObject(const ImageVersion& v) {
    QJsonObject obj;
    obj["version"] = v.version;
    obj["fileName"] = v.fileName;
    obj["timestamp"] = v.timestamp.toString(Qt::ISODateWithMs);
    obj["checksum"] = v.checksum;
    obj["isBase"] = v.isBase;
    return obj;
}

static ImageVersion fromJsonObject(const QJsonObject& obj) {
    ImageVersion v;
    v.version = obj["version"].toInt();
    v.fileName = obj["fileName"].toString();
    v.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODateWithMs);
    v.checksum = obj["checksum"].toString();
    v.isBase = obj["isBase"].toBool(true);
    return v;
}

static bool saveImageFile(const QString& targetPath, const QImage& image) {
    return atomicWrite(targetPath, [&image](const QString& tmp) {
        if (!image.save(tmp, "PNG")) {
            qWarning() << "[VersionStore] failed to save image:" << tmp;
            return false;
        }
        return true;
    });
}

static bool saveFile(const QString& targetPath, const QByteArray& data) {
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

static QByteArray computeDelta(const QImage& current, const QImage& previous) {
    QImage cur = current.convertToFormat(QImage::Format_ARGB32);
    QImage prev = previous.convertToFormat(QImage::Format_ARGB32);

    if (cur.size() != prev.size()) {
        qDebug() << "[VersionStore] current and previous images size mismatch";
        return {};
    }

    const uchar *curBits = cur.bits();
    const uchar *prevBits = prev.bits();
    int          size = cur.bytesPerLine() * cur.height();

    QByteArray delta(size, 0);
    uchar     *deltaBits = reinterpret_cast<uchar *>(delta.data());

    for (int i = 0; i < size; ++i) {
        deltaBits[i] = curBits[i] ^ prevBits[i];
    }

    return qCompress(delta);
}

static void applyDelta(QImage& image, const QByteArray& compressedDelta) {
    QByteArray delta = qUncompress(compressedDelta);
    if (delta.isEmpty())
        return;

    QImage       img = image.convertToFormat(QImage::Format_ARGB32);
    uchar       *imgBits = img.bits();
    const uchar *deltaBits = reinterpret_cast<const uchar *>(delta.data());
    int          size = img.bytesPerLine() * img.height();

    // Ensure delta size matches image size to avoid buffer overflow
    int processSize = std::min(size, (int)delta.size());
    for (int i = 0; i < processSize; ++i) {
        imgBits[i] = imgBits[i] ^ deltaBits[i];
    }
    image = img;
}

QString VersionStore::baseDir() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + '/' + c_baseSubDir;
}

QString VersionStore::imageKey(const QString& filePath) {
    QByteArray hash = QCryptographicHash::hash(filePath.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromUtf8(hash.left(16).toHex());
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
    v.timestamp = QDateTime::currentDateTimeUtc();
    v.checksum = checksum;

    bool shouldBeBase = (nextVer == 1) || ((nextVer - 1) % c_baseInterval == 0);

    // If not the first version, check if image properties changed
    if (!shouldBeBase && !versions.isEmpty()) {
        std::optional<QImage> prevImg = loadVersionImage(filePath, versions.last().version);
        if (!prevImg || prevImg->size() != image.size() || prevImg->format() != image.format()) {
            shouldBeBase = true;
        }
    }

    v.isBase = shouldBeBase;

    if (v.isBase) {
        v.fileName = QString::asprintf("v%04d.png", v.version);
        QString imgPath = vd + '/' + v.fileName;
        if (!saveImageFile(imgPath, image)) {
            qWarning() << "[VersionStore] failed to create version file: " << imgPath;
            return std::nullopt;
        }
    } else {
        v.fileName = QString::asprintf("v%04d.delta", v.version);

        // Get previous image to compute delta
        std::optional<QImage> prevImg = loadVersionImage(filePath, versions.last().version);
        if (!prevImg) {
            qWarning() << "[VersionStore] failed to load previous version for delta";
            return std::nullopt;
        }

        QByteArray delta = computeDelta(image, *prevImg);
        if (delta.isEmpty()) {
            qWarning() << "[VersionStore] failed to compute delta for" << v.fileName;
            return std::nullopt;
        }

        QString deltaPath = vd + '/' + v.fileName;
        if (!saveFile(deltaPath, delta)) {
            qWarning() << "[VersionStore] failed to save delta file: " << deltaPath;
            return std::nullopt;
        }
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
        QFile::remove(vd + '/' + v.fileName);
        return std::nullopt;
    }

    qDebug() << "[VersionStore] saved version" << v.version << (v.isBase ? " (base)" : " (delta)")
             << "for" << filePath;
    s_versionsCache[key] = versions;
    return v.version;
}

std::optional<QImage> VersionStore::loadVersionImage(const QString& filePath, int versionIndex) {
    QVector<ImageVersion> versions = loadVersions(filePath);
    QString               key = imageKey(filePath);

    // Find the target version
    int targetIdx = -1;
    for (int i = 0; i < versions.size(); ++i) {
        if (versions[i].version == versionIndex) {
            targetIdx = i;
            break;
        }
    }

    if (targetIdx == -1) {
        qWarning() << "[VersionStore] version" << versionIndex << "not found for" << filePath;
        return std::nullopt;
    }

    // Find the nearest preceding base image (Keyframe)
    int baseIdx = -1;
    for (int i = targetIdx; i >= 0; --i) {
        if (versions[i].isBase) {
            baseIdx = i;
            break;
        }
    }

    if (baseIdx == -1) {
        qWarning() << "[VersionStore] no base image found for version chain ending at"
                   << versionIndex;
        return std::nullopt;
    }

    // Load the base image
    QString vd = versionDirPath(key);
    QString basePath = vd + '/' + versions[baseIdx].fileName;
    QImage  img;
    if (!img.load(basePath)) {
        qWarning() << "[VersionStore] failed to load base image:" << basePath;
        return std::nullopt;
    }

    // Sequentially apply deltas from base to target
    for (int i = baseIdx + 1; i <= targetIdx; ++i) {
        QString deltaPath = vd + '/' + versions[i].fileName;
        QFile   f(deltaPath);
        if (!f.open(QFile::ReadOnly)) {
            qWarning() << "[VersionStore] failed to open delta file:" << deltaPath;
            return std::nullopt;
        }

        applyDelta(img, f.readAll());
        f.close();
    }

    qDebug() << "[VersionStore] Loaded version" << versionIndex << "reconstructed from base"
             << versions[baseIdx].version;
    return img;
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
