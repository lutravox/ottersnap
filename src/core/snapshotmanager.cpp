#include "core/snapshotmanager.h"
#include "config/appsettings.h"
#include "core/cpusnapshotreconstructor.h"
#include "core/diskutils.h"
#include "core/snapshotdb.h"
#include "core/thumbnailmanager.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QUuid>
#include <algorithm>
#include <functional>
#include <zip.h>

QHash<QString, QVector<ImageSnapshot>> SnapshotManager::s_snapshotsCache;
QHash<QString, QString>                SnapshotManager::s_pathKeyCache;
QRecursiveMutex                        SnapshotManager::s_opMutex;
QMutex                                 SnapshotManager::s_cacheMutex;

static const QString  c_baseSubDir = QLatin1String("snapshots");
static const int      c_tileWidth = 256;
static const int      c_tileHeight = 256;
static const uint32_t c_sparseFormatVersion = 1;

static QString snapshotDirPath(const QString& key) {
    return SnapshotManager::baseDir() + '/' + key;
}

static bool saveImageFile(const QString& path, const QImage& img) {
    return DiskUtils::atomicWrite(path, [&img](const QString& tmp) {
        if (!img.save(tmp, "PNG")) {
            qWarning() << "[SnapshotManager] Failed to save image:" << tmp;
            return false;
        }
        return true;
    });
}

