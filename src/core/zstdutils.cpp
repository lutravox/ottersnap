#include "core/zstdutils.h"

#include <QDebug>
#include <zstd.h>

QByteArray ZstdUtils::compress(const QByteArray& data, int level) {
    if (data.isEmpty())
        return {};

    size_t bound = ZSTD_compressBound(static_cast<size_t>(data.size()));
    QByteArray out(static_cast<int>(bound), Qt::Uninitialized);

    size_t written = ZSTD_compress(reinterpret_cast<void*>(out.data()), bound, data.constData(),
                                   static_cast<size_t>(data.size()), level);
    if (ZSTD_isError(written)) {
        qWarning() << "[ZstdUtils] Compression failed:" << ZSTD_getErrorName(written);
        return {};
    }
    out.truncate(static_cast<int>(written));
    return out;
}

QByteArray ZstdUtils::decompress(const QByteArray& data, size_t expectedSize) {
    if (data.isEmpty())
        return {};

    int64_t contentSize = ZSTD_getFrameContentSize(data.constData(), static_cast<size_t>(data.size()));
    if (contentSize < 0 && expectedSize == 0) {
        qWarning() << "[ZstdUtils] Frame size unknown and no expected size given";
        return {};
    }
    size_t outSize = contentSize >= 0 ? static_cast<size_t>(contentSize) : expectedSize;
    if (outSize == 0)
        return {};

    QByteArray out(static_cast<int>(outSize), Qt::Uninitialized);

    size_t decoded = ZSTD_decompress(out.data(), outSize, data.constData(),
                                     static_cast<size_t>(data.size()));
    if (ZSTD_isError(decoded)) {
        qWarning() << "[ZstdUtils] Decompression failed:" << ZSTD_getErrorName(decoded);
        return {};
    }
    out.truncate(static_cast<int>(decoded));
    return out;
}
