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
    int     snapshotIndex;
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

  signals:
    void thumbnailGenerated(const QString& filePath, int index, const QImage& img);

  private:
    ThumbnailManager(QObject *parent = nullptr);
    ~ThumbnailManager() override = default;

    void processNext();

    bool isCurrentImage(int index, const QVector<ImageSnapshot>& snapshots) const {
        return index == static_cast<int>(snapshots.size());
    }

    void onReconstructionFinished(QFutureWatcher<std::optional<QImage>> *watcher,
                                  const ThumbnailRequest&                request);

    void saveThumbnail(const QString& filePath, int snapshotIndex, const QImage& image);
    static std::optional<QImage> reconstructDiskImage(const QString& path,
                                                      const QImage&  currentImage = QImage());

    static std::optional<QImage> reconstructThumbnail(const QString& path, int snapshotIdx);

    QQueue<ThumbnailRequest> m_queue;
    QSet<QString>            m_activeRequests;
    bool                     m_isProcessing = false;
};
