#pragma once

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

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

    /// @brief Return the currently active image.
    const QImage& currentImage();
    /// @brief Select a snapshot by index.
    void selectSnapshot(int index);
    /// @brief Manually trigger a snapshot of the current image on disk.
    void saveSnapshot();

    /// @brief Delete a snapshot by its index.
    void deleteSnapshot(int index);

    /// @brief Retrieve thumbnails for all available snapshots and the current image.
    std::pair<QVector<QImage>, QVector<QString>> snapshotThumbnails(int size);

    // Getters
    QString filePath() const {
        return m_filePath;
    }
    int currentSnapshotIndex() const {
        return m_currentIndex;
    }
    const QVector<ImageSnapshot>& snapshots() const {
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
    void effectsChanged();
    /// @brief Emitted with a status message to show to the user.
    void statusMessage(const QString& message, int timeoutMs = -1);

  private slots:
    void onFileChanged();

  private:
    void rebuildSnapshotList();
    bool autosaveSnapshot(const QImage& newImage);
    int  getRelativeVersion(int snapshotIndex) const;

    QString                m_filePath;
    QImage                 m_diskImage;
    QImage                 m_cachedImage;
    QVector<ImageSnapshot> m_snapshots;
    QVector<QString>       m_labels;
    int                    m_currentIndex = 0;
    int                    m_loadedSnapshotIndex = -1;

    EffectsState m_effects;
    ViewState    m_viewState;

    ImageMonitor *m_monitor;
};
