#include <QtTest>
#include "config/appsettings.h"
#include "core/snapshotmanager.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFuture>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtConcurrent>
#include <zip.h>

#include <sys/stat.h>
#include <unistd.h>
#include "core/deltacache.h"
#include "core/snapshotdb.h"
#include "core/vulkancontext.h"

class TestSnapshotManager : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();

    // Checksum
    void testChecksumNullImage();
    void testChecksumDeterministic();
    void testChecksumDifferent();
    void testChecksumSameImage();

    // Save / Load cycle
    void testSaveFirstSnapshot();
    void testLoadSnapshots();
    void testMultipleSnapshots();
    void testSizeChangeTriggersBase();
    void testBaseIntervalLogic();

    // Reconstruction & Resizing
    void testReconstructSnapshot();
    void testSparseDeltaReconstruction();
    void testResizeImage();

    // Dedup
    void testSaveDuplicateSkipped();

    // Export / Import
    void testExportImportBasic();
    void testExportImportMerge();
    void testImportInterleavedByTimestamp();
    void testSaveAfterImportParentsOnNewest();
    void testDeleteAcrossChains();
    void testSnapshotChain();

    // Delete
    void testDeleteLastSnapshot();
    void testDeleteMiddleSnapshotRebase();
    void testDeleteFirstSnapshotRebase();
    void testDeleteAllSnapshots();

    // Edge cases
    void testLoadNonExistentFile();
    void testCorruptedSnapshot();
    void testExtremeResolution();
    void testConcurrentSaves();

    // Path registry
    void testKeyForPathUnregistered();
    void testKeyForPathRegisteredUuidShape();
    void testUpdateImagePath();
    void testUpdateImagePathCollision();
    void testPathNormalization();

  private:
    // Each test gets a unique file path so snapshot keys don't collide.
    static QString testFilePath(const QString& suffix);
    static QImage  makeImage(int width, int height, QColor color);
    static QImage makeGradientImage(int width, int height, int offset);

    // Build a bundle from an existing snapshot chain, optionally remapping
    // uuids and timestamps. Unmapped entries keep their originals.
    static bool makeBundleFromChain(const QString&            sourcePath,
                                    const QString&            bundlePath,
                                    const QHash<QUuid, QUuid>& uuidRemap = {},
                                    const QHash<QUuid, QDateTime>& timeRemap = {});
};

void TestSnapshotManager::initTestCase() {
    // Use a unique temporary database for this test case to avoid collisions with other tests.
    QString tempDb = QDir::tempPath() + "/test_snapshot_manager_" +
                     QString::number(QRandomGenerator::global()->generate()) + ".db";
    SnapshotDatabase::instance().init(tempDb);

    // Clear the delta cache to ensure a clean environment.
    DeltaCache::clear();

    // Initialize Vulkan context to enable GPU-accelerated reconstruction tests.
    if (!VulkanContext::instance().initializeInstance()) {
        qWarning() << "VulkanContext failed to initialize. GPU tests may fail.";
    }
}

void TestSnapshotManager::cleanupTestCase() {
    // Clean up any leftover test data — not strictly necessary
    // since each test uses unique paths, but good practice.
}

QString TestSnapshotManager::testFilePath(const QString& suffix) {
    // Use a path that looks like a real file path but doesn't need to
    // actually exist — it's only used to generate a snapshot key.
    return QString("/tmp/ottersnap_test_%1/file.png").arg(suffix);
}

QImage TestSnapshotManager::makeImage(int width, int height, QColor color) {
    QImage img(width, height, QImage::Format_ARGB32);
    img.fill(color);
    return img;
}

QImage TestSnapshotManager::makeGradientImage(int width, int height, int offset) {
    QImage img(width, height, QImage::Format_ARGB32);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int v = (x * 7 + y * 13 + offset) % 256;
            img.setPixel(x, y, qRgb(v, (v * 3) % 256, (v * 5) % 256));
        }
    }
    return img;
}

bool TestSnapshotManager::makeBundleFromChain(const QString&             sourcePath,
                                              const QString&             bundlePath,
                                              const QHash<QUuid, QUuid>& uuidRemap,
                                              const QHash<QUuid, QDateTime>& timeRemap) {
    QVector<ImageSnapshot> snaps = SnapshotManager::loadSnapshots(sourcePath);
    if (snaps.isEmpty())
        return false;

    const QString sd =
        SnapshotManager::baseDir() + '/' + SnapshotManager::cacheKeyForPath(sourcePath);
    if (!QDir(sd).exists())
        return false;

    QJsonArray array;
    for (const auto& s : snaps) {
        QUuid uuid = uuidRemap.value(s.uuid, s.uuid);
        QUuid parent = s.parentUuid;
        if (!parent.isNull())
            parent = uuidRemap.value(parent, parent);

        QJsonObject obj;
        obj["uuid"] = uuid.toString(QUuid::WithoutBraces);
        obj["parentUuid"] = parent.toString(QUuid::WithoutBraces);
        obj["file"] = s.fileName;
        obj["timestamp"] = timeRemap.value(s.uuid, s.timestamp).toString(Qt::ISODate);
        obj["checksum"] = s.checksum;
        obj["isBase"] = s.isBase;
        array.append(obj);
    }

    QTemporaryDir dir;
    if (!dir.isValid())
        return false;

    QFile metaFile(dir.path() + "/metadata.json");
    if (!metaFile.open(QIODevice::WriteOnly))
        return false;
    metaFile.write(QJsonDocument(array).toJson());
    metaFile.close();

    int    err = 0;
    zip_t *archive = zip_open(bundlePath.toUtf8().constData(), ZIP_CREATE | ZIP_TRUNCATE, &err);
    if (!archive)
        return false;

    QByteArray metaPath = metaFile.fileName().toUtf8();
    zip_source_t *metaSource = zip_source_file(archive, metaPath.constData(), 0, 0);
    if (!metaSource || zip_file_add(archive, "metadata.json", metaSource, 0) < 0)
        return false;

    for (const auto& s : snaps) {
        QByteArray filePath = (sd + '/' + s.fileName).toUtf8();
        zip_source_t *fileSource = zip_source_file(archive, filePath.constData(), 0, 0);
        if (!fileSource || zip_file_add(archive, ("files/" + s.fileName).toUtf8().constData(),
                                        fileSource,
                                        0) < 0) {
            return false;
        }
    }

    zip_close(archive);
    return true;
}

