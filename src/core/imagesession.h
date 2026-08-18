#pragma once

#include <QFutureWatcher>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>
#include "core/coloranalyzer.h"
#include "core/vksnapshotreconstructor.h"
#include "core/vulkan_types.h"

#include "core/effects_interfaces.h"
#include "core/effectsmodel.h"
#include "core/imagemonitor.h"
#include "core/snapshotmanager.h"
#include "core/viewmodel.h"

/// @brief Manages the state logic for a single opened image.
class ImageSession : public QObject, public IEffectsModel {
    Q_OBJECT
  public:
    static constexpr int MaxClusterCacheSize = 50;
    static inline const QString c_currentId = "current";
    static inline const QString c_secondaryNoneId = "";

    explicit ImageSession(QObject *parent = nullptr);
    ~ImageSession();

    void setGrayscale(bool enabled) override;
    void setMirror(bool enabled) override;
    bool grayscaleEnabled() const override {
        return m_effects.grayscale;
    }
    bool mirrorEnabled() const override {
        return m_effects.mirror;
    }

    ViewModel& viewModel() {
        return m_viewState;
    }
    const ViewModel& viewModel() const {
        return m_viewState;
    }

    /// @brief Manually reload the image from disk.
    void reloadImage();

    /// @brief Open an image file and initialize the session.
    bool openImage(const QString& filePath);
    /// @brief Close the current image and release resources.
    void close();

    /// @brief Return the current image loaded from disk.
    QImage& diskImage() {
        return m_diskImage;
    }

    /// @brief Return the dimensions of the image.
    QSize dimensions() const;

    /// @brief Retrieve the reconstruction sequence (base image and deltas) for the current
    /// snapshot.
    std::optional<ReconstructionSequence> getReconstructionSequence() const;

    /// @brief Retrieve the reconstruction sequence (base image and deltas) for a relative
    /// snapshot index.
    std::optional<ReconstructionSequence> getReconstructionSequence(int index) const;

    /// @brief Check if the given index refers to the current image (disk image).
    bool isCurrentImage(int index) const;

    /// @brief Select a snapshot by index.
    void selectSnapshot(int index);
    /// @brief Select a snapshot by its UUID.
    void selectSnapshot(const QUuid& uuid);
    /// @brief Select a snapshot by its identity string.
    void selectSnapshot(const QString& uuid);
    /// @brief Manually trigger a snapshot of the current image on disk.
    void saveSnapshot();

    /// @brief Delete a snapshot by its UUID.
    void deleteSnapshot(const QUuid& uuid);

    /// @brief Generate and cache a thumbnail for a specific snapshot.
    QImage generateThumbnail(int index, int size, bool padded = true);

    /// @brief Retrieve thumbnails for all available snapshots and the current image.
    std::tuple<QVector<QImage>, QVector<QString>, QVector<QUuid>>
    snapshotTimelineThumbnails(int size);

    /// @brief Generate and cache a thumbnail for the currently selected image.
    QImage thumbnail(int size);

    /// @brief Rebuild the internal list of snapshots from disk.
    void rebuildSnapshotList();

    QString filePath() const {
        return m_filePath;
    }
    QString currentUuid() const {
        return m_currentUuid;
    }

    int currentSnapshotIndex() const {
        if (m_currentUuid == c_currentId) {
            return static_cast<int>(m_snapshots.size());
        }
        for (int i = 0; i < static_cast<int>(m_snapshots.size()); ++i) {
            if (m_snapshots[i].uuid.toString(QUuid::WithoutBraces) == m_currentUuid) {
                return i;
            }
        }
        return -1;
    }

    QString secondarySnapshotId() const {
        return m_secondarySnapshotId;
    }

    void setSecondarySnapshotId(const QString& id) {
        m_secondarySnapshotId = id;
    }

    bool isCurrentImageSelected() const {
        return m_currentUuid == c_currentId;
    }

    const QVector<ImageSnapshot>& snapshots() const {
        return m_snapshots;
    }
    QDateTime lastModified() const {
        return m_lastModified;
    }

    /// @brief Return the maximum valid index for the current session mode.
    int maxValidIndex() const;

    /// @brief Returns the color clusters for the currently selected image.
    /// @return A list of clusters containing their center, average color, and count.
    QList<ColorAnalyzer::ColorCluster> colorClusters() const {
        return m_colorClusters;
    }

    /// @brief Calculates the color clusters using the thumbnail.
    void updateColorClusters();

    /// @brief Return the relative version of a snapshot on the timeline (1-based).
    int getRelativeVersion(const QUuid& uuid) const;

    /// @brief Returns the UI-bound reconstructor for the current session.
    std::shared_ptr<VkSnapshotReconstructor> uiReconstructor() const {
        return m_uiReconstructor;
    }

    /// @brief Returns a formatted timestamp string for the currently selected image.
    QString currentImageTimestamp() const;

    /// @brief Initializes the UI reconstructor with the provided device handles.
    void setUIReconstructorHandles(const VulkanHandles& handles);
    /// @brief Set whether the session is in snapshot-only mode (original file missing).
    void setSnapshotOnly(bool snapshotOnly) {
        m_isSnapshotOnly = snapshotOnly;
    }
    bool isSnapshotOnly() const {
        return m_isSnapshotOnly;
    }

  signals:
    /// @brief Emitted when the image data changes (requiring UI refresh).
    void imageChanged();
    /// @brief Emitted when the list of available snapshots changes.
    void snapshotsChanged();
    /// @brief Emitted when a new snapshot is created.
    void snapshotCreated(const QUuid& uuid);
    /// @brief Emitted when a specific thumbnail has been updated.
    void thumbnailChanged(int index);
    void secondarySnapshotChanged(const QString& id);
    void effectsChanged();
    void colorClustersChanged();
    /// @brief Emitted with a status message to show to the user.
    void statusMessage(const QString& message, int timeoutMs = -1);

  private slots:
    void handleThumbnailGenerated(const QString& path, const QUuid& uuid);
    void onFileChanged();
    void onDeviceChanged();

  private:
    void   autosaveSnapshot(const QImage& img);
    void   performSave(const QImage& img, bool isAutosave);
    void   handleSaveFinished(QFutureWatcher<std::optional<SnapshotManager::SaveResult>> *watcher,
                              bool                                                        isAutosave);
    QImage getPlaceholder(int size);

    bool                   m_isSnapshotOnly = false;
    QDateTime              m_lastModified;
    QString                m_filePath;
    QImage                 m_diskImage;
    QVector<ImageSnapshot> m_snapshots;
    QVector<QString>       m_labels;
    QString m_currentUuid = c_currentId;
    QString                  m_secondarySnapshotId = c_secondaryNoneId;

    QMap<int, QImage> m_placeholderCache;

    mutable struct {
        int    index = -1;
        QImage image;
    } m_baseCache;

    static std::mutex s_reconstructorMutex;

    EffectsModel m_effects;
    ViewModel    m_viewState;

    ImageMonitor                                   *m_monitor;
    std::shared_ptr<VkSnapshotReconstructor>        m_uiReconstructor;
    QList<ColorAnalyzer::ColorCluster>              m_colorClusters;
    QCache<QString, QList<ColorAnalyzer::ColorCluster>> m_clusterCache;
};
