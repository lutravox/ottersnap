#include "config/appsettings.h"
#include "core/diskutils.h"
#include "core/thumbnailmanager.h"
#include "core/vulkancontext.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <functional>

QHash<QString, QVector<ImageSnapshot>> SnapshotStore::s_snapshotsCache;
QMutex                                 SnapshotStore::s_mutex;

static const QString  c_baseSubDir = QLatin1String("snapshots");
static const int      c_tileWidth = 256;
static const int      c_tileHeight = 256;
static const uint32_t c_sparseFormatVersion = 1;

static const QString c_indexFile = QLatin1String("index.json");
static const QString c_tmp = QLatin1String(".tmp");

QImage
SnapshotStore::reconstructSnapshot(const QString& filePath, int snapshotIndex, QSize targetSize) {
    QVector<ImageSnapshot> snapshots = SnapshotStore::loadSnapshots(filePath);
    QString                key = SnapshotStore::imageKey(filePath);

    int targetIdx = -1;
    for (int i = 0; i < snapshots.size(); ++i) {
        if (snapshots[i].snapshotIndex == snapshotIndex) {
            targetIdx = i;
            break;
        }
    }

    int baseIdx = -1;
    for (int i = targetIdx; i >= 0; --i) {
        if (snapshots[i].isBase) {
            baseIdx = i;
            break;
        }
    }

    ReconstructionSequence seq;
    seq.baseIdx = baseIdx;
    auto optBase = SnapshotStore::loadBaseImage(filePath, snapshots[baseIdx].snapshotIndex);
    if (!optBase)
        return {}; // return null image
    seq.base = std::move(optBase->image);
    seq.baseChecksum = optBase->checksum;

    for (int i = baseIdx + 1; i <= targetIdx; ++i) {
        auto optDelta = SnapshotStore::loadDelta(filePath, snapshots[i].snapshotIndex);
        if (!optDelta)
            return {};
        seq.deltas.append({key + ":" + snapshots[i].fileName, std::move(*optDelta)});
    }

    auto reconstructor = VulkanContext::instance().getUtilityReconstructor();
    if (!reconstructor)
        return {};

    return reconstructor->reconstructToImage(seq, targetSize);
}

