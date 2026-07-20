#pragma once

#include <QFileSystemWatcher>
#include <QImage>
#include <QPixmap>
#include <QString>
#include <QVector>
#include <QWidget>
#include <utility>

#include <QFuture>
#include <QFutureWatcher>

#include "core/imagesession.h"
#include "core/viewstate.h"

/// @brief A single image tab. Manages image loading, version history,
/// image modifiers, and file-system watching.
class ImageTab : public QWidget {
    Q_OBJECT

  public:
    /// @brief Construct an empty tab.
    /// @param parent Optional parent widget.
    explicit ImageTab(QWidget *parent = nullptr);
    ~ImageTab();

    /// @brief Open an image file and populate the tab.
    /// @param filePath Absolute path to the image file.
    void openImage(const QString& filePath);

    /// @brief Close the current image and release resources.
    void closeImage();

    /// @brief Absolute path to the opened image file.
    /// @return File path, or empty string if no image is open.
    QString filePath() const {
        return m_session ? m_session->filePath() : QString();
    }

    /// @brief Generate a thumbnail of the current image.
    /// @param size Desired thumbnail size in pixels (square).
    /// @return Scaled thumbnail, or null pixmap if no image.
    QPixmap thumbnail(int size = 40) const;

    /// @brief Index of the currently displayed snapshot.
    /// @return Zero-based snapshot index.
    int currentSnapshotIndex() const {
        return m_session ? m_session->currentSnapshotIndex() : 0;
    }

    /// @brief Enable or disable grayscale rendering.
    void setGrayscale(bool enabled);

    /// @brief Enable or disable horizontal mirroring.
    void setMirror(bool enabled);

    /// @brief Returns whether grayscale is enabled.
    bool grayscaleEnabled() const {
        return m_grayscale;
    }

    /// @brief Returns whether mirroring is enabled.
    bool mirrorEnabled() const {
        return m_mirror;
    }

    /// @brief Returns the currently selected image.
    const QImage& currentImage() const;

    /// @brief Select a snapshot by index.
    void selectSnapshot(int index);

    /// @brief Returns the current view state.
    const ViewState& viewState() const {
        return m_viewState;
    }

    /// @brief Sets the current view state.
    void setViewState(const ViewState& state) {
        m_viewState = state;
    }

    /// @brief Create a manual snapshot of the current image on disk.
    void saveSnapshot();

    /// @param size Desired thumbnail size in pixels (square).
    /// @return Pair of thumbnail pixmaps and their labels.
    std::pair<QVector<QPixmap>, QVector<QString>> snapshotThumbnails(int size) const;

  signals:
    /// @brief Emitted with a status message to show to the user.
    /// @param message Status text.
    /// @param timeoutMs Optional timeout in ms. If -1, use default.
    void statusMessage(const QString& message, int timeoutMs = -1);

    /// @brief Emitted when the tab's image modifier state changes.
    /// @param grayscale Current grayscale state.
    /// @param mirror    Current mirror state.
    void effectsChanged(bool grayscale, bool mirror);

    /// @brief Emitted when the active snapshot changes.
    /// @param index New snapshot index.
    void snapshotChanged(int index);

    /// @brief Emitted when the list of available snapshots changes.
    void snapshotsChanged();

  private slots:
    void onImageChanged();
    void onSnapshotsChanged();

  private:
    void setupUi();

    bool      m_grayscale = false;
    bool      m_mirror = false;
    ViewState m_viewState;

    ImageSession *m_session = nullptr;
};
