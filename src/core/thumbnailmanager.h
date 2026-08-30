#pragma once

#include <QFutureWatcher>
#include <QImage>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QVector>
#include <optional>

#include "core/snapshotmanager.h"

struct ThumbnailRequest {
    int     index;
    QString filePath;
    QUuid   uuid;
    QImage  currentImage;
};

class ThumbnailManager : public QObject {
    Q_OBJECT
  public:
    /// @brief Returns the singleton instance of the ThumbnailManager.
    /// @return Reference to the global ThumbnailManager instance.
    static ThumbnailManager& instance();

    /// @brief Returns a thumbnail if available in cache, otherwise queues
    /// reconstruction and returns a null image.
    /// @param index The index of the snapshot.
    /// @param size The desired thumbnail size.
    /// @param filePath Absolute path of the source image.
    /// @param isCurrent Whether this is the currently active snapshot.
    /// @param snapshots List of available snapshots for this file.
    /// @return The cached thumbnail image, or a null image if reconstruction was queued.
    QImage getThumbnail(int                           index,
                        int                           size,
                        const QString&                filePath,
                        bool                          isCurrent,
                        const QVector<ImageSnapshot>& snapshots,
                        const QImage&                 currentImage = QImage());

    /// @brief Enqueues a request to generate a thumbnail.
    /// @param request The thumbnail request details.
    void enqueueRequest(const ThumbnailRequest& request);

    /// @brief Formats a thumbnail of the image, centered on a transparent canvas.
    static QImage formatThumbnail(const QImage& image, int size);

    /// @brief Computes the storage thumbnail size preserving the source aspect ratio.
    /// @param sourceSize The dimensions of the source image.
    /// @return A size with at least one dimension equal to the storage size.
    static QSize storageTargetSize(const QSize& sourceSize);

    /// @brief Persists a thumbnail to disk and the memory cache and notifies listeners.
    /// @param filePath Absolute path of the source image.
    /// @param uuid The snapshot identity (null for the current disk image).
    /// @param image The pre-scaled thumbnail to publish.
    void publishThumbnail(const QString& filePath, const QUuid& uuid, const QImage& image);

  signals:
    void thumbnailGenerated(const QString& filePath, const QUuid& snapshotUuid, const QImage& img);

  private:
    ThumbnailManager(QObject *parent = nullptr);
    ~ThumbnailManager() override = default;

    void processNext();

    bool isCurrentImage(int index, const QVector<ImageSnapshot>& snapshots) const {
        return index == static_cast<int>(snapshots.size());
    }

    void onReconstructionFinished(QFutureWatcher<std::optional<QImage>> *watcher,
                                  const ThumbnailRequest&                request);

    void saveThumbnail(const QString& filePath, const QUuid& uuid, const QImage& image);
    static std::optional<QImage> reconstructDiskImage(const QString& path,
                                                      const QImage&  currentImage = QImage());

    static std::optional<QImage> reconstructThumbnail(const QString& path, const QUuid& uuid);

    QString getIdentityString(const QUuid& uuid) const;
    QString getRequestKey(const QString& filePath, const QUuid& uuid) const;

    static constexpr int MaxConcurrentReconstructions = 4;

    QQueue<ThumbnailRequest> m_queue;
    QSet<QString>            m_activeRequests;
    int                      m_inFlight = 0;
};
