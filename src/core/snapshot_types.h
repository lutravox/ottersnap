#pragma once

#include <QImage>
#include <QString>
#include <QByteArray>
#include <QVector>
#include <cstdint>

/// @brief A single delta entry describing changes from a base image.
struct DeltaEntry {
    QString id;            ///< Unique identifier for this delta (often a UUID).
    QByteArray data;       ///< Compressed delta data.
};

/// @brief A sequence of a base image and deltas required to reconstruct a snapshot.
struct ReconstructionSequence {
    int                 baseIdx;   ///< Index of the base image in the sequence.
    QImage              base;      ///< The starting image.
    QString             baseChecksum; ///< Checksum to verify base image integrity.
    QVector<DeltaEntry> deltas;    ///< Ordered list of deltas to apply.
};

/// @brief Decompressed delta data ready for application.
struct DecompressedDelta {
    uint32_t tileW;                ///< Width of tiles in this delta.
    uint32_t tileH;                ///< Height of tiles in this delta.
    QByteArray packedPixels;       ///< Concatenated pixel data for all tiles.
    QVector<uint32_t> tileIndices; ///< Mapping of tile index to image location.
    QVector<uint32_t> tileOffsets; ///< Offsets into packedPixels for each tile.
};
