#pragma once

#include <QImage>
#include <QSize>
#include <QString>
#include "core/snapshot_types.h"

/// @brief Interface for snapshot reconstruction.
class ISnapshotReconstructor {
  public:
    virtual ~ISnapshotReconstructor() = default;

    /// @brief Reconstructs a snapshot from a base image and a series of deltas.
    virtual bool reconstruct(const ReconstructionSequence& seq) = 0;

    /// @brief Resets the current state to a base image.
    virtual bool resetToBase(const QImage& base, const QString& checksum) = 0;

    /// @brief Applies a delta to the current state.
    virtual bool applyDelta(const DeltaEntry& delta) = 0;

    /// @brief Reconstructs a snapshot and returns the result as a QImage.
    virtual QImage reconstructToImage(const ReconstructionSequence& seq,
                                      QSize                         targetSize = QSize()) = 0;

    /// @brief Samples a single pixel from the current reconstructed state.
    virtual QRgb samplePixel(int x, int y) = 0;

    /// @brief Releases any backend resources held by the reconstructor.
    virtual void cleanup() {}
};
