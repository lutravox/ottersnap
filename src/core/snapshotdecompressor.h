#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>
#include <QImage>
#include "core/snapshot_types.h"

class SnapshotDecompressor {
  public:
    /// @brief Decompresses a delta into its components.
    static bool decompress(const DeltaEntry& delta, DecompressedDelta& out);
};
