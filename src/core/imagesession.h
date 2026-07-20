#pragma once

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

#include "core/imagemonitor.h"
#include "core/versionstore.h"

/// @brief Manages the state and business logic for a single opened image.
/// This class decouples the image data and versioning logic from the UI.
class ImageSession : public QObject {
    Q_OBJECT
  public:
    explicit ImageSession(QObject *parent = nullptr);
    ~ImageSession();

    /// @brief Open an image file and initialize the session.
    bool openImage(const QString& filePath);
    /// @brief Close the current image and release resources.
    void close();

    /// @brief Return the currently active image (either from disk or history).
    const QImage& currentImage();
    /// @brief Select a snapshot by index.
    void selectSnapshot(int index);
    /// @brief Manually trigger a snapshot of the current image on disk.
    void saveSnapshot();

    /// @brief Retrieve thumbnails for all available versions and the current image.
    std::pair<QVector<QImage>, QVector<QString>> snapshotThumbnails(int size);

    // Getters
    QString filePath() const {
        return m_filePath;
    }
    int currentSnapshotIndex() const {
        return m_currentIndex;
    }
    const QVector<ImageVersion>& snapshots() const {
        return m_snapshots;
    }
    const QVector<QString>& labels() const {
        return m_labels;
    }

  signals:
    /// @brief Emitted when the image data changes (requiring UI refresh).
    void imageChanged();
    /// @brief Emitted when the list of available snapshots changes.
    void snapshotsChanged();
    /// @brief Emitted with a status message to show to the user.
    void statusMessage(const QString& message, int timeoutMs = -1);

  private slots:
    void onFileChanged();

  private:
    void rebuildSnapshotList();
    void reloadImage();
    bool autosaveSnapshot(const QImage& newImage);

    QString               m_filePath;
    QImage                m_diskImage;
    QImage                m_cachedImage;
    QVector<ImageVersion> m_snapshots;
    QVector<QString>      m_labels;
    int                   m_currentIndex = 0;
    int                   m_loadedSnapshotIndex = -1;

    ImageMonitor *m_monitor;
};
