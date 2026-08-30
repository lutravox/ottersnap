#include "core/snapshotdecompressor.h"
#include <QDataStream>
#include <QDebug>
#include <qlogging.h>

bool SnapshotDecompressor::decompress(const DeltaEntry& delta, DecompressedDelta& out) {
    const QByteArray& compressedDelta = delta.data;

    QDataStream stream(compressedDelta);
    stream.setByteOrder(QDataStream::LittleEndian);

    uint32_t version = 0;
    stream >> version;
    if (version != 1) {
        qWarning() << "[SnapshotDecompressor] Unsupported delta format version:" << version;
        return false;
    }

    uint32_t numTiles = 0;
    stream >> out.tileW >> out.tileH >> numTiles;

    if (out.tileW == 0 || out.tileH == 0 || out.tileW > 4096 || out.tileH > 4096) {
        qCritical() << "[SnapshotDecompressor] Delta corruption: invalid tile dimensions ("
                    << out.tileW << "x" << out.tileH << ")";
        return false;
    }

    if (numTiles == 0) {
        return true;
    }

    if (numTiles > 1000000) {
        qCritical() << "[SnapshotDecompressor] Delta corruption: numTiles too large ("
                    << numTiles << ")";
        return false;
    }

    struct TileData {
        uint32_t   index;
        QByteArray compressed;
    };
    QVector<TileData> tiles;
    tiles.reserve(numTiles);
    for (uint32_t i = 0; i < numTiles; ++i) {
        uint32_t   idx = 0;
        QByteArray compressed;
        stream >> idx >> compressed;
        tiles.append({idx, compressed});
    }

    struct UncompressedTile {
        uint32_t   index;
        QByteArray pixels;
    };
    QVector<UncompressedTile> uncompressed;
    uncompressed.reserve(numTiles);

    for (const auto& td : tiles) {
        QByteArray data = qUncompress(td.compressed);
        if (data.isEmpty()) {
            qCritical() << "[SnapshotDecompressor] Failed to decompress tile" << td.index;
            return false;
        }
        uncompressed.append({td.index, std::move(data)});
    }

    out.packedPixels.clear();
    out.tileIndices.clear();
    out.tileOffsets.clear();
    out.packedPixels.reserve(numTiles * out.tileW * out.tileH * 4);

    for (const auto& tile : uncompressed) {
        out.tileOffsets.append(out.packedPixels.size() / sizeof(uint32_t));
        out.packedPixels.append(tile.pixels);
        out.tileIndices.append(tile.index);
    }

    return true;
}
