#pragma once

#include <QImage>
#include <QSize>
#include <QString>
#include <mutex>
#include "core/snapshotreconstructor.h"

class CPUSnapshotReconstructor : public ISnapshotReconstructor {
  public:
    CPUSnapshotReconstructor() = default;

    bool reconstruct(const ReconstructionSequence& seq) override;
    bool resetToBase(const QImage& base, const QString& checksum) override;
    bool applyDelta(const DeltaEntry& delta) override;
    QImage reconstructToImage(const ReconstructionSequence& seq,
                              QSize                         targetSize = QSize()) override;
    QRgb samplePixel(int x, int y) override;
    QImage currentState() const override;

  private:
    mutable std::recursive_mutex m_mutex;
    QImage m_currentState;
    bool   m_hasValidState = false;
    QString m_lastChecksum;
};
