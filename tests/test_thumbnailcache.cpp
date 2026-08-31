#include <QCoreApplication>
#include <QImage>
#include <QString>
#include <QtTest>
#include "core/snapshotmanager.h"
#include "core/thumbnailcache.h"
#include "core/thumbnailmanager.h"

class TestThumbnailCache : public QObject {
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
    void testLoadThumbnailCacheMiss();
    void testLoadThumbnailCacheHit();
};

void TestThumbnailCache::initTestCase() {
    ThumbnailCache::clear();
}

void TestThumbnailCache::cleanupTestCase() {
    ThumbnailCache::clear();
}

void TestThumbnailCache::testInsertAndGet() {
    QString key = "test_image_1";
    QImage *img = new QImage(100, 100, QImage::Format_RGB32);
    img->fill(Qt::red);

    ThumbnailCache::insert(key, img, 1);
    QImage *retrieved = ThumbnailCache::get(key);

    QVERIFY(retrieved != nullptr);
    QCOMPARE(retrieved->size(), QSize(100, 100));
    QCOMPARE(retrieved->pixelColor(0, 0), QColor(Qt::red));
}

void TestThumbnailCache::testRemove() {
    QString key = "test_remove";
    ThumbnailCache::insert(key, new QImage(10, 10, QImage::Format_RGB32), 1);
    QVERIFY(ThumbnailCache::get(key) != nullptr);

    ThumbnailCache::remove(key);
    QVERIFY(ThumbnailCache::get(key) == nullptr);
}

void TestThumbnailCache::testClear() {
    ThumbnailCache::insert("a", new QImage(10, 10, QImage::Format_RGB32), 1);
    ThumbnailCache::insert("b", new QImage(10, 10, QImage::Format_RGB32), 1);

    ThumbnailCache::clear();

    QVERIFY(ThumbnailCache::get("a") == nullptr);
    QVERIFY(ThumbnailCache::get("b") == nullptr);
}

void TestThumbnailCache::testLRUEviction() {
    int limitMB = 10;
    ThumbnailCache::updateMaxCost(limitMB);
    int costPerImage = 4 * 1024 * 1024;

    ThumbnailCache::insert("img1", new QImage(10, 10, QImage::Format_RGB32), costPerImage);
    ThumbnailCache::insert("img2", new QImage(10, 10, QImage::Format_RGB32), costPerImage);
    QVERIFY(ThumbnailCache::get("img1") != nullptr);
    QVERIFY(ThumbnailCache::get("img2") != nullptr);

    ThumbnailCache::insert("img3", new QImage(10, 10, QImage::Format_RGB32), costPerImage);

    QVERIFY(ThumbnailCache::get("img1") == nullptr);
    QVERIFY(ThumbnailCache::get("img2") != nullptr);
    QVERIFY(ThumbnailCache::get("img3") != nullptr);
}

void TestThumbnailCache::testUpdateMaxCost() {
    ThumbnailCache::updateMaxCost(1);
    ThumbnailCache::insert("img1", new QImage(10, 10, QImage::Format_RGB32), 1);
    QVERIFY(ThumbnailCache::get("img1") != nullptr);

    ThumbnailCache::updateMaxCost(0);
    ThumbnailCache::insert("img2", new QImage(10, 10, QImage::Format_RGB32), 1);
}

void TestThumbnailCache::testLoadThumbnailCacheMiss() {
    QString key = "miss_key_" + QString::number(QRandomGenerator::global()->generate());
    QString version = "v1";
    QSize   size(32, 32);

    QImage result = ThumbnailCache::loadThumbnail(key, version, size);

    QVERIFY(result.isNull());
}

void TestThumbnailCache::testLoadThumbnailCacheHit() {
    QString key = "hit_key";
    QString version = "v1";
    QSize   size(32, 32);

    QString baseThumbDir = SnapshotManager::thumbnailDir();
    QString cacheDir = baseThumbDir + '/' + key;
    QDir().mkpath(cacheDir);
    QImage thumb(64, 64, QImage::Format_RGB32);
    thumb.save(cacheDir + "/" + version + ".webp", "WEBP");

    QImage result = ThumbnailCache::loadThumbnail(key, version, size);

    QVERIFY(!result.isNull());
}

QTEST_MAIN(TestThumbnailCache)
#include "test_thumbnailcache.moc"