// Checksum

void TestSnapshotManager::testChecksumNullImage() {
    QImage  img;
    QString checksum = SnapshotManager::computeChecksum(img);
    QVERIFY(checksum.isEmpty());
}

void TestSnapshotManager::testChecksumDeterministic() {
    QImage  img = makeImage(10, 10, Qt::red);
    QString cs1 = SnapshotManager::computeChecksum(img);
    QString cs2 = SnapshotManager::computeChecksum(img);
    QCOMPARE(cs1, cs2);
    QVERIFY(!cs1.isEmpty());
}

void TestSnapshotManager::testChecksumDifferent() {
    QImage img1 = makeImage(10, 10, Qt::red);
    QImage img2 = makeImage(10, 10, Qt::blue);
    QVERIFY(SnapshotManager::computeChecksum(img1) != SnapshotManager::computeChecksum(img2));
}

void TestSnapshotManager::testChecksumSameImage() {
    QImage img1 = makeImage(10, 10, Qt::green);
    QImage img2 = makeImage(10, 10, Qt::green);
    QCOMPARE(SnapshotManager::computeChecksum(img1), SnapshotManager::computeChecksum(img2));
}

// Save / Load cycle

void TestSnapshotManager::testSaveFirstSnapshot() {
    QString path = testFilePath("save1");
    SnapshotManager::deleteAllSnapshots(path);
    QImage img = makeImage(20, 20, Qt::magenta);

    auto snap = SnapshotManager::saveSnapshot(path, img);
    QVERIFY(snap.has_value());
    QVERIFY(!snap->uuid.isNull());
}

void TestSnapshotManager::testLoadSnapshots() {
    QString path = testFilePath("load1");
    SnapshotManager::deleteAllSnapshots(path);
    QImage img = makeImage(20, 20, Qt::cyan);
    SnapshotManager::saveSnapshot(path, img);

    QVector<ImageSnapshot> snapshots = SnapshotManager::loadSnapshots(path);
    QCOMPARE(snapshots.size(), 1);
    QVERIFY(!snapshots[0].uuid.isNull());
    QVERIFY(!snapshots[0].fileName.isEmpty());
    QVERIFY(!snapshots[0].checksum.isEmpty());
    QVERIFY(!snapshots[0].timestamp.isNull());
}

void TestSnapshotManager::testMultipleSnapshots() {
    QString path = testFilePath("multi");
    SnapshotManager::deleteAllSnapshots(path);

    SnapshotManager::saveSnapshot(path, makeImage(10, 10, Qt::red));
    SnapshotManager::saveSnapshot(path, makeImage(10, 10, Qt::green));
    SnapshotManager::saveSnapshot(path, makeImage(10, 10, Qt::blue));

    QVector<ImageSnapshot> snapshots = SnapshotManager::loadSnapshots(path);
    QCOMPARE(snapshots.size(), 3);
    QVERIFY(!snapshots[0].uuid.isNull());
    QVERIFY(!snapshots[1].uuid.isNull());
    QVERIFY(!snapshots[2].uuid.isNull());
}

void TestSnapshotManager::testSizeChangeTriggersBase() {
    QString path = testFilePath("sizeChangeBase");
    SnapshotManager::deleteAllSnapshots(path);

    // Set interval high so we don't trigger a base by index
    AppSettings::setBaseInterval(100);

    // Save first image (Base)
    QImage img1 = makeImage(100, 100, Qt::red);
    SnapshotManager::saveSnapshot(path, img1);

    // Save second image of same size (Delta)
    QImage img2 = makeImage(100, 100, Qt::blue);
    SnapshotManager::saveSnapshot(path, img2);

    // Save third image of DIFFERENT size (Should trigger Base)
    QImage img3 = makeImage(200, 200, Qt::green);
    SnapshotManager::saveSnapshot(path, img3);

    QVector<ImageSnapshot> snaps = SnapshotManager::loadSnapshots(path);
    QVERIFY(snaps.size() >= 3);
    QVERIFY(snaps[0].isBase);  // Snap 1
    QVERIFY(!snaps[1].isBase); // Snap 2
    QVERIFY(snaps[2].isBase);  // Snap 3 - should be base due to size change

    // Verify the metadata is correct
    QVERIFY(!snaps[2].uuid.isNull());
}

