#pragma once

#include <QFutureWatcher>
#include <QImage>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QVector>
#include <optional>

#include "core/snapshotstore.h"

struct ThumbnailRequest {
    int     index;
    QString filePath;
    int     snapshotIndex;
};

class ThumbnailManager : public QObject {
    Q_OBJECT
  public:
    static ThumbnailManager& instance();

    /// @brief Returns a thumbnail if available in cache, otherwise queues
    /// reconstruction and returns a null image.
    QImage getThumbnail(int                           index,
                        int                           size,
                        const QString&                filePath,
                        bool                          isCurrent,
                        const QVector<ImageSnapshot>& snapshots);

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
    static std::optional<QImage> reconstructDiskImage(const QString& path);
    static std::optional<QImage> reconstructSnapshot(const QString& path, int snapshotIdx);

    QQueue<ThumbnailRequest> m_queue;
    QSet<QString>            m_activeRequests;
    bool                     m_isProcessing = false;
};
