#include <QtTest>
#include "core/versionstore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>

class TestVersionStore : public QObject {
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
    void testSaveFirstVersion();
    void testLoadVersions();
    void testLoadVersionImage();
    void testMultipleVersions();

    // Dedup
    void testSaveDuplicateSkipped();

    // Delete
    void testDeleteAllVersions();

    // Edge cases
    void testLoadNonExistentFile();
    void testLoadBadVersionIndex();

  private:
    // Each test gets a unique file path so version keys don't collide.
    static QString testFilePath(const QString& suffix);
    static QImage  makeImage(int width, int height, QColor color);
};

void TestVersionStore::initTestCase() {
    // Nothing special needed — each test uses a unique file path,
    // so version directories don't collide.
}

void TestVersionStore::cleanupTestCase() {
    // Clean up any leftover test data — not strictly necessary
    // since each test uses unique paths, but good practice.
}

QString TestVersionStore::testFilePath(const QString& suffix) {
    // Use a path that looks like a real file path but doesn't need to
    // actually exist — it's only used to generate a version key.
    return QString("/tmp/ottersnap_test_%1/file.png").arg(suffix);
}

QImage TestVersionStore::makeImage(int width, int height, QColor color) {
    QImage img(width, height, QImage::Format_ARGB32);
    img.fill(color);
    return img;
}

// Checksum

void TestVersionStore::testChecksumNullImage() {
    QImage  img;
    QString checksum = VersionStore::computeChecksum(img);
    QVERIFY(checksum.isEmpty());
}

void TestVersionStore::testChecksumDeterministic() {
    QImage  img = makeImage(10, 10, Qt::red);
    QString cs1 = VersionStore::computeChecksum(img);
    QString cs2 = VersionStore::computeChecksum(img);
    QCOMPARE(cs1, cs2);
    QVERIFY(!cs1.isEmpty());
}

void TestVersionStore::testChecksumDifferent() {
    QImage img1 = makeImage(10, 10, Qt::red);
    QImage img2 = makeImage(10, 10, Qt::blue);
    QVERIFY(VersionStore::computeChecksum(img1) != VersionStore::computeChecksum(img2));
}

void TestVersionStore::testChecksumSameImage() {
    QImage img1 = makeImage(10, 10, Qt::green);
    QImage img2 = makeImage(10, 10, Qt::green);
    QCOMPARE(VersionStore::computeChecksum(img1), VersionStore::computeChecksum(img2));
}

// Save / Load cycle

void TestVersionStore::testSaveFirstVersion() {
    QString path = testFilePath("save1");
    VersionStore::deleteAllVersions(path);
    QImage img = makeImage(20, 20, Qt::magenta);

    auto ver = VersionStore::saveVersion(path, img);
    QVERIFY(ver.has_value());
    QCOMPARE(*ver, 1);
}

void TestVersionStore::testLoadVersions() {
    QString path = testFilePath("load1");
    VersionStore::deleteAllVersions(path);
    QImage img = makeImage(20, 20, Qt::cyan);
    VersionStore::saveVersion(path, img);

    QVector<ImageVersion> versions = VersionStore::loadVersions(path);
    QCOMPARE(versions.size(), 1);
    QCOMPARE(versions[0].version, 1);
    QVERIFY(!versions[0].fileName.isEmpty());
    QVERIFY(!versions[0].checksum.isEmpty());
    QVERIFY(!versions[0].timestamp.isNull());
}

void TestVersionStore::testLoadVersionImage() {
    QString path = testFilePath("loadImg");
    VersionStore::deleteAllVersions(path);
    QImage img = makeImage(30, 30, Qt::yellow);
    auto   ver = VersionStore::saveVersion(path, img);
    QVERIFY(ver.has_value());

    auto optLoaded = VersionStore::loadVersionImage(path, *ver);
    QVERIFY(optLoaded.has_value());
    QCOMPARE(optLoaded->width(), 30);
    QCOMPARE(optLoaded->height(), 30);
}

void TestVersionStore::testMultipleVersions() {
    QString path = testFilePath("multi");
    VersionStore::deleteAllVersions(path);

    VersionStore::saveVersion(path, makeImage(10, 10, Qt::red));
    VersionStore::saveVersion(path, makeImage(10, 10, Qt::green));
    VersionStore::saveVersion(path, makeImage(10, 10, Qt::blue));

    QVector<ImageVersion> versions = VersionStore::loadVersions(path);
    QCOMPARE(versions.size(), 3);
    QCOMPARE(versions[0].version, 1);
    QCOMPARE(versions[1].version, 2);
    QCOMPARE(versions[2].version, 3);
}

// Dedup

void TestVersionStore::testSaveDuplicateSkipped() {
    QString path = testFilePath("dedup");
    VersionStore::deleteAllVersions(path);
    QImage img = makeImage(15, 15, Qt::darkGray);

    auto ver1 = VersionStore::saveVersion(path, img);
    QVERIFY(ver1.has_value());
    QCOMPARE(*ver1, 1);

    // Saving the same image again should return the existing version number
    auto ver2 = VersionStore::saveVersion(path, img);
    QVERIFY(ver2.has_value());
    QCOMPARE(*ver2, 1);

    // Only one version on disk
    QVector<ImageVersion> versions = VersionStore::loadVersions(path);
    QCOMPARE(versions.size(), 1);
}

// Delete

void TestVersionStore::testDeleteAllVersions() {
    QString path = testFilePath("delete");
    VersionStore::deleteAllVersions(path);
    VersionStore::saveVersion(path, makeImage(10, 10, Qt::red));
    VersionStore::saveVersion(path, makeImage(10, 10, Qt::blue));

    QCOMPARE(VersionStore::loadVersions(path).size(), 2);

    VersionStore::deleteAllVersions(path);
    QCOMPARE(VersionStore::loadVersions(path).size(), 0);
}

// Edge cases

void TestVersionStore::testLoadNonExistentFile() {
    QString path = testFilePath("nonexistent");
    VersionStore::deleteAllVersions(path);
    QVector<ImageVersion> versions = VersionStore::loadVersions(path);
    QVERIFY(versions.isEmpty());
}

void TestVersionStore::testLoadBadVersionIndex() {
    QString path = testFilePath("badVer");
    VersionStore::deleteAllVersions(path);
    VersionStore::saveVersion(path, makeImage(10, 10, Qt::red));

    // Request a version that doesn't exist
    auto optLoaded = VersionStore::loadVersionImage(path, 999);
    QVERIFY(!optLoaded.has_value());
}

QTEST_MAIN(TestVersionStore)
#include "test_versionstore.moc"