void TestSnapshotManager::testBaseIntervalLogic() {
    QString path = testFilePath("baseInterval");
    SnapshotManager::deleteAllSnapshots(path);

    // Set a small interval for testing
    int testInterval = 3;
    AppSettings::setBaseInterval(testInterval);

    // Save 5 different snapshots of the same size/format
    for (int i = 0; i < 5; ++i) {
        // Use different colors to avoid deduplication
        SnapshotManager::saveSnapshot(path, makeImage(10, 10, QColor(i * 50, 0, 0)));
    }

    QVector<ImageSnapshot> snaps = SnapshotManager::loadSnapshots(path);
    QCOMPARE(snaps.size(), 5);

    // Index 1: Always base
    QVERIFY(snaps[0].isBase);
    // Index 2: Delta
    QVERIFY(!snaps[1].isBase);
    // Index 3: Delta
    QVERIFY(!snaps[2].isBase);
    // Index 4: Base (since (4-1) % 3 == 0)
    QVERIFY(snaps[3].isBase);
    // Index 5: Delta
    QVERIFY(!snaps[4].isBase);

    // Reset to default
    AppSettings::setBaseInterval(100);
}

void TestSnapshotManager::testReconstructSnapshot() {
    QString path = testFilePath("reconstruct");
    SnapshotManager::deleteAllSnapshots(path);

    QImage img1 = makeImage(100, 100, Qt::red);
    QImage img2 = makeImage(100, 100, Qt::blue);

    QUuid id1 = SnapshotManager::saveSnapshot(path, img1)->uuid;
    QTest::qWait(500);
    QUuid id2 = SnapshotManager::saveSnapshot(path, img2)->uuid;
    QTest::qWait(500);

    // Test base reconstruction first
    QImage restored1 = SnapshotManager::reconstructSnapshot(path, id1);
    QCOMPARE(restored1.size(), img1.size());
    QCOMPARE(restored1.pixelColor(0, 0), img1.pixelColor(0, 0));

    // Test delta reconstruction
    QImage restored2 = SnapshotManager::reconstructSnapshot(path, id2);
    QCOMPARE(restored2.size(), img2.size());
    QCOMPARE(restored2.pixelColor(0, 0), img2.pixelColor(0, 0));
}

void TestSnapshotManager::testSparseDeltaReconstruction() {
    QString path = testFilePath("sparseDelta");
    SnapshotManager::deleteAllSnapshots(path);

    // Create a base image (solid red)
    QImage img1 = makeImage(100, 100, Qt::red);
    SnapshotManager::saveSnapshot(path, img1);
    QTest::qWait(500);

    // Create a second image with only a few pixels changed
    QImage img2 = img1;
    img2.setPixelColor(10, 10, Qt::blue);
    img2.setPixelColor(20, 20, Qt::green);
    img2.setPixelColor(30, 30, Qt::white);
    SnapshotManager::saveSnapshot(path, img2);
    QTest::qWait(500);

    // Reconstruct the second snapshot
    QUuid  id2 = SnapshotManager::saveSnapshot(path, img2)->uuid;
    QImage restored = SnapshotManager::reconstructSnapshot(path, id2);

    QCOMPARE(restored.size(), img2.size());
    QCOMPARE(restored.pixelColor(10, 10), Qt::blue);
    QCOMPARE(restored.pixelColor(20, 20), Qt::green);
    QCOMPARE(restored.pixelColor(30, 30), Qt::white);
    QCOMPARE(restored.pixelColor(0, 0), Qt::red); // Unchanged pixel
}

void TestSnapshotManager::testResizeImage() {
    QImage img = makeImage(100, 100, Qt::green);
    QSize  targetSize(50, 50);

    QImage resized = SnapshotManager::resizeImage(img, targetSize);
    QCOMPARE(resized.size(), targetSize);
    QVERIFY(!resized.isNull());
}

void TestSnapshotManager::testSaveDuplicateSkipped() {
    QString path = testFilePath("dedup");
    SnapshotManager::deleteAllSnapshots(path);
    QImage img = makeImage(15, 15, Qt::darkGray);

    auto snap1 = SnapshotManager::saveSnapshot(path, img);
    QVERIFY(snap1.has_value());
    QVERIFY(!snap1->uuid.isNull());

    // Saving the same image again should return the existing snapshot UUID
    auto snap2 = SnapshotManager::saveSnapshot(path, img);
    QVERIFY(snap2.has_value());
    QCOMPARE(snap2->uuid, snap1->uuid);

    // Only one snapshot on disk
    QVector<ImageSnapshot> snapshots = SnapshotManager::loadSnapshots(path);
    QCOMPARE(snapshots.size(), 1);
}

