#include <QtTest>
#include "core/deltacache.h"

class TestDeltaCache : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void testBasicInsertAndGet();
    void testCacheMiss();
    void testClear();
    void testUpdateMaxCost();
    void testLRUEviction();
};

void TestDeltaCache::initTestCase() {
    DeltaCache::clear();
}

void TestDeltaCache::cleanupTestCase() {
    DeltaCache::clear();
}

void TestDeltaCache::testBasicInsertAndGet() {
    QString           id = "test_delta_1";
    DecompressedDelta data;
    data.tileW = 256;
    data.tileH = 256;
    data.packedPixels = QByteArray(1024, 0xAA);
    data.tileIndices = {1, 2, 3};
    data.tileOffsets = {0, 100, 200};

    DeltaCache::insert(id, data);
    auto result = DeltaCache::get(id);

    QVERIFY(result.has_value());
    QCOMPARE(result->tileW, 256);
    QCOMPARE(result->tileH, 256);
    QCOMPARE(result->packedPixels.size(), 1024);
    QCOMPARE(result->tileIndices, data.tileIndices);
    QCOMPARE(result->tileOffsets, data.tileOffsets);
}

void TestDeltaCache::testCacheMiss() {
    DeltaCache::clear();
    auto result = DeltaCache::get("non_existent");
    QVERIFY(!result.has_value());
}

void TestDeltaCache::testClear() {
    QString           id = "test_clear";
    DecompressedDelta data;
    data.packedPixels = QByteArray(100, 0);
    DeltaCache::insert(id, data);
    QVERIFY(DeltaCache::get(id).has_value());

    DeltaCache::clear();
    QVERIFY(!DeltaCache::get(id).has_value());
}

void TestDeltaCache::testUpdateMaxCost() {
    // This is a simple check to ensure it doesn't crash and can be called.
    DeltaCache::updateMaxCost(128);
}

void TestDeltaCache::testLRUEviction() {
    // Set a very small cache size to force eviction
    // 1 MB = 1024 * 1024 bytes
    DeltaCache::updateMaxCost(1);

    // Insert two items that together exceed 1MB
    // Each item cost is based on packedPixels.size()
    QString           id1 = "delta_1";
    DecompressedDelta data1;
    data1.packedPixels = QByteArray(600 * 1024, 'A'); // 600 KB
    DeltaCache::insert(id1, data1);

    QString           id2 = "delta_2";
    DecompressedDelta data2;
    data2.packedPixels = QByteArray(600 * 1024, 'B'); // 600 KB
    DeltaCache::insert(id2, data2);

    // Total cost is now 1.2MB, which exceeds 1MB.
    // delta_1 should be evicted as it was the oldest.
    QVERIFY(!DeltaCache::get(id1).has_value());
    QVERIFY(DeltaCache::get(id2).has_value());
}

QTEST_MAIN(TestDeltaCache)
#include "test_deltacache.moc"
