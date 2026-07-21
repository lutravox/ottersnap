#include <QtTest>
#include "config/appsettings.h"
#include "core/snapshotstore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>

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
    void testLoadSnapshotImage();
    void testMultipleSnapshots();
    void testDeltaReconstruction();
    void testDeltaReconstructionFromSubsequentBase();
    void testSizeChangeTriggersBase();
    void testBaseIntervalLogic();

    // Dedup
    void testSaveDuplicateSkipped();

    // Delete
    void testDeleteAllSnapshots();

    // Edge cases
    void testLoadNonExistentFile();
    void testLoadBadSnapshotIndex();

  private:
    // Each test gets a unique file path so snapshot keys don't collide.
    static QString testFilePath(const QString& suffix);
    static QImage  makeImage(int width, int height, QColor color);
};

void TestSnapshotStore::initTestCase() {
    // Nothing special needed — each test uses a unique file path,
    // so snapshot directories don't collide.
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

void TestSnapshotStore::testLoadSnapshotImage() {
    QString path = testFilePath("loadImg");
    SnapshotStore::deleteAllSnapshots(path);
    QImage img = makeImage(30, 30, Qt::yellow);
    auto   snap = SnapshotStore::saveSnapshot(path, img);
    QVERIFY(snap.has_value());

    auto optLoaded = SnapshotStore::loadSnapshotImage(path, snap->snapshotIndex);
    QVERIFY(optLoaded.has_value());
    QCOMPARE(optLoaded->width(), 30);
    QCOMPARE(optLoaded->height(), 30);
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

void TestSnapshotStore::testDeltaReconstruction() {
    QString path = testFilePath("deltaRec");
    SnapshotStore::deleteAllSnapshots(path);

    // 1. Save first image -> This will be the base (snapshotIndex 1)
    QImage img1 = makeImage(100, 100, Qt::red);
    auto   snap1 = SnapshotStore::saveSnapshot(path, img1);
    QVERIFY(snap1.has_value());
    QCOMPARE(snap1->snapshotIndex, 1);

    // 2. Save second image of same size/format -> This will be a delta (snapshotIndex 2)
    QImage img2 = makeImage(100, 100, Qt::blue);
    auto   snap2 = SnapshotStore::saveSnapshot(path, img2);
    QVERIFY(snap2.has_value());
    QCOMPARE(snap2->snapshotIndex, 2);

    // Verify base/delta status via metadata
    QVector<ImageSnapshot> snaps = SnapshotStore::loadSnapshots(path);
    QVERIFY(snaps.size() >= 2);
    QVERIFY(snaps[0].isBase);
    QVERIFY(!snaps[1].isBase);

    // 3. Load the second snapshot and verify reconstruction
    auto optLoaded = SnapshotStore::loadSnapshotImage(path, 2);
    QVERIFY(optLoaded.has_value());

    // Compare pixels of reconstructed image with original img2
    QImage reconstructed = *optLoaded;
    QCOMPARE(reconstructed.size(), img2.size());
    QCOMPARE(reconstructed.pixelColor(0, 0), img2.pixelColor(0, 0));

    // Verify it is indeed the blue image, not the red base
    QVERIFY(reconstructed.pixelColor(0, 0) != img1.pixelColor(0, 0));
}

void TestSnapshotStore::testDeltaReconstructionFromSubsequentBase() {
    QString path = testFilePath("deltaRecSubsequent");
    SnapshotStore::deleteAllSnapshots(path);

    // Set a small interval so we get a new base quickly
    int testInterval = 3;
    AppSettings::setBaseInterval(testInterval);

    // Save a sequence of images.
    // With interval 3:
    // Snap 1: Base
    // Snap 2: Delta
    // Snap 3: Delta
    // Snap 4: Base  <-- We want this to be our base
    // Snap 5: Delta  <-- We want to reconstruct this
    QVector<QImage> images;
    for (int i = 0; i < 6; ++i) {
        images.append(makeImage(100, 100, QColor(i * 40, 0, 0)));
        SnapshotStore::saveSnapshot(path, images.last());
    }

    // Verify the structure
    QVector<ImageSnapshot> snaps = SnapshotStore::loadSnapshots(path);
    QVERIFY(snaps.size() >= 5);
    QVERIFY(snaps[0].isBase);  // Snap 1
    QVERIFY(!snaps[1].isBase); // Snap 2
    QVERIFY(!snaps[2].isBase); // Snap 3
    QVERIFY(snaps[3].isBase);  // Snap 4
    QVERIFY(!snaps[4].isBase); // Snap 5

    // Reconstruct Snap 5. It should use Snap 4 as its base.
    auto optLoaded = SnapshotStore::loadSnapshotImage(path, 5);
    QVERIFY(optLoaded.has_value());

    QImage reconstructed = *optLoaded;
    QCOMPARE(reconstructed.size(), images[4].size());
    QCOMPARE(reconstructed.pixelColor(0, 0), images[4].pixelColor(0, 0));

    // Ensure it's not using the first base (Snap 1)
    QVERIFY(reconstructed.pixelColor(0, 0) != images[0].pixelColor(0, 0));

    // Reset interval
    AppSettings::setBaseInterval(100);
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

    // Verify we can actually load it
    auto optLoaded = SnapshotStore::loadSnapshotImage(path, 3);
    QVERIFY(optLoaded.has_value());
    QCOMPARE(optLoaded->width(), 200);
    QCOMPARE(optLoaded->height(), 200);
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

// Delete

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

void TestSnapshotStore::testLoadBadSnapshotIndex() {
    QString path = testFilePath("badSnap");
    SnapshotStore::deleteAllSnapshots(path);
    SnapshotStore::saveSnapshot(path, makeImage(10, 10, Qt::red));

    // Request a snapshot that doesn't exist
    auto optLoaded = SnapshotStore::loadSnapshotImage(path, 999);
    QVERIFY(!optLoaded.has_value());
}

QTEST_MAIN(TestSnapshotStore)
#include "test_snapshotstore.moc"