void TestSnapshotManager::testExportImportBasic() {
    QString sourcePath = testFilePath("export_src");
    QString destPath = testFilePath("export_dest");
    QString bundlePath = QDir::tempPath() + "/test_bundle.snaphist";

    SnapshotManager::deleteAllSnapshots(sourcePath);
    SnapshotManager::deleteAllSnapshots(destPath);

    // Create snapshots in source
    QUuid uuid1 = SnapshotManager::saveSnapshot(sourcePath, makeImage(10, 10, Qt::red))->uuid;
    QUuid uuid2 = SnapshotManager::saveSnapshot(sourcePath, makeImage(10, 10, Qt::blue))->uuid;

    // Export
    QVERIFY(SnapshotManager::exportHistory(sourcePath, bundlePath));

    // Import to destination
    QVERIFY(SnapshotManager::importHistory(destPath, bundlePath));

    // Verify
    QVector<ImageSnapshot> imported = SnapshotManager::loadSnapshots(destPath);
    QCOMPARE(imported.size(), 2);
    QCOMPARE(imported[0].uuid, uuid1);
    QCOMPARE(imported[1].uuid, uuid2);

    // Verify reconstruction works
    QImage res1 = SnapshotManager::reconstructSnapshot(destPath, uuid1);
    QCOMPARE(res1.pixelColor(0, 0), Qt::red);
    QImage res2 = SnapshotManager::reconstructSnapshot(destPath, uuid2);
    QCOMPARE(res2.pixelColor(0, 0), Qt::blue);
}

void TestSnapshotManager::testExportImportMerge() {
    QString sourcePath = testFilePath("merge_src");
    QString destPath = testFilePath("merge_dest");
    QString bundlePath = QDir::tempPath() + "/merge_bundle.snaphist";

    SnapshotManager::deleteAllSnapshots(sourcePath);
    SnapshotManager::deleteAllSnapshots(destPath);

    // Source snapshots (A -> B), created first (older timestamps)
    QUuid uuidA = SnapshotManager::saveSnapshot(sourcePath, makeImage(10, 10, Qt::red))->uuid;
    QUuid uuidB = SnapshotManager::saveSnapshot(sourcePath, makeImage(10, 10, Qt::blue))->uuid;

    // Export
    QVERIFY(SnapshotManager::exportHistory(sourcePath, bundlePath));

    // Destination has one snapshot (C), created later (newer timestamp)
    QUuid uuidC = SnapshotManager::saveSnapshot(destPath, makeImage(10, 10, Qt::green))->uuid;

    // Import: chains merge chronologically, no re-linking
    QVERIFY(SnapshotManager::importHistory(destPath, bundlePath));

    QVector<ImageSnapshot> snapshots = SnapshotManager::loadSnapshots(destPath);
    QCOMPARE(snapshots.size(), 3);
    QCOMPARE(snapshots[0].uuid, uuidA);
    QCOMPARE(snapshots[1].uuid, uuidB);
    QCOMPARE(snapshots[2].uuid, uuidC);

    // Parent links of both chains are preserved: A and C are independent bases.
    QVERIFY(snapshots[0].parentUuid.isNull());
    QVERIFY(snapshots[0].isBase);
    QCOMPARE(snapshots[1].parentUuid, uuidA);
    QVERIFY(!snapshots[1].isBase);
    QVERIFY(snapshots[2].parentUuid.isNull());
    QVERIFY(snapshots[2].isBase);

    // Re-import skips duplicates
    int duplicates = -1;
    QVERIFY(SnapshotManager::importHistory(destPath, bundlePath, &duplicates));
    QCOMPARE(duplicates, 2);
    QCOMPARE(SnapshotManager::loadSnapshots(destPath).size(), 3);

    // Reconstruction works across both chains
    QCOMPARE(SnapshotManager::reconstructSnapshot(destPath, uuidA).pixelColor(0, 0), Qt::red);
    QCOMPARE(SnapshotManager::reconstructSnapshot(destPath, uuidB).pixelColor(0, 0), Qt::blue);
    QCOMPARE(SnapshotManager::reconstructSnapshot(destPath, uuidC).pixelColor(0, 0), Qt::green);

    SnapshotManager::deleteAllSnapshots(sourcePath);
    SnapshotManager::deleteAllSnapshots(destPath);
    QFile::remove(bundlePath);
}

