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
};

// loadImage

void TestDiskUtils::testLoadRealFile() {
    // Create a temp PNG file and load it through loadImage.
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

    // Create a multi-color image to verify data integrity
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

QTEST_MAIN(TestDiskUtils)
#include "test_diskutils.moc"
