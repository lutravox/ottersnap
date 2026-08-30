#include <QtTest>
#include <QDataStream>
#include <QByteArray>
#include "core/snapshotdecompressor.h"
#include "core/snapshot_types.h"

class TestSnapshotDecompressor : public QObject {
    Q_OBJECT

private:
    /// @brief Helper to create a binary delta blob.
    QByteArray createDeltaBlob(uint32_t version, uint32_t tileW, uint32_t tileH, 
                               const QVector<std::pair<uint32_t, QByteArray>>& tiles) {
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        
        stream << version << tileW << tileH << (uint32_t)tiles.size();
        
        for (const auto& tile : tiles) {
            stream << tile.first << qCompress(tile.second);
        }
        
        return data;
    }

private slots:
    void testSuccessfulDecompression() {
        uint32_t tileW = 16;
        uint32_t tileH = 16;
        QByteArray tilePixels(tileW * tileH * 4, 0); // Black tile
        QVector<std::pair<uint32_t, QByteArray>> tiles = {{0, tilePixels}};
        
        DeltaEntry delta = {"test", createDeltaBlob(1, tileW, tileH, tiles)};
        DecompressedDelta out;
        
        QVERIFY(SnapshotDecompressor::decompress(delta, out));
        QCOMPARE(out.tileW, tileW);
        QCOMPARE(out.tileH, tileH);
        QCOMPARE(out.tileIndices.size(), 1);
        QCOMPARE(out.tileIndices[0], 0);
        QCOMPARE(out.packedPixels.size(), (int)(tileW * tileH * 4));
    }

    void testInvalidVersion() {
        QByteArray data = createDeltaBlob(2, 16, 16, {}); // Version 2 instead of 1
        DeltaEntry delta = {"test", data};
        DecompressedDelta out;
        
        QVERIFY(!SnapshotDecompressor::decompress(delta, out));
    }

    void testEmptyDelta() {
        QByteArray data = createDeltaBlob(1, 16, 16, {});
        DeltaEntry delta = {"test", data};
        DecompressedDelta out;
        
        QVERIFY(SnapshotDecompressor::decompress(delta, out));
        QVERIFY(out.tileIndices.isEmpty());
    }

    void testInvalidTileDimensions() {
        // Tile size too large
        QByteArray data = createDeltaBlob(1, 5000, 16, {});
        DeltaEntry delta = {"test", data};
        DecompressedDelta out;
        QVERIFY(!SnapshotDecompressor::decompress(delta, out));

        // Tile size zero
        data = createDeltaBlob(1, 0, 16, {});
        delta = {"test", data};
        QVERIFY(!SnapshotDecompressor::decompress(delta, out));
    }

    void testCorruptedTileData() {
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << (uint32_t)1 << (uint32_t)16 << (uint32_t)16 << (uint32_t)1;
        stream << (uint32_t)0 << QByteArray("not compressed data");

        DeltaEntry delta = {"test", data};
        DecompressedDelta out;
        
        QVERIFY(!SnapshotDecompressor::decompress(delta, out));
    }
};

QTEST_MAIN(TestSnapshotDecompressor)
#include "test_snapshotdecompressor.moc"