static bool saveFile(const QString& targetPath, const QByteArray& data) {
    return DiskUtils::atomicWrite(targetPath, [&data](const QString& tmp) {
        QFile f(tmp);
        if (!f.open(QFile::WriteOnly | QFile::Truncate)) {
            qWarning() << "[SnapshotManager] Failed to open temp file for writing:" << tmp;
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
        qDebug() << "[SnapshotManager] computeDelta: Current and previous images size mismatch";
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

QImage
SnapshotManager::reconstructSnapshot(const QString&                           filePath,
                                     const QUuid&                             uuid,
                                     QSize                                    targetSize,
                                     std::shared_ptr<VkSnapshotReconstructor> reconstructor) {
    QVector<ImageSnapshot> snapshots = SnapshotManager::loadSnapshots(filePath);

    ImageSnapshot target;
    bool          found = false;
    for (const auto& s : snapshots) {
        if (s.uuid == uuid) {
            target = s;
            found = true;
            break;
        }
    }

    if (!found)
        return {};

    auto chainOpt = snapshotChain(snapshots, target);
    if (!chainOpt)
        return {};
    const QVector<ImageSnapshot>& chain = *chainOpt;

    // chain.first() is the base, chain.last() is the target
    ReconstructionSequence seq;
    seq.baseIdx = 0; // For the sequence, we just need the base image
    auto optBase = SnapshotManager::loadBaseImage(filePath, chain.first());
    if (!optBase)
        return {};
    seq.base = std::move(optBase->image);
    seq.baseChecksum = optBase->checksum;

    QString key = SnapshotManager::cacheKeyForPath(filePath);
    for (int i = 1; i < chain.size(); ++i) {
        auto optDelta = SnapshotManager::loadDelta(filePath, chain[i]);
        if (!optDelta)
            return {};
        seq.deltas.append(DeltaEntry{key + ":" + chain[i].fileName, std::move(*optDelta)});
    }

    if (!reconstructor) {
        CPUSnapshotReconstructor cpu;
        return cpu.reconstructToImage(seq, targetSize);
    }
    return reconstructor->reconstructToImage(seq, targetSize);
}

QImage SnapshotManager::resizeImage(const QImage& image, QSize targetSize) {
    ReconstructionSequence seq;
    seq.base = image;
    seq.baseChecksum = "";

    CPUSnapshotReconstructor cpu;
    return cpu.reconstructToImage(seq, targetSize);
}

bool SnapshotManager::exportHistory(const QString& filePath, const QString& bundlePath) {
    QMutexLocker           locker(&s_opMutex);
    QVector<ImageSnapshot> snapshots = loadSnapshots(filePath);

    if (snapshots.isEmpty()) {
        qWarning() << "[SnapshotManager] No snapshots to export for" << filePath;
        return false;
    }

    // Create a temporary directory to hold files before they are zipped
    QString tempDirPath = QDir::tempPath() + "/ottersnap_export_" +
                          QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!QDir().mkpath(tempDirPath)) {
        qWarning() << "[SnapshotManager] Failed to create temporary export directory";
        return false;
    }
    QDir tempDir(tempDirPath);

    int    err = 0;
    zip_t *archive = zip_open(bundlePath.toUtf8().constData(), ZIP_CREATE | ZIP_TRUNCATE, &err);
    if (!archive) {
        qWarning() << "[SnapshotManager] Failed to open ZIP archive for writing:" << bundlePath;
        tempDir.removeRecursively();
        return false;
    }

    // Metadata: Create a temporary file that persists until we call zip_close
    QJsonArray array;
    for (const auto& s : snapshots) {
        QJsonObject obj;
        obj["uuid"] = s.uuid.toString(QUuid::WithoutBraces);
        obj["parentUuid"] = s.parentUuid.toString(QUuid::WithoutBraces);
        obj["file"] = s.fileName;
        obj["timestamp"] = s.timestamp.toString(Qt::ISODate);
        obj["checksum"] = s.checksum;
        obj["isBase"] = s.isBase;
        array.append(obj);
    }

    QString metaPath = tempDirPath + "/metadata.json";
    QFile   metaFile(metaPath);
    if (!metaFile.open(QIODevice::WriteOnly)) {
        qWarning() << "[SnapshotManager] Failed to write temporary metadata file";
        zip_close(archive);
        tempDir.removeRecursively();
        return false;
    }
    metaFile.write(QJsonDocument(array).toJson());
    metaFile.close();

    QByteArray    metaPathBytes = metaPath.toUtf8();
    zip_source_t *metaSource = zip_source_file(archive, metaPathBytes.constData(), 0, 0);
    if (!metaSource || zip_file_add(archive, "metadata.json", metaSource, 0) < 0) {
        qWarning() << "[SnapshotManager] Failed to add metadata.json to ZIP";
        zip_close(archive);
        tempDir.removeRecursively();
        return false;
    }

    auto keyOpt = keyForPath(filePath);
    if (!keyOpt) {
        qWarning() << "[SnapshotManager] No registered key for" << filePath;
        zip_close(archive);
        tempDir.removeRecursively();
        return false;
    }

    QString       sd = snapshotDirPath(*keyOpt);
    QDir          srcDir(sd);
    QFileInfoList files = srcDir.entryInfoList(QDir::Files);

    for (const QFileInfo& info : files) {
        QString tempFilePath = tempDirPath + "/" + info.fileName();
        if (!QFile::copy(info.absoluteFilePath(), tempFilePath)) {
            qWarning() << "[SnapshotManager] Failed to copy file to temp export dir:"
                       << info.fileName();
            zip_close(archive);
            tempDir.removeRecursively();
            return false;
        }

        QString       zipPath = "files/" + info.fileName();
        QByteArray    tempFilePathBytes = tempFilePath.toUtf8();
        QByteArray    zipPathBytes = zipPath.toUtf8();
        zip_source_t *fileSource = zip_source_file(archive, tempFilePathBytes.constData(), 0, 0);
        if (!fileSource || zip_file_add(archive, zipPathBytes.constData(), fileSource, 0) < 0) {
            qWarning() << "[SnapshotManager] Failed to add file to ZIP:" << zipPath;
            zip_close(archive);
            tempDir.removeRecursively();
            return false;
        }
    }

    if (zip_close(archive) < 0) {
        qWarning() << "[SnapshotManager] Failed to close and save ZIP archive";
        tempDir.removeRecursively();
        return false;
    }

    tempDir.removeRecursively();
    qDebug() << "[SnapshotManager] Exported snapshot history for" << filePath << "to" << bundlePath;
    return true;
}

bool SnapshotManager::importHistory(const QString& filePath,
                                    const QString& bundlePath,
                                    int           *duplicatesFound) {
    if (duplicatesFound)
        *duplicatesFound = 0;

    QMutexLocker           locker(&s_opMutex);
    QVector<ImageSnapshot> existing = loadSnapshots(filePath);

    int    err = 0;
    zip_t *archive = zip_open(bundlePath.toUtf8().constData(), 0, &err);
    if (!archive) {
        qWarning() << "[SnapshotManager] Failed to open ZIP archive for reading:" << bundlePath;
        return false;
    }

    // Metadata
    zip_file_t *metaFile = zip_fopen(archive, "metadata.json", 0);
    if (!metaFile) {
        qWarning() << "[SnapshotManager] metadata.json not found in bundle:" << bundlePath;
        zip_close(archive);
        return false;
    }

    struct zip_stat st;
    zip_stat(archive, "metadata.json", 0, &st);
    QByteArray metaData;
    metaData.resize(st.size);
    zip_fread(metaFile, metaData.data(), st.size);
    zip_fclose(metaFile);

    QJsonDocument doc = QJsonDocument::fromJson(metaData);
    if (!doc.isArray()) {
        qWarning() << "[SnapshotManager] Invalid metadata in bundle:" << bundlePath;
        zip_close(archive);
        return false;
    }

    QJsonArray             array = doc.array();
    QVector<ImageSnapshot> imported;
    for (const QJsonValue& val : array) {
        QJsonObject   obj = val.toObject();
        ImageSnapshot s;
        s.uuid = QUuid::fromString(obj["uuid"].toString());
        s.parentUuid = QUuid::fromString(obj["parentUuid"].toString());
        s.fileName = obj["file"].toString();
        s.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
        s.checksum = obj["checksum"].toString();
        s.isBase = obj["isBase"].toBool();
        imported.append(s);
    }

    for (const auto& s : imported) {
        if (s.parentUuid.isNull() && !s.isBase) {
            qWarning() << "[SnapshotManager] Bundle chain head is not a base; that chain cannot "
                          "be reconstructed:"
                       << s.uuid.toString(QUuid::WithoutBraces);
        }
    }

    QString sd = ensureStorageReady(filePath);
    if (sd.isEmpty()) {
        zip_close(archive);
        return false;
    }

    auto keyOpt = keyForPath(filePath);
    if (!keyOpt) {
        zip_close(archive);
        return false;
    }
    const QString& key = *keyOpt;

    // Phase 1: extract files to disk, collecting the snapshots actually written.
    QVector<ImageSnapshot> importedNow;
    for (const auto& s : imported) {
        // Skip if this snapshot already exists in the local history
        bool alreadyExists = false;
        for (const auto& ex : existing) {
            if (ex.uuid == s.uuid) {
                alreadyExists = true;
                break;
            }
        }
        if (alreadyExists) {
            if (duplicatesFound)
                (*duplicatesFound)++;
            continue;
        }

        QString     zipPath = "files/" + s.fileName;
        zip_file_t *file = zip_fopen(archive, zipPath.toUtf8().constData(), 0);
        if (!file) {
            qWarning() << "[SnapshotManager] Missing file in bundle:" << s.fileName;
            zip_close(archive);
            return false;
        }

        struct zip_stat stFile;
        zip_stat(archive, zipPath.toUtf8().constData(), 0, &stFile);
        QByteArray data;
        data.resize(stFile.size);
        zip_fread(file, data.data(), stFile.size);
        zip_fclose(file);

        if (!saveFile(sd + '/' + s.fileName, data)) {
            qWarning() << "[SnapshotManager] Failed to write imported file:" << s.fileName;
            zip_close(archive);
            return false;
        }

        importedNow.append(s);
    }

    zip_close(archive);

    // Phase 2: record all imported snapshots in a single transaction so
    // readers never observe a partially imported history.
    if (!importedNow.isEmpty()) {
        SnapshotDbTransaction tx;
        bool                  allOk = true;
        for (const auto& s : importedNow) {
            if (!SnapshotDatabase::instance().addSnapshot(key, s))
                allOk = false;
        }
        if (!allOk) {
            tx.rollback();
            qWarning() << "[SnapshotManager] Failed to record imported snapshots in database";
            return false;
        }
    }

    // Merge the chains: imported snapshots keep their own parent links and are
    // displayed interleaved with the existing history by timestamp.
    QVector<ImageSnapshot> finalSnapshots = existing;
    for (const auto& s : importedNow) {
        finalSnapshots.append(s);
    }
    sortSnapshots(finalSnapshots);
    {
        QMutexLocker cacheLocker(&s_cacheMutex);
        s_snapshotsCache[key] = finalSnapshots;
    }

    qDebug() << "[SnapshotManager] Imported snapshot history for" << filePath << "from"
             << bundlePath;
    return true;
}

qint64 SnapshotManager::calculateStorageUsage(const QString& filePath) {
    auto keyOpt = keyForPath(filePath);
    if (!keyOpt)
        return 0;
    QString sd = snapshotDirPath(*keyOpt);
    QDir    dir(sd);
    if (!dir.exists()) {
        return 0;
    }

    qint64 totalSize = 0;
    for (const QFileInfo& info : dir.entryInfoList(QDir::Files)) {
        totalSize += info.size();
    }
    return totalSize;
}

QString SnapshotManager::baseDir() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + '/' + c_baseSubDir;
}

QString SnapshotManager::thumbnailDir() {
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/thumbnails";
}

QString SnapshotManager::normalizePath(const QString& filePath) {
    QFileInfo fi(filePath);
    QString   canonical = fi.canonicalFilePath();
    return canonical.isEmpty() ? fi.absoluteFilePath() : canonical;
}

QString SnapshotManager::hashPath(const QString& filePath) {
    QByteArray hash = QCryptographicHash::hash(filePath.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromUtf8(hash.left(16).toHex());
}

std::optional<QString> SnapshotManager::keyForPath(const QString& filePath) {
    QMutexLocker locker(&s_cacheMutex);
    return keyForPathLocked(filePath);
}

std::optional<QString> SnapshotManager::keyForPathLocked(const QString& filePath) {
    const QString path = normalizePath(filePath);

    auto cached = s_pathKeyCache.constFind(path);
    if (cached != s_pathKeyCache.constEnd()) {
        return cached.value();
    }

    auto key = SnapshotDatabase::instance().keyForPath(path);
    if (key) {
        s_pathKeyCache.insert(path, *key);
    }
    return key;
}

QString SnapshotManager::cacheKeyForPath(const QString& filePath) {
    if (auto key = keyForPath(filePath)) {
        return *key;
    }
    return hashPath(normalizePath(filePath));
}

SnapshotManager::UpdatePathResult SnapshotManager::updateImagePath(const QString& oldPath,
                                                                   const QString& newPath) {
    QMutexLocker  locker(&s_opMutex);
    const QString old = normalizePath(oldPath);
    const QString next = normalizePath(newPath);

    if (old.isEmpty() || next.isEmpty() || old == next)
        return UpdatePathResult::Failed;

    if (auto existing = keyForPath(next)) {
        auto oldKey = keyForPath(old);
        if (!oldKey || *existing != *oldKey)
            return UpdatePathResult::TargetAlreadyRegistered;
    }

    if (auto oldKey = keyForPath(old)) {
        if (!SnapshotDatabase::instance().setImagePath(*oldKey, next))
            return UpdatePathResult::Failed;
        {
            QMutexLocker cacheLocker(&s_cacheMutex);
            s_pathKeyCache.remove(old);
            s_pathKeyCache.insert(next, *oldKey);
        }
        return UpdatePathResult::Ok;
    }

    // No snapshots registered for the old path; nothing to re-point.
    return UpdatePathResult::Ok;
}

void SnapshotManager::ensureDir() {
    QString bd = baseDir();
    DiskUtils::ensureDir(bd);
}

QString SnapshotManager::ensureStorageReady(const QString& filePath) {
    ensureDir();
    const QString path = normalizePath(filePath);

    QMutexLocker locker(&s_opMutex);
    auto         key = keyForPath(path);
    if (!key) {
        const QString newKey = QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (!SnapshotDatabase::instance().registerImage(newKey, path)) {
            qWarning() << "[SnapshotManager] Failed to register image" << path;
            return QString();
        }
        // Re-fetch the canonical key: if a concurrent registration of the same
        // path won the race, INSERT OR IGNORE kept that key instead of ours.
        key = keyForPath(path);
        if (!key) {
            qWarning() << "[SnapshotManager] Failed to register image" << path;
            return QString();
        }
    }

    QString sd = snapshotDirPath(*key);
    if (!DiskUtils::ensureDir(sd)) {
        return QString();
    }
    return sd;
}

QString SnapshotManager::computeChecksum(const QImage& image) {
    QByteArray ba;
    QBuffer    buffer(&ba);
    if (!image.save(&buffer, "PNG")) {
        qWarning() << "[SnapshotManager] Failed to save image to buffer for checksum";
        return {};
    }
    return QString::fromUtf8(QCryptographicHash::hash(ba, QCryptographicHash::Sha256).toHex());
}

QVector<ImageSnapshot> SnapshotManager::loadSnapshots(const QString& filePath) {
    QMutexLocker locker(&s_cacheMutex);
    auto         keyOpt = keyForPathLocked(filePath);
    if (!keyOpt)
        return {};
    const QString& key = *keyOpt;

    if (s_snapshotsCache.contains(key)) {
        return s_snapshotsCache.value(key);
    }

    QVector<ImageSnapshot> snapshots = SnapshotDatabase::instance().getSnapshots(key);
    sortSnapshots(snapshots);

    s_snapshotsCache[key] = snapshots;
    return snapshots;
}

std::optional<QVector<ImageSnapshot>>
SnapshotManager::snapshotChain(const QVector<ImageSnapshot>& snapshots,
                               const ImageSnapshot&          target) {
    QHash<QUuid, const ImageSnapshot *> byUuid;
    for (const auto& s : snapshots)
        byUuid.insert(s.uuid, &s);

    auto targetIt = byUuid.constFind(target.uuid);
    if (targetIt == byUuid.constEnd())
        return std::nullopt;
    const ImageSnapshot *current = targetIt.value();

    QVector<ImageSnapshot> chain;
    while (true) {
        chain.prepend(*current);
        if ((*current).isBase)
            break;

        auto parent = byUuid.constFind((*current).parentUuid);
        if (parent == byUuid.constEnd()) {
            qWarning() << "[SnapshotManager] Broken snapshot chain: parent"
                       << (*current).parentUuid.toString(QUuid::WithoutBraces) << "not found";
            return std::nullopt;
        }
        current = parent.value();
    }
    return chain;
}

void SnapshotManager::sortSnapshots(QVector<ImageSnapshot>& snapshots) {
    // Stable: equal timestamps keep database insertion order.
    std::stable_sort(
        snapshots.begin(), snapshots.end(), [](const ImageSnapshot& a, const ImageSnapshot& b) {
            return a.timestamp < b.timestamp;
        });
}

std::optional<SnapshotManager::SaveResult> SnapshotManager::saveSnapshot(const QString& filePath,
                                                                         const QImage&  image) {
    QMutexLocker locker(&s_opMutex);
    QString      checksum = computeChecksum(image);
    if (checksum.isEmpty()) {
        qWarning() << "[SnapshotManager] Refused to save: image has null pixel data or save failed";
        return std::nullopt;
    }

    QVector<ImageSnapshot> snapshots = loadSnapshots(filePath);

    // Skip if latest snapshot is identical
    if (!snapshots.isEmpty()) {
        const ImageSnapshot& last = snapshots.last();
        if (last.checksum == checksum) {
            qDebug() << "[SnapshotManager] Duplicate of latest snapshot skipped for" << filePath
                     << "(" << last.uuid.toString(QUuid::WithoutBraces) << ")";
            return SaveResult{SaveStatus::Existing, last.uuid};
        }
    }

    QString sd = ensureStorageReady(filePath);
    if (sd.isEmpty()) {
        return std::nullopt;
    }

    auto keyOpt = keyForPath(filePath);
    if (!keyOpt) {
        return std::nullopt;
    }
    const QString& key = *keyOpt;

    ImageSnapshot s;
    s.uuid = QUuid::createUuid();
    s.timestamp = QDateTime::currentDateTimeUtc();
    s.checksum = checksum;

    if (!snapshots.isEmpty()) {
        s.parentUuid = snapshots.last().uuid;
    }

    bool shouldBeBase = snapshots.isEmpty() ||
                        ((static_cast<int>(snapshots.size())) % AppSettings::baseInterval() == 0);

    QImage prevImg;
    if (!snapshots.isEmpty()) {
        prevImg = reconstructSnapshot(filePath, snapshots.last().uuid);
    }

    if (!shouldBeBase && !prevImg.isNull()) {
        if (prevImg.size() != image.size()) {
            shouldBeBase = true;
        }
    }

    s.isBase = shouldBeBase;

    if (s.isBase) {
        s.fileName = QString("%1.png").arg(s.uuid.toString(QUuid::WithoutBraces));
        QString imgPath = sd + '/' + s.fileName;
        if (!saveImageFile(imgPath, image.convertToFormat(QImage::Format_ARGB32))) {
            qWarning() << "[SnapshotManager] Failed to create snapshot file:" << imgPath;
            return std::nullopt;
        }
    } else {
        s.fileName = QString("%1.delta").arg(s.uuid.toString(QUuid::WithoutBraces));

        if (prevImg.isNull()) {
            qWarning() << "[SnapshotManager] Failed to reconstruct previous image for delta";
            return std::nullopt;
        }

        QByteArray delta = computeDelta(image, prevImg);
        if (delta.isEmpty()) {
            qWarning() << "[SnapshotManager] Failed to compute delta for" << s.fileName;
            return std::nullopt;
        }

        QString deltaPath = sd + '/' + s.fileName;
        if (!saveFile(deltaPath, delta)) {
            qWarning() << "[SnapshotManager] Failed to save delta file:" << deltaPath;
            return std::nullopt;
        }
    }

    // Trigger thumbnail generation via the manager
    ThumbnailManager::instance().enqueueRequest(
        {.index = static_cast<int>(snapshots.size()) + 1, .filePath = filePath, .uuid = s.uuid});

    if (!SnapshotDatabase::instance().addSnapshot(key, s)) {
        qWarning() << "[SnapshotManager] Failed to record snapshot in database for" << filePath;
        QFile::remove(sd + '/' + s.fileName);
        return std::nullopt;
    }

    qDebug() << "[SnapshotManager] Saved snapshot" << s.uuid.toString(QUuid::WithoutBraces)
             << (s.isBase ? "(base)" : "(delta)") << "for" << filePath;
    snapshots.append(s);
    {
        QMutexLocker cacheLocker(&s_cacheMutex);
        s_snapshotsCache[key] = snapshots;
    }

    return SaveResult{SaveStatus::Created, s.uuid};
}

std::optional<SnapshotManager::BaseImage> SnapshotManager::loadBaseImage(const QString& filePath,
                                                                         const ImageSnapshot& s) {
    if (!s.isBase)
        return std::nullopt;

    auto keyOpt = keyForPath(filePath);
    if (!keyOpt)
        return std::nullopt;
    QString sd = snapshotDirPath(*keyOpt);
    QImage  img;
    if (img.load(sd + '/' + s.fileName)) {
        return BaseImage{img, s.checksum};
    }
    return std::nullopt;
}

std::optional<QByteArray> SnapshotManager::loadDelta(const QString&       filePath,
                                                     const ImageSnapshot& s) {
    if (s.isBase)
        return std::nullopt;

    auto keyOpt = keyForPath(filePath);
    if (!keyOpt)
        return std::nullopt;
    QString sd = snapshotDirPath(*keyOpt);
    QFile   f(sd + '/' + s.fileName);
    if (f.open(QFile::ReadOnly)) {
        return f.readAll();
    }
    return std::nullopt;
}

void SnapshotManager::deleteAllSnapshots(const QString& filePath) {
    QMutexLocker locker(&s_opMutex);

    auto keyOpt = keyForPath(filePath);
    if (!keyOpt)
        return;
    const QString& key = *keyOpt;

    // Clear the database first (atomic, cascades to snapshots) so that
    // concurrent readers resolve to an empty history before the files go.
    SnapshotDatabase::instance().clearImage(key);

    QString sd = snapshotDirPath(key);
    QDir    dir(sd);
    if (dir.exists()) {
        if (!dir.removeRecursively()) {
            qWarning() << "[SnapshotS"
                          "tore] "
                          "Failed to "
                          "delete "
                          "snapshot "
                          "directory:"
                       << sd;
        } else {
            qDebug() << "[SnapshotS"
                        "tore] "
                        "Deleted "
                        "all "
                        "snapshots "
                        "for"
                     << filePath;
        }
    }
    {
        QMutexLocker cacheLocker(&s_cacheMutex);
        s_snapshotsCache.remove(key);
        s_pathKeyCache.remove(normalizePath(filePath));
    }
}

void SnapshotManager::clearCache() {
    QMutexLocker locker(&s_cacheMutex);
    s_snapshotsCache.clear();
}

std::optional<QVector<ImageSnapshot>> SnapshotManager::deleteSnapshot(const QString& filePath,
                                                                      const QUuid&   uuid) {
    return deleteSnapshots(filePath, {uuid});
}

std::optional<QVector<ImageSnapshot>>
SnapshotManager::deleteSnapshots(const QString& filePath, const QVector<QUuid>& uuids) {
    QMutexLocker locker(&s_opMutex);
    auto         keyOpt = keyForPath(filePath);
    if (!keyOpt)
        return std::nullopt;
    const QString&         key = *keyOpt;
    const QString          sd = snapshotDirPath(key);
    QVector<ImageSnapshot> snapshots = loadSnapshots(filePath);

    QSet<QUuid> requested(uuids.begin(), uuids.end());
    QSet<QUuid> targets;
    for (const auto& s : snapshots) {
        if (requested.contains(s.uuid))
            targets.insert(s.uuid);
    }

    if (targets.isEmpty()) {
        qWarning() << "[SnapshotManager] No snapshots found for deletion:" << filePath;
        return std::nullopt;
    }

    // Repair the snapshot chain
    QVector<ImageSnapshot *> dependents;
    for (int i = 0; i < snapshots.size(); ++i) {
        const ImageSnapshot& s = snapshots[i];
        if (!s.parentUuid.isNull() && !targets.contains(s.uuid) && targets.contains(s.parentUuid))
            dependents.append(&snapshots[i]);
    }

    for (auto *dependent : dependents) {
        ImageSnapshot& next = *dependent;

        // Nearest ancestor that is not being deleted, if any.
        const ImageSnapshot *survivor = nullptr;
        QUuid                cursor = next.parentUuid;
        QSet<QUuid>          visited;
        while (!cursor.isNull() && !visited.contains(cursor)) {
            visited.insert(cursor);
            bool found = false;
            for (const auto& s : snapshots) {
                if (s.uuid == cursor) {
                    found = true;
                    if (!targets.contains(cursor))
                        survivor = &s;
                    cursor = s.parentUuid;
                    break;
                }
            }
            if (!found)
                break;
        }

        QImage imgNext = reconstructSnapshot(filePath, next.uuid);
        if (imgNext.isNull()) {
            qWarning()
                << "[SnapshotManager] Failed to reconstruct dependent snapshot for chain repair:"
                << next.uuid.toString(QUuid::WithoutBraces);
            return std::nullopt;
        }

        const QString oldFileName = next.fileName;

        if (!survivor) {
            // The deleted run reached the base. The dependent becomes the new base.
            next.isBase = true;
            next.fileName = QString("%1.png").arg(next.uuid.toString(QUuid::WithoutBraces));
            next.parentUuid = QUuid();

            if (!saveImageFile(sd + '/' + next.fileName, imgNext)) {
                qWarning() << "[SnapshotManager] Failed to save new base image during chain repair:"
                           << next.fileName;
                return std::nullopt;
            }
        } else {
            // Rebase the dependent against the nearest surviving ancestor.
            QImage imgParent = reconstructSnapshot(filePath, survivor->uuid);
            if (imgParent.isNull()) {
                qWarning()
                    << "[SnapshotManager] Failed to reconstruct surviving parent for rebasing:"
                    << survivor->uuid.toString(QUuid::WithoutBraces);
                return std::nullopt;
            }

            QByteArray delta = computeDelta(imgNext, imgParent);
            if (delta.isEmpty()) {
                qWarning()
                    << "[SnapshotManager] Failed to compute rebase delta during chain repair";
                return std::nullopt;
            }

            next.isBase = false;
            next.fileName = QString("%1.delta").arg(next.uuid.toString(QUuid::WithoutBraces));
            next.parentUuid = survivor->uuid;

            if (!saveFile(sd + '/' + next.fileName, delta)) {
                qWarning() << "[SnapshotManager] Failed to save rebase delta during chain repair:"
                           << next.fileName;
                return std::nullopt;
            }
        }

        if (oldFileName != next.fileName)
            QFile::remove(sd + '/' + oldFileName);
    }

    // Delete the target snapshot files
    for (const auto& s : snapshots) {
        if (targets.contains(s.uuid))
            QFile::remove(sd + '/' + s.fileName);
    }

    {
        SnapshotDbTransaction tx;
        bool                  dbOk = true;
        for (const auto *dependent : dependents) {
            dbOk = SnapshotDatabase::instance().updateSnapshot(key, *dependent) && dbOk;
        }
        for (const auto& s : snapshots) {
            if (targets.contains(s.uuid)) {
                dbOk = SnapshotDatabase::instance().removeSnapshot(key, s.uuid) && dbOk;
            }
        }
        if (!dbOk) {
            tx.rollback();
            qWarning() << "[SnapshotManager] Failed to persist snapshot deletion for" << filePath;
        }
    }

    // Update in-memory cache
    for (int i = snapshots.size() - 1; i >= 0; --i) {
        if (targets.contains(snapshots[i].uuid))
            snapshots.removeAt(i);
    }
    {
        QMutexLocker cacheLocker(&s_cacheMutex);
        s_snapshotsCache[key] = snapshots;
    }
    return snapshots;
}