QImage SnapshotStore::resizeImage(const QImage& image, QSize targetSize) {
    auto reconstructor = VulkanContext::instance().getUtilityReconstructor();
    if (!reconstructor)
        return {};

    ReconstructionSequence seq;
    seq.base = image;
    seq.baseChecksum = "";

    return reconstructor->reconstructToImage(seq, targetSize);
}

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
        qDebug() << "[SnapshotStore] computeDelta: Current and previous images size mismatch";
        return {};
    }

    int width = cur.width();
    int height = cur.height();
    int tilesX = (width + c_tileWidth - 1) / c_tileWidth;
    int tilesY = (height + c_tileHeight - 1) / c_tileHeight;

    QByteArray  result;
    QDataStream stream(&result, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    // Header
    stream << (uint32_t)c_sparseFormatVersion;
    stream << (uint32_t)c_tileWidth;
    stream << (uint32_t)c_tileHeight;

    struct TileChange {
        uint32_t   index;
        QByteArray data;
    };
    QVector<TileChange> changes;

    for (int ty = 0; ty < tilesY; ++ty) {
        for (int tx = 0; tx < tilesX; ++tx) {
            int xStart = tx * c_tileWidth;
            int yStart = ty * c_tileHeight;
            int xEnd = std::min(xStart + c_tileWidth, width);
            int yEnd = std::min(yStart + c_tileHeight, height);
            int tileW = xEnd - xStart;

            bool       changed = false;
            QByteArray tileDelta;
            tileDelta.resize(static_cast<int>(c_tileWidth) * static_cast<int>(c_tileHeight) * 4);
            tileDelta.fill(0);
            uchar *dBits = reinterpret_cast<uchar *>(tileDelta.data());

            for (int y = yStart; y < yEnd; ++y) {
                const uchar *curRow = cur.scanLine(y) + static_cast<int>(xStart) * 4;
                const uchar *prevRow = prev.scanLine(y) + static_cast<int>(xStart) * 4;
                uchar       *destRow = dBits + static_cast<int>((y - yStart) * c_tileWidth * 4);

                for (int x = 0; x < tileW * 4; ++x) {
                    uchar xorVal = curRow[x] ^ prevRow[x];
                    destRow[x] = xorVal;
                    if (xorVal != 0)
                        changed = true;
                }
            }

            if (changed) {
                changes.append({(uint32_t)(ty * tilesX + tx), qCompress(tileDelta)});
            }
        }
    }

    stream << (uint32_t)changes.size();
    for (const auto& change : changes) {
        stream << change.index;
        stream << change.data;
    }

    return result;
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
    QMutexLocker locker(&s_mutex);
    QString      checksum = computeChecksum(image);
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

    QImage prevImg;
    if (!snapshots.isEmpty()) {
        prevImg = reconstructSnapshot(filePath, snapshots.last().snapshotIndex);
    }

    if (!shouldBeBase && !prevImg.isNull()) {
        if (prevImg.size() != image.size() || prevImg.format() != image.format()) {
            shouldBeBase = true;
        }
    }

    s.isBase = shouldBeBase;

    if (s.isBase) {
        s.fileName = QString("s%1.png").arg(s.snapshotIndex, 4, 10, QChar('0'));
        QString imgPath = sd + '/' + s.fileName;
        if (!saveImageFile(imgPath, image.convertToFormat(QImage::Format_ARGB32))) {
            qWarning() << "[SnapshotStore] Failed to create snapshot file:" << imgPath;
            return std::nullopt;
        }
    } else {
        s.fileName = QString("s%1.delta").arg(s.snapshotIndex, 4, 10, QChar('0'));

        if (prevImg.isNull()) {
            qWarning() << "[SnapshotStore] Failed to reconstruct previous image for delta";
            return std::nullopt;
        }

        QByteArray delta = computeDelta(image, prevImg);
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

    // Trigger thumbnail generation via the manager
    ThumbnailManager::instance().enqueueRequest(
        {.index = s.snapshotIndex, .filePath = filePath, .snapshotIndex = s.snapshotIndex});

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

    qDebug() << "[SnapshotStore] Saved snapshot" << s.snapshotIndex
             << (s.isBase ? "(base)" : "(delta)") << "for" << filePath;
    s_snapshotsCache[key] = snapshots;

    return SaveResult{SaveStatus::Created, s.snapshotIndex};
}

std::optional<SnapshotStore::BaseImage> SnapshotStore::loadBaseImage(const QString& filePath,
                                                                     int            snapshotIndex) {
    QString                key = imageKey(filePath);
    QVector<ImageSnapshot> snapshots = loadSnapshots(filePath);

    for (const auto& s : snapshots) {
        if (s.snapshotIndex == snapshotIndex && s.isBase) {
            QString sd = snapshotDirPath(key);
            QImage  img;
            if (img.load(sd + '/' + s.fileName)) {
                return BaseImage{img, s.checksum};
            }
            break;
        }
    }
    return std::nullopt;
}

std::optional<QByteArray> SnapshotStore::loadDelta(const QString& filePath, int snapshotIndex) {
    QString                key = imageKey(filePath);
    QVector<ImageSnapshot> snapshots = loadSnapshots(filePath);

    for (const auto& s : snapshots) {
        if (s.snapshotIndex == snapshotIndex && !s.isBase) {
            QString sd = snapshotDirPath(key);
            QFile   f(sd + '/' + s.fileName);
            if (f.open(QFile::ReadOnly)) {
                return f.readAll();
            }
            break;
        }
    }
    return std::nullopt;
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

        // Reconstruct the image of the next snapshot
        QImage imgNext = reconstructSnapshot(filePath, nextSnapshotId);
        if (imgNext.isNull()) {
            qWarning() << "[SnapshotStore] Failed to reconstruct next snapshot for chain repair";
            return false;
        }

        QString sd = snapshotDirPath(key);
        if (targetIdx == 0) {
            // Deleting the first snapshot; the next one must now become the base
            snapshots[nextIdxInList].isBase = true;
            snapshots[nextIdxInList].fileName =
                QString("s%1.png").arg(nextSnapshotId, 4, 10, QChar('0'));
            if (!saveImageFile(sd + '/' + snapshots[nextIdxInList].fileName, imgNext)) {
                qWarning() << "[SnapshotStore] Failed to save new base image during chain repair:"
                           << snapshots[nextIdxInList].fileName;
                return false;
            }
        } else {
            // Rebase the next snapshot onto the predecessor
            int    prevIdxInList = targetIdx - 1;
            QImage imgPrev = reconstructSnapshot(filePath, snapshots[prevIdxInList].snapshotIndex);
            if (imgPrev.isNull()) {
                qWarning() << "[SnapshotStore] Failed to reconstruct predecessor for rebasing";
                return false;
            }

            QByteArray delta = computeDelta(imgNext, imgPrev);
            if (delta.isEmpty()) {
                qWarning() << "[SnapshotStore] Failed to compute rebase delta for snapshot"
                           << nextSnapshotId;
                return false;
            }

            snapshots[nextIdxInList].isBase = false;
            snapshots[nextIdxInList].fileName =
                QString("s%1.delta").arg(nextSnapshotId, 4, 10, QChar('0'));
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
