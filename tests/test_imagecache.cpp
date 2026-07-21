#include <QCoreApplication>
#include <QImage>
#include <QString>
#include <QtTest>
#include <functional>
#include "core/imagecache.h"

class TestImageCache : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();

    // Basic Cache Ops
    void testInsertAndGet();
    void testRemove();
    void testClear();

    // LRU and Capacity
    void testLRUEviction();
    void testUpdateMaxCost();

    // Thumbnail Logic
    void testFormatThumbnail();
    void testLoadThumbnailCacheMiss();
    void testLoadThumbnailCacheHit();
};

void TestImageCache::initTestCase() {
    ImageCache::clear();
}

void TestImageCache::cleanupTestCase() {
    ImageCache::clear();
}

void TestImageCache::testInsertAndGet() {
    QString key = "test_image_1";
    QImage *img = new QImage(100, 100, QImage::Format_RGB32);
    img->fill(Qt::red);

    ImageCache::insert(key, img, 1);
    QImage *retrieved = ImageCache::get(key);

    QVERIFY(retrieved != nullptr);
    QCOMPARE(retrieved->size(), QSize(100, 100));
    QCOMPARE(retrieved->pixelColor(0, 0), QColor(Qt::red));
}

void TestImageCache::testRemove() {
    QString key = "test_remove";
    ImageCache::insert(key, new QImage(10, 10, QImage::Format_RGB32), 1);
    QVERIFY(ImageCache::get(key) != nullptr);

    ImageCache::remove(key);
    QVERIFY(ImageCache::get(key) == nullptr);
}

void TestImageCache::testClear() {
    ImageCache::insert("a", new QImage(10, 10, QImage::Format_RGB32), 1);
    ImageCache::insert("b", new QImage(10, 10, QImage::Format_RGB32), 1);

    ImageCache::clear();

    QVERIFY(ImageCache::get("a") == nullptr);
    QVERIFY(ImageCache::get("b") == nullptr);
}

void TestImageCache::testLRUEviction() {
    // Set limit to 10MB
    int limitMB = 10;
    ImageCache::updateMaxCost(limitMB);
    int costPerImage = 4 * 1024 * 1024; // 4 MB

    // Insert img1 and img2: Total = 8MB (Fits in 10MB)
    ImageCache::insert("img1", new QImage(10, 10, QImage::Format_RGB32), costPerImage);
    ImageCache::insert("img2", new QImage(10, 10, QImage::Format_RGB32), costPerImage);
    QVERIFY(ImageCache::get("img1") != nullptr);
    QVERIFY(ImageCache::get("img2") != nullptr);

    // Insert img3: Total = 12MB (Exceeds 10MB)
    ImageCache::insert("img3", new QImage(10, 10, QImage::Format_RGB32), costPerImage);

    // To fit 12MB into 10MB, QCache must evict the oldest item (img1).
    // Total cost becomes 8MB (img2 + img3), which fits in 10MB.
    QVERIFY(ImageCache::get("img1") == nullptr);
    QVERIFY(ImageCache::get("img2") != nullptr);
    QVERIFY(ImageCache::get("img3") != nullptr);
}

void TestImageCache::testUpdateMaxCost() {
    ImageCache::updateMaxCost(1);
    ImageCache::insert("img1", new QImage(10, 10, QImage::Format_RGB32), 1);
    QVERIFY(ImageCache::get("img1") != nullptr);

    ImageCache::updateMaxCost(0); // Should effectively clear or limit severely
    // Depending on QCache implementation, reducing cost might trigger eviction
    // We just verify it doesn't crash and behaves predictably.
    ImageCache::insert("img2", new QImage(10, 10, QImage::Format_RGB32), 1);
}

void TestImageCache::testFormatThumbnail() {
    QImage img(100, 50, QImage::Format_RGB32);
    img.fill(Qt::blue);

    int    thumbSize = 32;
    QImage thumb = ImageCache::formatThumbnail(img, thumbSize);

    QCOMPARE(thumb.width(), thumbSize);
    QCOMPARE(thumb.height(), thumbSize);
    // Check that it's not just empty (blue should be present)
    QVERIFY(thumb.pixelColor(thumbSize / 2, thumbSize / 2) == QColor(Qt::blue));
}

void TestImageCache::testLoadThumbnailCacheMiss() {
    // Use a unique key to avoid disk-cache hits from previous runs
    QString key = "miss_key_" + QString::number(QRandomGenerator::global()->generate());
    int     version = 1;
    QSize   size(32, 32);
    bool    callbackCalled = false;

    QImage result = ImageCache::loadThumbnail(key, version, size, [&]() {
        callbackCalled = true;
        return QImage(64, 64, QImage::Format_RGB32);
    });

    QVERIFY(callbackCalled);
    QCOMPARE(result.size(), size);
}

void TestImageCache::testLoadThumbnailCacheHit() {
    QString key = "hit_key";
    int     version = 1;
    QSize   size(32, 32);

    // First call to populate cache
    ImageCache::loadThumbnail(
        key, version, size, []() { return QImage(64, 64, QImage::Format_RGB32); });

    bool   callbackCalled = false;
    QImage result = ImageCache::loadThumbnail(key, version, size, [&]() {
        callbackCalled = true;
        return QImage(64, 64, QImage::Format_RGB32);
    });

    // Callback should NOT be called on hit
    QVERIFY(!callbackCalled);
    QCOMPARE(result.size(), size);
}

QTEST_MAIN(TestImageCache)
#include "test_imagecache.moc"
