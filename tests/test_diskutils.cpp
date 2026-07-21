#include <QImage>
#include <QTemporaryDir>
#include <QtTest>
#include "core/diskutils.h"

class TestDiskUtils : public QObject {
    Q_OBJECT

  private slots:
    // loadImage
    void testLoadRealFile();
    void testLoadNonExistentFile();
    void testLoadRealPng();

    // Directory management
    void testEnsureDir();
    void testGetAndEnsureDir();

    // Atomic Write
    void testAtomicWriteSuccess();
    void testAtomicWriteFailure();
};

// loadImage

void TestDiskUtils::testLoadRealFile() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QImage img(10, 10, QImage::Format_ARGB32);
    img.fill(Qt::red);
    QString path = dir.path() + "/test.png";
    QVERIFY(img.save(path, "PNG"));

    QImage loaded = DiskUtils::loadImage(path);
    QCOMPARE(loaded.height(), 10);
}

void TestDiskUtils::testLoadNonExistentFile() {
    QImage loaded = DiskUtils::loadImage("/tmp/ottersnap_nonexistent_file.png");
    QVERIFY(loaded.isNull());
}

void TestDiskUtils::testLoadRealPng() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QImage img(3, 3, QImage::Format_ARGB32);
    img.setPixelColor(0, 0, QColor(255, 0, 0, 255));
    img.setPixelColor(1, 1, QColor(0, 255, 0, 255));
    img.setPixelColor(2, 2, QColor(0, 0, 255, 255));
    QString path = dir.path() + "/multi.png";
    QVERIFY(img.save(path, "PNG"));

    QImage loaded = DiskUtils::loadImage(path);
    QCOMPARE(loaded.height(), 3);
    QCOMPARE(loaded.pixelColor(0, 0), QColor(255, 0, 0, 255));
    QCOMPARE(loaded.pixelColor(1, 1), QColor(0, 255, 0, 255));
    QCOMPARE(loaded.pixelColor(2, 2), QColor(0, 0, 255, 255));
}

// Directory management

void TestDiskUtils::testEnsureDir() {
    QTemporaryDir root;
    QString       path = root.path() + "/nested/dir/test";

    QVERIFY(DiskUtils::ensureDir(path));
    QVERIFY(QDir(path).exists());
}

void TestDiskUtils::testGetAndEnsureDir() {
    QTemporaryDir root;
    QString       key = "my_cache_key";

    QString path = DiskUtils::getAndEnsureDir(root.path(), key);
    QVERIFY(path.endsWith('/' + key));
    QVERIFY(QDir(path).exists());
}

// Atomic Write

void TestDiskUtils::testAtomicWriteSuccess() {
    QTemporaryDir dir;
    QString       target = dir.path() + "/final.txt";

    auto writeOp = [](const QString& tmpPath) {
        QFile file(tmpPath);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        file.write("hello world");
        file.close();
        return true;
    };

    QVERIFY(DiskUtils::atomicWrite(target, writeOp));
    QVERIFY(QFile::exists(target));

    QFile result(target);
    QVERIFY(result.open(QIODevice::ReadOnly));
    QCOMPARE(result.readAll(), QByteArray("hello world"));
}

void TestDiskUtils::testAtomicWriteFailure() {
    QTemporaryDir dir;
    QString       target = dir.path() + "/final.txt";

    auto writeOp = [](const QString& /*tmpPath*/) {
        return false; // Simulate write failure
    };

    QVERIFY(!DiskUtils::atomicWrite(target, writeOp));
    QVERIFY(!QFile::exists(target));
}

QTEST_MAIN(TestDiskUtils)
#include "test_diskutils.moc"
