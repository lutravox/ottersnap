#include <QtTest>
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
