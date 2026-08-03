#include <QMutexLocker>
#include "core/deltacache.h"
#include "config/appsettings.h"

QCache<QString, DecompressedDelta> DeltaCache::s_deltaCache;
QMutex                             DeltaCache::s_mutex;

struct DeltaCacheInitializer {
    DeltaCacheInitializer() {
        DeltaCache::updateMaxCost(AppSettings::maxDeltaCacheSizeMB());
    }
};
static DeltaCacheInitializer s_initializer;

std::optional<DecompressedDelta> DeltaCache::get(const QString& deltaId) {
    QMutexLocker       locker(&s_mutex);
    DecompressedDelta *obj = s_deltaCache.object(deltaId);
    return obj ? std::make_optional<DecompressedDelta>(*obj) : std::nullopt;
}

void DeltaCache::insert(const QString& deltaId, const DecompressedDelta& data) {
    QMutexLocker locker(&s_mutex);
    int          cost = data.packedPixels.size();
    s_deltaCache.insert(deltaId, new DecompressedDelta(data), cost);
}

void DeltaCache::updateMaxCost(int sizeMB) {
    QMutexLocker locker(&s_mutex);
    int          maxBytes = sizeMB * 1024 * 1024;
    if (s_deltaCache.maxCost() != maxBytes) {
        s_deltaCache.setMaxCost(maxBytes);
    }
}

void DeltaCache::clear() {
    QMutexLocker locker(&s_mutex);
    s_deltaCache.clear();
}
