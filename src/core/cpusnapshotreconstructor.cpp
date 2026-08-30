#include "core/cpusnapshotreconstructor.h"
#include "core/snapshotdecompressor.h"
#include <qlogging.h>

bool CPUSnapshotReconstructor::reconstruct(const ReconstructionSequence& seq) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (!resetToBase(seq.base, seq.baseChecksum))
        return false;

    for (const auto& delta : seq.deltas) {
        if (!applyDelta(delta))
            return false;
    }
    return true;
}

bool CPUSnapshotReconstructor::resetToBase(const QImage& base, const QString& checksum) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_currentState = base.convertToFormat(QImage::Format_ARGB32);
    m_lastChecksum = checksum;
    return true;
}

bool CPUSnapshotReconstructor::applyDelta(const DeltaEntry& delta) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    
    DecompressedDelta decoded;
    if (!SnapshotDecompressor::decompress(delta, decoded)) {
        return false;
    }

    if (decoded.tileIndices.isEmpty()) {
        return true;
    }

    uint32_t width = m_currentState.width();
    uint32_t height = m_currentState.height();
    uint32_t tileW = decoded.tileW;
    uint32_t tileH = decoded.tileH;

    const uint32_t* packedPixels = reinterpret_cast<const uint32_t*>(decoded.packedPixels.data());
    uint32_t* currentPixels = reinterpret_cast<uint32_t*>(m_currentState.bits());

    for (size_t i = 0; i < static_cast<size_t>(decoded.tileIndices.size()); ++i) {
        uint32_t tileIdx = decoded.tileIndices[i];
        uint32_t offset = decoded.tileOffsets[i];

        // Calculate tile coordinates based on the tile grid dimensions
        uint32_t cols = (width + tileW - 1) / tileW;
        uint32_t tx = (tileIdx % cols) * tileW;
        uint32_t ty = (tileIdx / cols) * tileH;

        const uint32_t* tileData = &packedPixels[offset];

        for (uint32_t py = 0; py < tileH; ++py) {
            uint32_t targetY = ty + py;
            if (targetY >= height) break;

            for (uint32_t px = 0; px < tileW; ++px) {
                uint32_t targetX = tx + px;
                if (targetX >= width) break;

                currentPixels[targetY * width + targetX] ^= tileData[py * tileW + px];
            }
        }
    }

    return true;
}

QImage CPUSnapshotReconstructor::reconstructToImage(const ReconstructionSequence& seq,
                                                   QSize                         targetSize) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    if (!reconstruct(seq)) {
        return QImage();
    }
    
    if (targetSize.isEmpty() || (m_currentState.width() <= targetSize.width() && m_currentState.height() <= targetSize.height())) {
        return m_currentState.copy();
    }
    
    return m_currentState.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QRgb CPUSnapshotReconstructor::samplePixel(int x, int y) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (x < 0 || x >= m_currentState.width() || y < 0 || y >= m_currentState.height()) {
        return 0;
    }
    return m_currentState.pixel(x, y);
}