void TestSnapshotManager::testImportInterleavedByTimestamp() {
    const QString path = testFilePath("interleave");
    const QString localBundle = QDir::tempPath() + "/interleave_local.snaphist";
    const QString importBundle = QDir::tempPath() + "/interleave_import.snaphist";
    SnapshotManager::deleteAllSnapshots(path);

    // Two independent chains built on scratch paths so the bundles carry
    // real snapshot files, imported with explicit interleaved timestamps:
    // L0 (10:00) -> I0 (10:30) -> I1 (11:00) -> L1 (12:00).
    const QString scratchL = testFilePath("interleave_scratchL");
    const QString scratchI = testFilePath("interleave_scratchI");
    SnapshotManager::deleteAllSnapshots(scratchL);
    SnapshotManager::deleteAllSnapshots(scratchI);

    QImage l0 = makeGradientImage(64, 64, 0);
    QImage l1 = makeGradientImage(64, 64, 100);
    QUuid  sL0 = SnapshotManager::saveSnapshot(scratchL, l0)->uuid;
    QUuid  sL1 = SnapshotManager::saveSnapshot(scratchL, l1)->uuid;

    QImage i0 = makeGradientImage(64, 64, 300);
    QImage i1 = makeGradientImage(64, 64, 400);
    QUuid  sI0 = SnapshotManager::saveSnapshot(scratchI, i0)->uuid;
    QUuid  sI1 = SnapshotManager::saveSnapshot(scratchI, i1)->uuid;

    QUuid     uuidL0 = QUuid::createUuid();
    QUuid     uuidL1 = QUuid::createUuid();
    QUuid     uuidI0 = QUuid::createUuid();
    QUuid     uuidI1 = QUuid::createUuid();
    QDateTime t      = QDateTime::currentDateTimeUtc().addSecs(-7200);

    QVERIFY(makeBundleFromChain(
        scratchL, localBundle, {{sL0, uuidL0}, {sL1, uuidL1}},
        {{sL0, t}, {sL1, t.addSecs(7200)}}));
    QVERIFY(makeBundleFromChain(
        scratchI, importBundle, {{sI0, uuidI0}, {sI1, uuidI1}},
        {{sI0, t.addSecs(1800)}, {sI1, t.addSecs(3600)}}));

    QVERIFY(SnapshotManager::importHistory(path, localBundle));
    QVERIFY(SnapshotManager::importHistory(path, importBundle));

    // Timeline is interleaved by timestamp: L0(10:00) I0(10:30) I1(11:00) L1(12:00)
    QVector<ImageSnapshot> snaps = SnapshotManager::loadSnapshots(path);
    QCOMPARE(snaps.size(), 4);
    QCOMPARE(snaps[0].uuid, uuidL0);
    QCOMPARE(snaps[1].uuid, uuidI0);
    QCOMPARE(snaps[2].uuid, uuidI1);
    QCOMPARE(snaps[3].uuid, uuidL1);

    // Parent links: each chain is intact.
    QVERIFY(snaps[0].parentUuid.isNull());
    QVERIFY(snaps[0].isBase);
    QCOMPARE(snaps[3].parentUuid, uuidL0);
    QVERIFY(snaps[3].isBase == false);
    QVERIFY(snaps[1].parentUuid.isNull());
    QVERIFY(snaps[1].isBase);
    QCOMPARE(snaps[2].parentUuid, uuidI0);
    QVERIFY(!snaps[2].isBase);

    // Reconstruction of every snapshot yields its true content: the local
    // delta L1 must be applied to L0 (not to the older imported base I0).
    QCOMPARE(SnapshotManager::reconstructSnapshot(path, uuidL0), l0);
    QCOMPARE(SnapshotManager::reconstructSnapshot(path, uuidL1), l1);
    QCOMPARE(SnapshotManager::reconstructSnapshot(path, uuidI0), i0);
    QCOMPARE(SnapshotManager::reconstructSnapshot(path, uuidI1), i1);

    SnapshotManager::deleteAllSnapshots(path);
    SnapshotManager::deleteAllSnapshots(scratchL);
    SnapshotManager::deleteAllSnapshots(scratchI);
    QFile::remove(localBundle);
    QFile::remove(importBundle);
}

void TestSnapshotManager::testSaveAfterImportParentsOnNewest() {
    const QString path = testFilePath("save_after_import");
    const QString bundlePath = QDir::tempPath() + "/save_after_import.snaphist";
    SnapshotManager::deleteAllSnapshots(path);

    // Import a two-snapshot chain (A -> B) built on a scratch path.
    QImage img0 = makeImage(10, 10, Qt::red);
    QImage img1 = makeImage(10, 10, Qt::blue);
    const QString scratch = testFilePath("save_after_import_scratch");
    SnapshotManager::deleteAllSnapshots(scratch);
    QUuid sA = SnapshotManager::saveSnapshot(scratch, img0)->uuid;
    QUuid sB = SnapshotManager::saveSnapshot(scratch, img1)->uuid;

    QUuid uuidA = QUuid::createUuid();
    QUuid uuidB = QUuid::createUuid();
    QVERIFY(makeBundleFromChain(scratch, bundlePath, {{sA, uuidA}, {sB, uuidB}}));

    QVERIFY(SnapshotManager::importHistory(path, bundlePath));

    // A new snapshot parents onto the newest snapshot (the imported head).
    QImage img2 = makeImage(10, 10, Qt::green);
    auto     res = SnapshotManager::saveSnapshot(path, img2);
    QVERIFY(res.has_value());

    QVector<ImageSnapshot> snaps = SnapshotManager::loadSnapshots(path);
    QCOMPARE(snaps.size(), 3);
    QCOMPARE(snaps.last().uuid, res->uuid);
    QCOMPARE(snaps.last().parentUuid, uuidB);
    QVERIFY(!snaps.last().isBase);

    // Reconstruction walks the imported chain: base A + delta B + new delta.
    QCOMPARE(SnapshotManager::reconstructSnapshot(path, res->uuid), img2);

    SnapshotManager::deleteAllSnapshots(path);
    SnapshotManager::deleteAllSnapshots(scratch);
    QFile::remove(bundlePath);
}

