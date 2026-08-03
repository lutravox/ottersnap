#include <QtTest>
#include "config/appsettings.h"
#include "core/snapshotstore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFuture>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtConcurrent>
#include "core/vulkancontext.h"

class TestSnapshotStore : public QObject {
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
    void testResizeImage();

    // Dedup
    void testSaveDuplicateSkipped();

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

void TestSnapshotStore::initTestCase() {
    // Initialize Vulkan context to enable GPU-accelerated reconstruction tests.
    if (!VulkanContext::instance().initializeInstance()) {
        qWarning() << "VulkanContext failed to initialize. GPU tests may fail.";
    }
}

void TestSnapshotStore::cleanupTestCase() {
    // Clean up any leftover test data — not strictly necessary
    // since each test uses unique paths, but good practice.
}

QString TestSnapshotStore::testFilePath(const QString& suffix) {
    // Use a path that looks like a real file path but doesn't need to
    // actually exist — it's only used to generate a snapshot key.
    return QString("/tmp/ottersnap_test_%1/file.png").arg(suffix);
}

QImage TestSnapshotStore::makeImage(int width, int height, QColor color) {
    QImage img(width, height, QImage::Format_ARGB32);
    img.fill(color);
    return img;
}

// Checksum

void TestSnapshotStore::testChecksumNullImage() {
    QImage  img;
    QString checksum = SnapshotStore::computeChecksum(img);
    QVERIFY(checksum.isEmpty());
}

void TestSnapshotStore::testChecksumDeterministic() {
    QImage  img = makeImage(10, 10, Qt::red);
    QString cs1 = SnapshotStore::computeChecksum(img);
    QString cs2 = SnapshotStore::computeChecksum(img);
    QCOMPARE(cs1, cs2);
    QVERIFY(!cs1.isEmpty());
}

void TestSnapshotStore::testChecksumDifferent() {
    QImage img1 = makeImage(10, 10, Qt::red);
    QImage img2 = makeImage(10, 10, Qt::blue);
    QVERIFY(SnapshotStore::computeChecksum(img1) != SnapshotStore::computeChecksum(img2));
}

void TestSnapshotStore::testChecksumSameImage() {
    QImage img1 = makeImage(10, 10, Qt::green);
    QImage img2 = makeImage(10, 10, Qt::green);
    QCOMPARE(SnapshotStore::computeChecksum(img1), SnapshotStore::computeChecksum(img2));
}

// Save / Load cycle

void TestSnapshotStore::testSaveFirstSnapshot() {
    QString path = testFilePath("save1");
    SnapshotStore::deleteAllSnapshots(path);
    QImage img = makeImage(20, 20, Qt::magenta);

    auto snap = SnapshotStore::saveSnapshot(path, img);
    QVERIFY(snap.has_value());
    QCOMPARE(snap->snapshotIndex, 1);
}

void TestSnapshotStore::testLoadSnapshots() {
    QString path = testFilePath("load1");
    SnapshotStore::deleteAllSnapshots(path);
    QImage img = makeImage(20, 20, Qt::cyan);
    SnapshotStore::saveSnapshot(path, img);

    QVector<ImageSnapshot> snapshots = SnapshotStore::loadSnapshots(path);
    QCOMPARE(snapshots.size(), 1);
    QCOMPARE(snapshots[0].snapshotIndex, 1);
    QVERIFY(!snapshots[0].fileName.isEmpty());
    QVERIFY(!snapshots[0].checksum.isEmpty());
    QVERIFY(!snapshots[0].timestamp.isNull());
}

void TestSnapshotStore::testMultipleSnapshots() {
    QString path = testFilePath("multi");
    SnapshotStore::deleteAllSnapshots(path);

    SnapshotStore::saveSnapshot(path, makeImage(10, 10, Qt::red));
    SnapshotStore::saveSnapshot(path, makeImage(10, 10, Qt::green));
    SnapshotStore::saveSnapshot(path, makeImage(10, 10, Qt::blue));

    QVector<ImageSnapshot> snapshots = SnapshotStore::loadSnapshots(path);
    QCOMPARE(snapshots.size(), 3);
    QCOMPARE(snapshots[0].snapshotIndex, 1);
    QCOMPARE(snapshots[1].snapshotIndex, 2);
    QCOMPARE(snapshots[2].snapshotIndex, 3);
}

void TestSnapshotStore::testSizeChangeTriggersBase() {
    QString path = testFilePath("sizeChangeBase");
    SnapshotStore::deleteAllSnapshots(path);

    // Set interval high so we don't trigger a base by index
    AppSettings::setBaseInterval(100);

    // Save first image (Base)
    QImage img1 = makeImage(100, 100, Qt::red);
    SnapshotStore::saveSnapshot(path, img1);

    // Save second image of same size (Delta)
    QImage img2 = makeImage(100, 100, Qt::blue);
    SnapshotStore::saveSnapshot(path, img2);

    // Save third image of DIFFERENT size (Should trigger Base)
    QImage img3 = makeImage(200, 200, Qt::green);
    SnapshotStore::saveSnapshot(path, img3);

    QVector<ImageSnapshot> snaps = SnapshotStore::loadSnapshots(path);
    QVERIFY(snaps.size() >= 3);
    QVERIFY(snaps[0].isBase);  // Snap 1
    QVERIFY(!snaps[1].isBase); // Snap 2
    QVERIFY(snaps[2].isBase);  // Snap 3 - should be base due to size change

    // Verify the metadata is correct
    QCOMPARE(snaps[2].snapshotIndex, 3);
}

void TestSnapshotStore::testBaseIntervalLogic() {
    QString path = testFilePath("baseInterval");
    SnapshotStore::deleteAllSnapshots(path);

    // Set a small interval for testing
    int testInterval = 3;
    AppSettings::setBaseInterval(testInterval);

    // Save 5 different snapshots of the same size/format
    for (int i = 0; i < 5; ++i) {
        // Use different colors to avoid deduplication
        SnapshotStore::saveSnapshot(path, makeImage(10, 10, QColor(i * 50, 0, 0)));
    }

    QVector<ImageSnapshot> snaps = SnapshotStore::loadSnapshots(path);
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

void TestSnapshotStore::testReconstructSnapshot() {
    QString path = testFilePath("reconstruct");
    SnapshotStore::deleteAllSnapshots(path);

    QImage img1 = makeImage(100, 100, Qt::red);
    QImage img2 = makeImage(100, 100, Qt::blue);

    SnapshotStore::saveSnapshot(path, img1); // Snap 1 (Base)
    SnapshotStore::saveSnapshot(path, img2); // Snap 2 (Delta)

    QImage restored = SnapshotStore::reconstructSnapshot(path, 2);
    QCOMPARE(restored.size(), img2.size());
    QCOMPARE(restored.pixelColor(0, 0), img2.pixelColor(0, 0));
}

void TestSnapshotStore::testResizeImage() {
    QImage img = makeImage(100, 100, Qt::green);
    QSize  targetSize(50, 50);

    QImage resized = SnapshotStore::resizeImage(img, targetSize);
    QCOMPARE(resized.size(), targetSize);
    QVERIFY(!resized.isNull());
}

// Dedup

void TestSnapshotStore::testSaveDuplicateSkipped() {
    QString path = testFilePath("dedup");
    SnapshotStore::deleteAllSnapshots(path);
    QImage img = makeImage(15, 15, Qt::darkGray);

    auto snap1 = SnapshotStore::saveSnapshot(path, img);
    QVERIFY(snap1.has_value());
    QCOMPARE(snap1->snapshotIndex, 1);

    // Saving the same image again should return the existing snapshot index
    auto snap2 = SnapshotStore::saveSnapshot(path, img);
    QVERIFY(snap2.has_value());
    QCOMPARE(snap2->snapshotIndex, 1);

    // Only one snapshot on disk
    QVector<ImageSnapshot> snapshots = SnapshotStore::loadSnapshots(path);
    QCOMPARE(snapshots.size(), 1);
}

void TestSnapshotStore::testDeleteLastSnapshot() {
    QString path = testFilePath("deleteLast");
    SnapshotStore::deleteAllSnapshots(path);

    SnapshotStore::saveSnapshot(path, makeImage(10, 10, Qt::red));
    SnapshotStore::saveSnapshot(path, makeImage(10, 10, Qt::blue));

    QVector<ImageSnapshot> snapsBefore = SnapshotStore::loadSnapshots(path);
    QCOMPARE(snapsBefore.size(), 2);

    // Delete the last one (index 2)
    QVERIFY(SnapshotStore::deleteSnapshot(path, 2));

    QVector<ImageSnapshot> snapsAfter = SnapshotStore::loadSnapshots(path);
    QCOMPARE(snapsAfter.size(), 1);
    QCOMPARE(snapsAfter[0].snapshotIndex, 1);
}

void TestSnapshotStore::testDeleteMiddleSnapshotRebase() {
    QString path = testFilePath("deleteMiddle");
    SnapshotStore::deleteAllSnapshots(path);

    QImage img1 = makeImage(10, 10, Qt::red);
    QImage img2 = makeImage(10, 10, Qt::green);
    QImage img3 = makeImage(10, 10, Qt::blue);

    SnapshotStore::saveSnapshot(path, img1);
    SnapshotStore::saveSnapshot(path, img2);
    SnapshotStore::saveSnapshot(path, img3);

    // Verify initial state: S1(Base), S2(Delta), S3(Delta)
    QVector<ImageSnapshot> snaps = SnapshotStore::loadSnapshots(path);
    QCOMPARE(snaps.size(), 3);
    QVERIFY(snaps[0].isBase);
    QVERIFY(!snaps[1].isBase);
    QVERIFY(!snaps[2].isBase);

    // Delete S2 (middle)
    QVERIFY(SnapshotStore::deleteSnapshot(path, 2));

    // Verify new state: S1(Base), S3(Delta of S1)
    snaps = SnapshotStore::loadSnapshots(path);
    QCOMPARE(snaps.size(), 2);
    QCOMPARE(snaps[0].snapshotIndex, 1);
    QCOMPARE(snaps[1].snapshotIndex, 3);
    QVERIFY(snaps[0].isBase);
    QVERIFY(!snaps[1].isBase); // S3 should be rebased as a delta of S1

    // Verify rebased structure is correct
    QCOMPARE(snaps[1].snapshotIndex, 3);
    QVERIFY(!snaps[1].isBase);
}

void TestSnapshotStore::testDeleteFirstSnapshotRebase() {
    QString path = testFilePath("deleteFirst");
    SnapshotStore::deleteAllSnapshots(path);

    QImage img1 = makeImage(10, 10, Qt::red);
    QImage img2 = makeImage(10, 10, Qt::blue);

    SnapshotStore::saveSnapshot(path, img1);
    SnapshotStore::saveSnapshot(path, img2);

    // Delete S1 (the base)
    QVERIFY(SnapshotStore::deleteSnapshot(path, 1));

    // Verify new state: S2(Base)
    QVector<ImageSnapshot> snaps = SnapshotStore::loadSnapshots(path);
    QCOMPARE(snaps.size(), 1);
    QCOMPARE(snaps[0].snapshotIndex, 2);
    QVERIFY(snaps[0].isBase);

    // Verify S2 is now the base
    QVERIFY(snaps[0].isBase);
    QCOMPARE(snaps[0].snapshotIndex, 2);
}

void TestSnapshotStore::testDeleteAllSnapshots() {
    QString path = testFilePath("delete");
    SnapshotStore::deleteAllSnapshots(path);
    SnapshotStore::saveSnapshot(path, makeImage(10, 10, Qt::red));
    SnapshotStore::saveSnapshot(path, makeImage(10, 10, Qt::blue));

    QCOMPARE(SnapshotStore::loadSnapshots(path).size(), 2);

    SnapshotStore::deleteAllSnapshots(path);
    QCOMPARE(SnapshotStore::loadSnapshots(path).size(), 0);
}

// Edge cases

void TestSnapshotStore::testLoadNonExistentFile() {
    QString path = testFilePath("nonexistent");
    SnapshotStore::deleteAllSnapshots(path);
    QVector<ImageSnapshot> snapshots = SnapshotStore::loadSnapshots(path);
    QVERIFY(snapshots.isEmpty());
}

void TestSnapshotStore::testCorruptedSnapshot() {
    QString path = testFilePath("corrupt");
    SnapshotStore::deleteAllSnapshots(path);

    QImage img = makeImage(100, 100, Qt::red);
    auto   res = SnapshotStore::saveSnapshot(path, img);
    QVERIFY(res.has_value());

    // Find the file on disk to corrupt it
    QString     key = SnapshotStore::imageKey(path);
    QDir        dir(SnapshotStore::baseDir() + "/" + key);
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
    QImage restored = SnapshotStore::reconstructSnapshot(path, res->snapshotIndex);
    // We just verify it doesn't crash the process.
    Q_UNUSED(restored);
}

void TestSnapshotStore::testExtremeResolution() {
    // Test with a large image (e.g. 16k x 16k).
    // We use a small image if memory is an issue, but 16k is usually okay for GPU.
    QImage largeImg(16384, 16384, QImage::Format_ARGB32);
    largeImg.fill(Qt::blue);

    QSize  targetSize(256, 256);
    QImage resized = SnapshotStore::resizeImage(largeImg, targetSize);

    QCOMPARE(resized.size(), targetSize);
    QVERIFY(!resized.isNull());
}

void TestSnapshotStore::testConcurrentSaves() {
    QString path = testFilePath("concurrent");
    SnapshotStore::deleteAllSnapshots(path);

    const int                                                  numThreads = 8;
    QVector<QFuture<std::optional<SnapshotStore::SaveResult>>> futures;

    for (int i = 0; i < numThreads; ++i) {
        futures.append(QtConcurrent::run([path, i]() {
            // Each thread saves a different color to avoid deduplication
            QImage img(10, 10, QImage::Format_ARGB32);
            img.fill(QColor(i * 20, 0, 0));
            return SnapshotStore::saveSnapshot(path, img);
        }));
    }

    for (auto& f : futures) {
        auto res = f.result();
        QVERIFY(res.has_value());
    }

    QVector<ImageSnapshot> snaps = SnapshotStore::loadSnapshots(path);
    QCOMPARE(snaps.size(), numThreads);
}

QTEST_MAIN(TestSnapshotStore)
#include "test_snapshotstore.moc"
