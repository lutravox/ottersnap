#include <QImage>
#include <QTemporaryDir>
#include <QtTest>
#include "core/imageloader.h"

class TestImageLoader : public QObject {
    Q_OBJECT

  private slots:
    // loadImage
    void testLoadRealFile();
    void testLoadNonExistentFile();
    void testLoadRealPng();
};

// loadImage

void TestImageLoader::testLoadRealFile() {
    // Create a temp PNG file and load it through loadImage.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QImage img(10, 10, QImage::Format_ARGB32);
    img.fill(Qt::red);
    QString path = dir.path() + "/test.png";
    QVERIFY(img.save(path, "PNG"));

    QImage loaded = loadImage(path);
    QVERIFY(!loaded.isNull());
    QCOMPARE(loaded.width(), 10);
    QCOMPARE(loaded.height(), 10);
}

void TestImageLoader::testLoadNonExistentFile() {
    QImage loaded = loadImage("/tmp/ottersnap_nonexistent_file.png");
    QVERIFY(loaded.isNull());
}

void TestImageLoader::testLoadRealPng() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Create a multi-color image to verify data integrity
    QImage img(3, 3, QImage::Format_ARGB32);
    img.setPixelColor(0, 0, QColor(255, 0, 0, 255));
    img.setPixelColor(1, 1, QColor(0, 255, 0, 255));
    img.setPixelColor(2, 2, QColor(0, 0, 255, 255));
    QString path = dir.path() + "/multi.png";
    QVERIFY(img.save(path, "PNG"));

    QImage loaded = loadImage(path);
    QVERIFY(!loaded.isNull());
    QCOMPARE(loaded.width(), 3);
    QCOMPARE(loaded.height(), 3);
    QCOMPARE(loaded.pixelColor(0, 0), QColor(255, 0, 0, 255));
    QCOMPARE(loaded.pixelColor(1, 1), QColor(0, 255, 0, 255));
    QCOMPARE(loaded.pixelColor(2, 2), QColor(0, 0, 255, 255));
}

QTEST_MAIN(TestImageLoader)
#include "test_imageloader.moc"