void TestSnapshotManager::testDeleteAcrossChains() {
    const QString path = testFilePath("delete_chains");
    const QString bundlePath = QDir::tempPath() + "/delete_chains.snaphist";
    SnapshotManager::deleteAllSnapshots(path);

    // Local chain: L0 (base) -> L1.
    QImage l0 = makeImage(10, 10, Qt::red);
    QImage l1 = makeImage(10, 10, Qt::blue);
    QUuid  uuidL0 = SnapshotManager::saveSnapshot(path, l0)->uuid;
    QUuid  uuidL1 = SnapshotManager::saveSnapshot(path, l1)->uuid;

    // Imported chain I0 (base) -> I1 with older timestamps.
    QImage i0 = makeImage(10, 10, Qt::green);
    QImage i1 = makeImage(10, 10, Qt::yellow);
    const QString scratch = testFilePath("delete_chains_scratch");
    SnapshotManager::deleteAllSnapshots(scratch);
    QUuid sI0 = SnapshotManager::saveSnapshot(scratch, i0)->uuid;
    QUuid sI1 = SnapshotManager::saveSnapshot(scratch, i1)->uuid;

    QUuid     uuidI0 = QUuid::createUuid();
    QUuid     uuidI1 = QUuid::createUuid();
    QDateTime older  = QDateTime::currentDateTimeUtc().addSecs(-7200);
    QVERIFY(makeBundleFromChain(
        scratch, bundlePath, {{sI0, uuidI0}, {sI1, uuidI1}},
        {{sI0, older}, {sI1, older.addSecs(1800)}}));
    QVERIFY(SnapshotManager::importHistory(path, bundlePath));

    // Delete I1 (a chain head): no repair, L-chain untouched.
    QVERIFY(SnapshotManager::deleteSnapshot(path, uuidI1));
    QVector<ImageSnapshot> after = SnapshotManager::loadSnapshots(path);
    QCOMPARE(after.size(), 3);
    for (const auto& s : after) {
        if (s.uuid == uuidL1)
            QCOMPARE(s.parentUuid, uuidL0);
    }

    // Delete I0 (a base): no dependent, no repair.
    QVERIFY(SnapshotManager::deleteSnapshot(path, uuidI0));
    after = SnapshotManager::loadSnapshots(path);
    QCOMPARE(after.size(), 2);
    QCOMPARE(after[0].uuid, uuidL0);
    QCOMPARE(after[1].uuid, uuidL1);

    // Delete L0 (the local base): L1 must be rebased within its own chain.
    QVERIFY(SnapshotManager::deleteSnapshot(path, uuidL0));
    after = SnapshotManager::loadSnapshots(path);
    QCOMPARE(after.size(), 1);
    QCOMPARE(after[0].uuid, uuidL1);
    QVERIFY(after[0].isBase);
    QVERIFY(after[0].parentUuid.isNull());
    QCOMPARE(SnapshotManager::reconstructSnapshot(path, uuidL1), l1);

    SnapshotManager::deleteAllSnapshots(path);
    SnapshotManager::deleteAllSnapshots(scratch);
    QFile::remove(bundlePath);
}

void TestSnapshotManager::testSnapshotChain() {
    const QString path = testFilePath("chain_walk");
    SnapshotManager::deleteAllSnapshots(path);

    SnapshotManager::saveSnapshot(path, makeImage(10, 10, Qt::red));
    SnapshotManager::saveSnapshot(path, makeImage(10, 10, Qt::blue));
    SnapshotManager::saveSnapshot(path, makeImage(10, 10, Qt::green));

    QVector<ImageSnapshot> snaps = SnapshotManager::loadSnapshots(path);
    QCOMPARE(snaps.size(), 3);

    auto chain = SnapshotManager::snapshotChain(snaps, snaps.last());
    QVERIFY(chain.has_value());
    QCOMPARE(chain->size(), 3);
    QVERIFY(chain->first().isBase);
    QCOMPARE(chain->last().uuid, snaps.last().uuid);

    // Chain of the base itself is a single node.
    auto baseChain = SnapshotManager::snapshotChain(snaps, snaps.first());
    QVERIFY(baseChain.has_value());
    QCOMPARE(baseChain->size(), 1);

    // Missing target yields nullopt.
    auto missing = SnapshotManager::snapshotChain(snaps, ImageSnapshot{QUuid::createUuid()});
    QVERIFY(!missing.has_value());

    SnapshotManager::deleteAllSnapshots(path);
}

void TestSnapshotManager::testDeleteLastSnapshot() {
    QString path = testFilePath("deleteLast");
    SnapshotManager::deleteAllSnapshots(path);

    SnapshotManager::saveSnapshot(path, makeImage(10, 10, Qt::red));
    SnapshotManager::saveSnapshot(path, makeImage(10, 10, Qt::blue));

    QVector<ImageSnapshot> snapshots = SnapshotManager::loadSnapshots(path);
    QCOMPARE(snapshots.size(), 2);

    // Delete the last one
    QUuid lastUuid = snapshots.last().uuid;
    QVERIFY(SnapshotManager::deleteSnapshot(path, lastUuid));

    QVector<ImageSnapshot> snapsAfter = SnapshotManager::loadSnapshots(path);
    QCOMPARE(snapsAfter.size(), 1);
    QVERIFY(!snapsAfter[0].uuid.isNull());
}

void TestSnapshotManager::testDeleteMiddleSnapshotRebase() {
    QString path = testFilePath("deleteMiddle");
    SnapshotManager::deleteAllSnapshots(path);

    QImage img1 = makeImage(10, 10, Qt::red);
    QImage img2 = makeImage(10, 10, Qt::green);
    QImage img3 = makeImage(10, 10, Qt::blue);

    SnapshotManager::saveSnapshot(path, img1);
    SnapshotManager::saveSnapshot(path, img2);
    SnapshotManager::saveSnapshot(path, img3);

    // Verify initial state: S1(Base), S2(Delta), S3(Delta)
    QVector<ImageSnapshot> snapshots = SnapshotManager::loadSnapshots(path);
    QCOMPARE(snapshots.size(), 3);
    QVERIFY(snapshots[0].isBase);
    QVERIFY(!snapshots[1].isBase);
    QVERIFY(!snapshots[2].isBase);

    // Delete S2 (middle)
    QUuid id2 = snapshots[1].uuid;
    QVERIFY(SnapshotManager::deleteSnapshot(path, id2));

    // Verify new state: S1(Base), S3(Delta of S1)
    QVector<ImageSnapshot> snaps = SnapshotManager::loadSnapshots(path);
    QCOMPARE(snaps.size(), 2);
    QVERIFY(!snaps[0].uuid.isNull());
    QVERIFY(!snaps[1].uuid.isNull());
    QVERIFY(snaps[0].isBase);
    QVERIFY(!snaps[1].isBase); // S3 should be rebased as a delta of S1

    // Verify structure is correct
    QVERIFY(!snaps[1].uuid.isNull());
    QVERIFY(!snaps[1].isBase);
}

