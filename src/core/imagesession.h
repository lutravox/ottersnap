#pragma once

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>
#include "core/vksnapshotreconstructor.h"
#include "core/vulkan_types.h"

#include "core/effects_interfaces.h"
#include "core/effectsstate.h"
#include "core/imagemonitor.h"
#include "core/snapshotstore.h"
#include "core/viewstate.h"

/// @brief Manages the state logic for a single opened image.
class ImageSession : public QObject, public IEffectsState {
    Q_OBJECT
  public:
    explicit ImageSession(QObject *parent = nullptr);
    ~ImageSession();

    // IEffectsState implementation
    void setGrayscale(bool enabled) override;
    void setMirror(bool enabled) override;
    bool grayscaleEnabled() const override {
        return m_effects.grayscale;
    }
    bool mirrorEnabled() const override {
        return m_effects.mirror;
    }

    // ViewState access
    ViewState& viewState() {
        return m_viewState;
    }
    const ViewState& viewState() const {
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
    QSize dimensions() const {
        return m_diskImage.size();
    }

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
    /// @brief Manually trigger a snapshot of the current image on disk.
    void saveSnapshot();

    /// @brief Delete a snapshot by its index.
    void deleteSnapshot(int index);

    /// @brief Generate and cache a thumbnail for a specific snapshot.
    QImage generateThumbnail(int index, int size);

    /// @brief Retrieve thumbnails for all available snapshots and the current image.
    std::pair<QVector<QImage>, QVector<QString>> snapshotTimelineThumbnails(int size);

    /// @brief Generate and cache a thumbnail for the currently selected image.
    QImage thumbnail(int size);

    // Getters
    QString filePath() const {
        return m_filePath;
    }
    int currentSnapshotIndex() const {
        return m_selectedIndex;
    }

    bool isCurrentImageSelected() const {
        return isCurrentImage(m_selectedIndex);
    }

    const QVector<ImageSnapshot>& snapshots() const {
        return m_snapshots;
    }
    const QVector<QString>& labels() const {
        return m_labels;
    }

    /// @brief Returns the UI-bound reconstructor for the current session.
    std::shared_ptr<VkSnapshotReconstructor> uiReconstructor() const {
        return m_uiReconstructor;
    }

    /// @brief Initializes the UI reconstructor with the provided device handles.
    void setUIReconstructorHandles(const VulkanHandles& handles);

  signals:
    /// @brief Emitted when the image data changes (requiring UI refresh).
    void imageChanged();
    /// @brief Emitted when the list of available snapshots changes.
    void snapshotsChanged();
    /// @brief Emitted when a specific thumbnail has been updated.
    void thumbnailChanged(int index);
    void effectsChanged();
    /// @brief Emitted with a status message to show to the user.
    void statusMessage(const QString& message, int timeoutMs = -1);

  private slots:
    void handleThumbnailGenerated(const QString& path, int index, const QImage& img);
    void onFileChanged();
    void onDeviceChanged();

  private:
    void   rebuildSnapshotList();
    void   autosaveSnapshot(const QImage& img);
    int    getRelativeVersion(int snapshotIndex) const;
    QImage getPlaceholder(int size);

    QString                m_filePath;
    QImage                 m_diskImage;
    QVector<ImageSnapshot> m_snapshots;
    QVector<QString>       m_labels;
    int                    m_selectedIndex = 0;

    QMap<int, QImage> m_placeholderCache;

    mutable struct {
        int    index = -1;
        QImage image;
    } m_baseCache;

    static std::mutex s_reconstructorMutex;

    EffectsState m_effects;
    ViewState    m_viewState;

    ImageMonitor                            *m_monitor;
    std::shared_ptr<VkSnapshotReconstructor> m_uiReconstructor;
};
