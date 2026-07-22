#include "core/snapshotstore.h"
#include "config/appsettings.h"
#include "core/diskutils.h"
#include "core/imagecache.h"

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

QHash<QString, QVector<ImageSnapshot>> SnapshotStore::s_snapshotsCache;

static const QString c_baseSubDir = QLatin1String("snapshots");
static const QString c_indexFile = QLatin1String("index.json");
static const QString c_tmp = QLatin1String(".tmp");

static QString snapshotDirPath(const QString& key) {
    return SnapshotStore::baseDir() + '/' + key;
}

static QString indexPath(const QString& snapshotDir) {
    return snapshotDir + '/' + c_indexFile;
}

static QJsonObject toJsonObject(const ImageSnapshot& s) {
    QJsonObject obj;
    obj["snapshotIndex"] = s.snapshotIndex;
    obj["fileName"] = s.fileName;
    obj["timestamp"] = s.timestamp.toString(Qt::ISODateWithMs);
    obj["checksum"] = s.checksum;
    obj["isBase"] = s.isBase;
    return obj;
}

static ImageSnapshot fromJsonObject(const QJsonObject& obj) {
    ImageSnapshot s;
    s.snapshotIndex = obj["snapshotIndex"].toInt();
    s.fileName = obj["fileName"].toString();
    s.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODateWithMs);
    s.checksum = obj["checksum"].toString();
    s.isBase = obj["isBase"].toBool(true);
    return s;
}

static bool saveImageFile(const QString& targetPath, const QImage& image) {
    return DiskUtils::atomicWrite(targetPath, [&image](const QString& tmp) {
        if (!image.save(tmp, "PNG")) {
            qWarning() << "[SnapshotStore] Failed to save image:" << tmp;
            return false;
        }
        return true;
    });
}