void TestSnapshotManager::testDeleteFirstSnapshotRebase() {
    QString path = testFilePath("deleteFirst");
    SnapshotManager::deleteAllSnapshots(path);

    QImage img1 = makeImage(10, 10, Qt::red);
    QImage img2 = makeImage(10, 10, Qt::blue);

    SnapshotManager::saveSnapshot(path, img1);
    SnapshotManager::saveSnapshot(path, img2);

    // Delete S1 (the base)
    QVector<ImageSnapshot> snapshots = SnapshotManager::loadSnapshots(path);
    QUuid                  id1 = snapshots[0].uuid;
    QVERIFY(SnapshotManager::deleteSnapshot(path, id1));

    // Verify new state: S2(Base)
    QVector<ImageSnapshot> snaps = SnapshotManager::loadSnapshots(path);
    QCOMPARE(snaps.size(), 1);
    QVERIFY(!snaps[0].uuid.isNull());
    QVERIFY(snaps[0].isBase);

    // Verify S2 is now the base
    QVERIFY(snaps[0].isBase);
    QVERIFY(!snaps[0].uuid.isNull());
}

void TestSnapshotManager::testDeleteAllSnapshots() {
    QString path = testFilePath("delete");
    SnapshotManager::deleteAllSnapshots(path);
    SnapshotManager::saveSnapshot(path, makeImage(10, 10, Qt::red));
    SnapshotManager::saveSnapshot(path, makeImage(10, 10, Qt::blue));

    QCOMPARE(SnapshotManager::loadSnapshots(path).size(), 2);

    SnapshotManager::deleteAllSnapshots(path);
    QCOMPARE(SnapshotManager::loadSnapshots(path).size(), 0);
}

// Edge cases

void TestSnapshotManager::testLoadNonExistentFile() {
    QString path = testFilePath("nonexistent");
    SnapshotManager::deleteAllSnapshots(path);
    QVector<ImageSnapshot> snapshots = SnapshotManager::loadSnapshots(path);
    QVERIFY(snapshots.isEmpty());
}

void TestSnapshotManager::testCorruptedSnapshot() {
    QString path = testFilePath("corrupt");
    SnapshotManager::deleteAllSnapshots(path);

    QImage img = makeImage(100, 100, Qt::red);
    auto   res = SnapshotManager::saveSnapshot(path, img);
    QVERIFY(res.has_value());

    // Find the file on disk to corrupt it
    auto keyOpt = SnapshotManager::keyForPath(path);
    QVERIFY(keyOpt.has_value());
    QDir dir(SnapshotManager::baseDir() + "/" + *keyOpt);
    QStringList files = dir.entryList(QDir::Files);
    QVERIFY(!files.isEmpty());

    // Overwrite the first snapshot file with garbage
    QFile file(dir.absoluteFilePath(files[0]));
    if (file.open(QIODevice::WriteOnly)) {
        file.write("THIS IS CORRUPTED DATA");
        file.close();
    }

    // Reconstruction should not crash.
    // Depending on implementation, it might return a null image or partial.
    QImage restored = SnapshotManager::reconstructSnapshot(path, res->uuid);
    // We just verify it doesn't crash the process.
    Q_UNUSED(restored);
}

void TestSnapshotManager::testExtremeResolution() {
    // Test with a large image (e.g. 16k x 16k).
    // We use a small image if memory is an issue, but 16k is usually okay for GPU.
    QImage largeImg(16384, 16384, QImage::Format_ARGB32);
    largeImg.fill(Qt::blue);

    QSize  targetSize(256, 256);
    QImage resized = SnapshotManager::resizeImage(largeImg, targetSize);

    QCOMPARE(resized.size(), targetSize);
    QVERIFY(!resized.isNull());
}

void TestSnapshotManager::testConcurrentSaves() {
    QString path = testFilePath("concurrent");
    SnapshotManager::deleteAllSnapshots(path);

    const int                                                    numThreads = 8;
    QVector<QFuture<std::optional<SnapshotManager::SaveResult>>> futures;

    for (int i = 0; i < numThreads; ++i) {
        futures.append(QtConcurrent::run([path, i]() {
            // Each thread saves a different color to avoid deduplication
            QImage img(10, 10, QImage::Format_ARGB32);
            img.fill(QColor(i * 20, 0, 0));
            return SnapshotManager::saveSnapshot(path, img);
        }));
    }

    for (auto& f : futures) {
        auto res = f.result();
        QVERIFY(res.has_value());
    }

    QVector<ImageSnapshot> snaps = SnapshotManager::loadSnapshots(path);
    QCOMPARE(snaps.size(), numThreads);
}

