#include <QtTest>
#include "config/appsettings.h"
#include "core/snapshotmanager.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFuture>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtConcurrent>
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
    void testExportImportPrepend();

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

  private:
    // Each test gets a unique file path so snapshot keys don't collide.
    static QString testFilePath(const QString& suffix);
    static QImage  makeImage(int width, int height, QColor color);
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

void TestSnapshotManager::testExportImportPrepend() {
    QString sourcePath = testFilePath("prepend_src");
    QString destPath = testFilePath("prepend_dest");
    QString bundlePath = QDir::tempPath() + "/prepend_bundle.snaphist";

    SnapshotManager::deleteAllSnapshots(sourcePath);
    SnapshotManager::deleteAllSnapshots(destPath);

    // Source snapshots (A -> B)
    QUuid uuidA = SnapshotManager::saveSnapshot(sourcePath, makeImage(10, 10, Qt::red))->uuid;
    QUuid uuidB = SnapshotManager::saveSnapshot(sourcePath, makeImage(10, 10, Qt::blue))->uuid;

    // Export
    QVERIFY(SnapshotManager::exportHistory(sourcePath, bundlePath));

    // Destination has one snapshot (C)
    QUuid uuidC = SnapshotManager::saveSnapshot(destPath, makeImage(10, 10, Qt::green))->uuid;

    // Import Prepend
    QVERIFY(SnapshotManager::importHistory(destPath, bundlePath));

    // Verify chain: A -> B -> C
    QVector<ImageSnapshot> snapshots = SnapshotManager::loadSnapshots(destPath);
    QCOMPARE(snapshots.size(), 3);
    QCOMPARE(snapshots[0].uuid, uuidA);
    QCOMPARE(snapshots[1].uuid, uuidB);
    QCOMPARE(snapshots[2].uuid, uuidC);

    // Verify C is now a base image (bridge strategy)
    QVERIFY(snapshots[2].isBase);

    // Verify reconstruction of all
    QCOMPARE(SnapshotManager::reconstructSnapshot(destPath, uuidA).pixelColor(0, 0), Qt::red);
    QCOMPARE(SnapshotManager::reconstructSnapshot(destPath, uuidB).pixelColor(0, 0), Qt::blue);
    QCOMPARE(SnapshotManager::reconstructSnapshot(destPath, uuidC).pixelColor(0, 0), Qt::green);
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
    QString     key = SnapshotManager::imageKey(path);
    QDir        dir(SnapshotManager::baseDir() + "/" + key);
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

QTEST_MAIN(TestSnapshotManager)
#include "test_snapshotmanager.moc"