static bool saveFile(const QString& targetPath, const QByteArray& data) {
    return DiskUtils::atomicWrite(targetPath, [&data](const QString& tmp) {
        QFile f(tmp);
        if (!f.open(QFile::WriteOnly | QFile::Truncate)) {
            qWarning() << "[SnapshotStore] Failed to open temp file for writing:" << tmp;
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
        qDebug() << "[SnapshotStore] Current and previous images size mismatch";
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

QString SnapshotStore::baseDir() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + '/' + c_baseSubDir;
}

QString SnapshotStore::thumbnailDir() {
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/thumbnails";
}

QString SnapshotStore::imageKey(const QString& filePath) {
    QByteArray hash = QCryptographicHash::hash(filePath.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromUtf8(hash.left(16).toHex());
}

void SnapshotStore::ensureDir() {
    QString bd = baseDir();
    DiskUtils::ensureDir(bd);
}

QString SnapshotStore::computeChecksum(const QImage& image) {
    QByteArray ba;
    QBuffer    buffer(&ba);
    if (!image.save(&buffer, "PNG")) {
        qWarning() << "[SnapshotStore] Failed to save image to buffer for checksum";
        return {};
    }
    return QString::fromUtf8(QCryptographicHash::hash(ba, QCryptographicHash::Sha256).toHex());
}

QVector<ImageSnapshot> SnapshotStore::loadSnapshots(const QString& filePath) {
    QString key = imageKey(filePath);
    if (s_snapshotsCache.contains(key)) {
        return s_snapshotsCache.value(key);
    }

    // Lazy load index
    QString sd = snapshotDirPath(key);
    QString idxPath = indexPath(sd);
    if (!QFile::exists(idxPath)) {
        return {};
    }

    QFile f(idxPath);
    if (!f.open(QFile::ReadOnly)) {
        qWarning() << "[SnapshotStore] Failed to open index file:" << idxPath;
        return {};
    }

    QJsonParseError err;
    QJsonDocument   doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        qWarning() << "[SnapshotStore] Failed to parse index file:" << idxPath;
        return {};
    }

    QVector<ImageSnapshot> snapshots;
    snapshots.reserve(doc.array().size());
    for (const QJsonValue& val : doc.array()) {
        snapshots.append(fromJsonObject(val.toObject()));
    }
    s_snapshotsCache[key] = snapshots;
    return snapshots;
}

std::optional<SnapshotStore::SaveResult> SnapshotStore::saveSnapshot(const QString& filePath,
                                                                     const QImage&  image) {
    QString checksum = computeChecksum(image);
    if (checksum.isEmpty()) {
        qWarning() << "[SnapshotStore] Refused to save: image has null pixel data or save failed";
        return std::nullopt;
    }

    QString                key = imageKey(filePath);
    QVector<ImageSnapshot> snapshots = loadSnapshots(filePath);

    // Skip if identical snapshot already exists
    for (const ImageSnapshot& s : snapshots) {
        if (s.checksum == checksum) {
            qDebug() << "[SnapshotStore] Duplicate image skipped for" << filePath
                     << "(existing snapshot" << s.snapshotIndex << ")";
            return SaveResult{SaveStatus::Existing, s.snapshotIndex};
        }
    }

    ensureDir();

    int     nextIdx = snapshots.isEmpty() ? 1 : snapshots.last().snapshotIndex + 1;
    QString sd = snapshotDirPath(key);
    if (!DiskUtils::ensureDir(sd)) {
        return std::nullopt;
    }

    ImageSnapshot s;
    s.snapshotIndex = nextIdx;
    s.timestamp = QDateTime::currentDateTimeUtc();
    s.checksum = checksum;

    bool shouldBeBase = (nextIdx == 1) || ((nextIdx - 1) % AppSettings::baseInterval() == 0);

    // If not the first snapshot, check if image properties changed
    if (!shouldBeBase && !snapshots.isEmpty()) {
        std::optional<QImage> prevImg = loadSnapshotImage(filePath, snapshots.last().snapshotIndex);
        if (!prevImg || prevImg->size() != image.size() || prevImg->format() != image.format()) {
            shouldBeBase = true;
        }
    }

    s.isBase = shouldBeBase;

    if (s.isBase) {
        s.fileName = QString::asprintf("s%04d.png", s.snapshotIndex);
        QString imgPath = sd + '/' + s.fileName;
        if (!saveImageFile(imgPath, image)) {
            qWarning() << "[SnapshotStore] Failed to create snapshot file:" << imgPath;
            return std::nullopt;
        }
    } else {
        s.fileName = QString::asprintf("s%04d.delta", s.snapshotIndex);

        // Get previous image to compute delta
        std::optional<QImage> prevImg = loadSnapshotImage(filePath, snapshots.last().snapshotIndex);
        if (!prevImg) {
            qWarning() << "[SnapshotStore] Failed to load previous snapshot for delta";
            return std::nullopt;
        }

        QByteArray delta = computeDelta(image, *prevImg);
        if (delta.isEmpty()) {
            qWarning() << "[SnapshotStore] Failed to compute delta for" << s.fileName;
            return std::nullopt;
        }

        QString deltaPath = sd + '/' + s.fileName;
        if (!saveFile(deltaPath, delta)) {
            qWarning() << "[SnapshotStore] Failed to save delta file:" << deltaPath;
            return std::nullopt;
        }
    }

    // Update index atomically
    snapshots.append(s);
    QJsonArray arr;
    for (const ImageSnapshot& es : snapshots) {
        arr.append(toJsonObject(es));
    }

    QString idxPath = indexPath(sd);
    if (!saveFile(idxPath, QJsonDocument(arr).toJson(QJsonDocument::Compact))) {
        qWarning() << "[SnapshotStore] Failed to create index file:" << idxPath;
        QFile::remove(sd + '/' + s.fileName);
        return std::nullopt;
    }

    qDebug() << "[SnapshotStore] saved snapshot" << s.snapshotIndex
             << (s.isBase ? "(base)" : " (delta)") << "for" << filePath;
    s_snapshotsCache[key] = snapshots;

    return SaveResult{SaveStatus::Created, s.snapshotIndex};
}

std::optional<QImage> SnapshotStore::loadSnapshotImage(const QString& filePath, int snapshotIndex) {
    QString key = imageKey(filePath);
    QString cacheKey = key + ":" + QString::number(snapshotIndex);

    // Check the LRU cache
    if (QImage *cached = ImageCache::get(cacheKey)) {
        qDebug() << "[SnapshotStore] Loaded snapshot" << snapshotIndex << "from cache";
        return *cached;
    }

    QVector<ImageSnapshot> snapshots = loadSnapshots(filePath);

    // Find the target snapshot
    int targetIdx = -1;
    for (int i = 0; i < snapshots.size(); ++i) {
        if (snapshots[i].snapshotIndex == snapshotIndex) {
            targetIdx = i;
            break;
        }
    }

    if (targetIdx == -1) {
        qWarning() << "[SnapshotStore] Snapshot" << snapshotIndex << "not found for" << filePath;
        return std::nullopt;
    }

    // Find the nearest preceding base image (Keyframe)
    int baseIdx = -1;
    for (int i = targetIdx; i >= 0; --i) {
        if (snapshots[i].isBase) {
            baseIdx = i;
            break;
        }
    }

    if (baseIdx == -1) {
        qWarning() << "[SnapshotStore] No base image found for snapshot chain ending at"
                   << snapshotIndex;
        return std::nullopt;
    }

    // Load the base image
    QString sd = snapshotDirPath(key);
    QString basePath = sd + '/' + snapshots[baseIdx].fileName;
    QImage  img;
    if (!img.load(basePath)) {
        qWarning() << "[SnapshotStore] Failed to load base image:" << basePath;
        return std::nullopt;
    }

    // Sequentially apply deltas from base to target
    for (int i = baseIdx + 1; i <= targetIdx; ++i) {
        QString deltaPath = sd + '/' + snapshots[i].fileName;
        QFile   f(deltaPath);
        if (!f.open(QFile::ReadOnly)) {
            qWarning() << "[SnapshotStore] Failed to open delta file:" << deltaPath;
            return std::nullopt;
        }

        applyDelta(img, f.readAll());
        f.close();
    }

    // Cache the reconstructed image
    // Update max cost based on current settings
    ImageCache::updateMaxCost(AppSettings::maxSnapshotCacheSizeMB());
    ImageCache::insert(cacheKey, new QImage(img), img.sizeInBytes());

    qDebug() << "[SnapshotStore] Loaded snapshot" << snapshotIndex << "reconstructed from base"
             << snapshots[baseIdx].snapshotIndex << "and cached";
    return img;
}

void SnapshotStore::deleteAllSnapshots(const QString& filePath) {
    QString key = imageKey(filePath);
    QString sd = snapshotDirPath(key);
    QDir    dir(sd);
    if (dir.exists()) {
        if (!dir.removeRecursively()) {
            qWarning() << "[SnapshotStore] Failed to delete snapshot directory:" << sd;
        } else {
            qDebug() << "[SnapshotStore] Deleted all snapshots for" << filePath;
        }
    }
    s_snapshotsCache.remove(key);

    // We cannot easily remove specific keys from QCache by prefix.
    // The images will be evicted naturally via LRU.
}

void SnapshotStore::clearCache() {
    s_snapshotsCache.clear();
}

bool SnapshotStore::deleteSnapshot(const QString& filePath, int snapshotIndex) {
    QString                key = imageKey(filePath);
    QVector<ImageSnapshot> snapshots = loadSnapshots(filePath);

    int targetIdx = -1;
    for (int i = 0; i < snapshots.size(); ++i) {
        if (snapshots[i].snapshotIndex == snapshotIndex) {
            targetIdx = i;
            break;
        }
    }

    if (targetIdx == -1) {
        qWarning() << "[SnapshotStore] Snapshot not found for deletion:" << snapshotIndex;
        return false;
    }

    // If we are deleting a snapshot that has dependents (i.e., not the last one),
    // we must repair the chain to avoid orphaning subsequent snapshots.
    if (targetIdx < snapshots.size() - 1) {
        int nextIdxInList = targetIdx + 1;
        int nextSnapshotId = snapshots[nextIdxInList].snapshotIndex;

        // Reconstruct the image of the next snapshot using the current (valid) chain
        auto optImgNext = loadSnapshotImage(filePath, nextSnapshotId);
        if (!optImgNext) {
            qWarning() << "[SnapshotStore] Failed to reconstruct next snapshot for chain repair";
            return false;
        }

        QString sd = snapshotDirPath(key);
        if (targetIdx == 0) {
            // Deleting the first snapshot; the next one must now become the base
            snapshots[nextIdxInList].isBase = true;
            snapshots[nextIdxInList].fileName = QString::asprintf("s%04d.png", nextSnapshotId);
            if (!saveImageFile(sd + '/' + snapshots[nextIdxInList].fileName, *optImgNext)) {
                qWarning() << "[SnapshotStore] Failed to save new base image during chain repair:"
                           << snapshots[nextIdxInList].fileName;
                return false;
            }
        } else {
            // Rebase the next snapshot onto the predecessor
            int  prevIdxInList = targetIdx - 1;
            auto optImgPrev = loadSnapshotImage(filePath, snapshots[prevIdxInList].snapshotIndex);
            if (!optImgPrev) {
                qWarning() << "[SnapshotStore] Failed to reconstruct predecessor for rebasing";
                return false;
            }

            QByteArray delta = computeDelta(*optImgNext, *optImgPrev);
            if (delta.isEmpty()) {
                qWarning() << "[SnapshotStore] Failed to compute rebase delta for snapshot"
                           << nextSnapshotId;
                return false;
            }

            snapshots[nextIdxInList].isBase = false;
            snapshots[nextIdxInList].fileName = QString::asprintf("s%04d.delta", nextSnapshotId);
            if (!saveFile(sd + '/' + snapshots[nextIdxInList].fileName, delta)) {
                qWarning() << "[SnapshotStore] Failed to save rebase delta file:"
                           << snapshots[nextIdxInList].fileName;
                return false;
            }
        }
    }

    // Delete the target snapshot file
    QString sd = snapshotDirPath(key);
    QFile::remove(sd + '/' + snapshots[targetIdx].fileName);

    // Remove from list and update index
    snapshots.removeAt(targetIdx);
    QJsonArray arr;
    for (const ImageSnapshot& s : snapshots) {
        arr.append(toJsonObject(s));
    }

    if (!saveFile(indexPath(sd), QJsonDocument(arr).toJson(QJsonDocument::Compact))) {
        qWarning() << "[SnapshotStore] Failed to update index after deletion";
        return false;
    }

    s_snapshotsCache[key] = snapshots;
    return true;
}