void TestSnapshotManager::testKeyForPathUnregistered() {
    QString path = testFilePath("unregistered");
    SnapshotManager::deleteAllSnapshots(path);

    QVERIFY(!SnapshotManager::keyForPath(path).has_value());
}

void TestSnapshotManager::testKeyForPathRegisteredUuidShape() {
    QString path = testFilePath("uuidshape");
    SnapshotManager::deleteAllSnapshots(path);

    auto res = SnapshotManager::saveSnapshot(path, makeImage(16, 16, Qt::green));
    QVERIFY(res.has_value());

    auto key = SnapshotManager::keyForPath(path);
    QVERIFY(key.has_value());
    // New keys are UUID-shaped (dashes included, no braces).
    QCOMPARE(key->size(), 36);
    QVERIFY(!key->contains(QChar('{')));
    QVERIFY(!QUuid::fromString(*key).isNull());

    SnapshotManager::deleteAllSnapshots(path);
}

void TestSnapshotManager::testUpdateImagePath() {
    QString oldPath = testFilePath("move_old");
    QString newPath = testFilePath("move_new");
    SnapshotManager::deleteAllSnapshots(oldPath);
    SnapshotManager::deleteAllSnapshots(newPath);

    SnapshotManager::saveSnapshot(oldPath, makeImage(12, 12, Qt::red));
    SnapshotManager::saveSnapshot(oldPath, makeImage(12, 12, Qt::blue));

    QVector<ImageSnapshot> before = SnapshotManager::loadSnapshots(oldPath);
    QCOMPARE(before.size(), 2);
    auto oldKey = SnapshotManager::keyForPath(oldPath);
    QVERIFY(oldKey.has_value());

    QCOMPARE(SnapshotManager::updateImagePath(oldPath, newPath),
             SnapshotManager::UpdatePathResult::Ok);

    // Snapshots are visible under the new path with the same UUIDs.
    QVector<ImageSnapshot> after = SnapshotManager::loadSnapshots(newPath);
    QCOMPARE(after.size(), before.size());
    for (int i = 0; i < before.size(); ++i) {
        QCOMPARE(after[i].uuid, before[i].uuid);
    }

    // The old path no longer resolves.
    QVERIFY(!SnapshotManager::keyForPath(oldPath).has_value());
    QVERIFY(SnapshotManager::loadSnapshots(oldPath).isEmpty());

    // Storage stayed under the original key (no directory migration).
    QCOMPARE(SnapshotManager::keyForPath(newPath), oldKey);
    QVERIFY(QDir(SnapshotManager::baseDir() + "/" + *oldKey).exists());

    // The registry reflects the new path.
    bool found = false;
    for (const auto& rec : SnapshotDatabase::instance().getAllSnapshottedImages()) {
        if (rec.path == newPath) {
            found = true;
            QCOMPARE(rec.key, *oldKey);
        }
    }
    QVERIFY(found);

    SnapshotManager::deleteAllSnapshots(newPath);
}

void TestSnapshotManager::testUpdateImagePathCollision() {
    QString pathA = testFilePath("collision_a");
    QString pathB = testFilePath("collision_b");
    SnapshotManager::deleteAllSnapshots(pathA);
    SnapshotManager::deleteAllSnapshots(pathB);

    SnapshotManager::saveSnapshot(pathA, makeImage(8, 8, Qt::red));
    SnapshotManager::saveSnapshot(pathB, makeImage(8, 8, Qt::blue));

    auto keyA = SnapshotManager::keyForPath(pathA);
    auto keyB = SnapshotManager::keyForPath(pathB);
    QVERIFY(keyA.has_value());
    QVERIFY(keyB.has_value());
    QVERIFY(*keyA != *keyB);

    // Re-pointing A to B must fail: B is registered under a different key.
    QCOMPARE(SnapshotManager::updateImagePath(pathA, pathB),
             SnapshotManager::UpdatePathResult::TargetAlreadyRegistered);

    // Both histories are intact and unchanged.
    QCOMPARE(SnapshotManager::loadSnapshots(pathA).size(), 1);
    QCOMPARE(SnapshotManager::loadSnapshots(pathB).size(), 1);
    QCOMPARE(SnapshotManager::keyForPath(pathA), keyA);
    QCOMPARE(SnapshotManager::keyForPath(pathB), keyB);

    SnapshotManager::deleteAllSnapshots(pathA);
    SnapshotManager::deleteAllSnapshots(pathB);
}

void TestSnapshotManager::testPathNormalization() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString realPath = tempDir.filePath("real.png");
    QImage img(4, 4, QImage::Format_RGB32);
    img.fill(Qt::black);
    QVERIFY(img.save(realPath));

    SnapshotManager::saveSnapshot(realPath, img);
    auto realKey = SnapshotManager::keyForPath(realPath);
    QVERIFY(realKey.has_value());

    // A symlink variant of the path resolves to the same key.
    QString linkPath = tempDir.filePath("link.png");
    QVERIFY(::symlink(realPath.toLocal8Bit().constData(), linkPath.toLocal8Bit().constData()) == 0);
    QCOMPARE(SnapshotManager::keyForPath(linkPath), realKey);
    QCOMPARE(SnapshotManager::loadSnapshots(linkPath).size(), 1);

    SnapshotManager::deleteAllSnapshots(realPath);
}

QTEST_MAIN(TestSnapshotManager)
#include "test_snapshotmanager.moc"
