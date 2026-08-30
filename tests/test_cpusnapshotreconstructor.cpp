#include <QtTest>
#include <QImage>
#include <QDataStream>
#include <QVector>
#include "core/cpusnapshotreconstructor.h"
#include "core/snapshot_types.h"

class TestCPUSnapshotReconstructor : public QObject {
    Q_OBJECT

private:
    /// @brief Helper to create a valid DeltaEntry that SnapshotDecompressor can handle.
    DeltaEntry createDelta(uint32_t tileW, uint32_t tileH, 
                           const QVector<uint32_t>& indices, 
                           const QVector<QByteArray>& tilePixels) {
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        
        uint32_t version = 1;
        stream << version << tileW << tileH << (uint32_t)indices.size();
        
        for (size_t i = 0; i < indices.size(); ++i) {
            stream << indices[i] << qCompress(tilePixels[i]);
        }
        
        return {"test-delta", data};
    }

private slots:
    void testBasicReconstruction() {
        CPUSnapshotReconstructor reconstructor;
        
        // 1. Create a 100x100 red base image
        QImage base(100, 100, QImage::Format_ARGB32);
        base.fill(Qt::red);
        
        // 2. Create a delta that XORs the top-left tile with red to make it blue
        // Red (FF0000FF) XOR Blue (0000FFFF) = Purple (FF00FFFF)
        // To get Blue from Red, we need: Red ^ Blue = Delta
        uint32_t red = qRgba(255, 0, 0, 255);
        uint32_t blue = qRgba(0, 0, 255, 255);
        uint32_t deltaVal = red ^ blue;
        
        uint32_t tileW = 10;
        uint32_t tileH = 10;
        
        QImage tile(tileW, tileH, QImage::Format_ARGB32);
        for(int y=0; y<tileH; ++y) {
            uint32_t* row = reinterpret_cast<uint32_t*>(tile.scanLine(y));
            for(int x=0; x<tileW; ++x) row[x] = deltaVal;
        }
        
        QByteArray tileData(reinterpret_cast<const char*>(tile.bits()), tileW * tileH * 4);
        QVector<uint32_t> indices = {0};
        QVector<QByteArray> tiles = {tileData};
        
        DeltaEntry delta = createDelta(tileW, tileH, indices, tiles);
        
        ReconstructionSequence seq;
        seq.base = base;
        seq.baseChecksum = "base-hash";
        seq.deltas = {delta};
        
        bool success = reconstructor.reconstruct(seq);
        QVERIFY2(success, "Reconstruction failed");
        
        // Verify the top-left tile is now blue
        QVERIFY(reconstructor.samplePixel(0, 0) == blue); // Blue
        QVERIFY(reconstructor.samplePixel(5, 5) == blue); // Blue
        
        // Verify the rest of the image is still red
        QVERIFY(reconstructor.samplePixel(11, 11) == red); // Red
    }

    void testMultiTileReconstruction() {
        CPUSnapshotReconstructor reconstructor;

        // 100x100 red base image, 10x10 tiles -> 10x10 grid
        QImage base(100, 100, QImage::Format_ARGB32);
        base.fill(Qt::red);

        uint32_t red = qRgba(255, 0, 0, 255);
        uint32_t blue = qRgba(0, 0, 255, 255);
        uint32_t green = qRgba(0, 255, 0, 255);
        uint32_t yellow = qRgba(255, 255, 0, 255);

        uint32_t tileW = 10;
        uint32_t tileH = 10;

        auto makeTile = [](uint32_t value, uint32_t w, uint32_t h) {
            QImage tile(w, h, QImage::Format_ARGB32);
            for (int y = 0; y < h; ++y) {
                uint32_t* row = reinterpret_cast<uint32_t*>(tile.scanLine(y));
                for (int x = 0; x < w; ++x) row[x] = value;
            }
            return QByteArray(reinterpret_cast<const char*>(tile.constBits()), w * h * 4);
        };

        // Modify three non-adjacent tiles:
        // Tile 0 -> (0,0), Tile 1 -> (10,0), Tile 12 -> (20,10)
        QVector<uint32_t> indices = {0, 1, 12};
        QVector<QByteArray> tiles = {makeTile(red ^ blue, tileW, tileH),
                                     makeTile(red ^ green, tileW, tileH),
                                     makeTile(red ^ yellow, tileW, tileH)};

        DeltaEntry delta = createDelta(tileW, tileH, indices, tiles);

        ReconstructionSequence seq;
        seq.base = base;
        seq.baseChecksum = "base-hash";
        seq.deltas = {delta};

        bool success = reconstructor.reconstruct(seq);
        QVERIFY2(success, "Reconstruction failed");

        // Each modified tile must hold its own target color
        for (int y = 0; y < 10; ++y) {
            for (int x = 0; x < 10; ++x) {
                QCOMPARE(reconstructor.samplePixel(x, y), blue);
                QCOMPARE(reconstructor.samplePixel(x + 10, y), green);
                QCOMPARE(reconstructor.samplePixel(x + 20, y + 10), yellow);
            }
        }

        // Untouched pixels remain red
        QCOMPARE(reconstructor.samplePixel(35, 25), red);
        QCOMPARE(reconstructor.samplePixel(99, 99), red);
    }

    void testReconstructToImage() {
        CPUSnapshotReconstructor reconstructor;
        
        QImage base(100, 100, QImage::Format_ARGB32);
        base.fill(Qt::red);
        
        ReconstructionSequence seq;
        seq.base = base;
        seq.baseChecksum = "base-hash";
        
        // Test without downsampling (target size larger or empty)
        QImage res1 = reconstructor.reconstructToImage(seq, QSize(0, 0));
        QCOMPARE(res1.size(), QSize(100, 100));
        QVERIFY(res1.pixel(0, 0) == qRgba(255, 0, 0, 255));
        
        // Test with downsampling
        QImage res2 = reconstructor.reconstructToImage(seq, QSize(50, 50));
        QCOMPARE(res2.size(), QSize(50, 50));
        QVERIFY(res2.pixel(0, 0) == qRgba(255, 0, 0, 255));
    }

    void testSamplePixelBounds() {
        CPUSnapshotReconstructor reconstructor;
        
        QImage base(10, 10, QImage::Format_ARGB32);
        base.fill(Qt::black);
        
        ReconstructionSequence seq;
        seq.base = base;
        seq.baseChecksum = "hash";
        reconstructor.reconstruct(seq);
        
        QVERIFY(reconstructor.samplePixel(-1, 0) == 0);
        QVERIFY(reconstructor.samplePixel(0, -1) == 0);
        QVERIFY(reconstructor.samplePixel(10, 0) == 0);
        QVERIFY(reconstructor.samplePixel(0, 10) == 0);
    }
};

QTEST_MAIN(TestCPUSnapshotReconstructor)
#include "test_cpusnapshotreconstructor.moc"
