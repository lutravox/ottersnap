#pragma once

#include <QWidget>
#include "core/imagesession.h"

/// @brief A tab for an opened image.
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

    /// @brief The session associated with this tab.
    ImageSession *session() const {
        return m_session;
    }

    /// @brief Absolute path to the opened image file.
    /// @return File path, or empty string if no image is open.
    QString filePath() const {
        return m_session ? m_session->filePath() : QString();
    }

    /// @brief Returns the currently selected image.
    const QImage& currentImage() const;

    /// @brief Select a snapshot by index.
    void selectSnapshot(int index);

    /// @brief Create a manual snapshot of the current image on disk.
    void saveSnapshot();

    /// @brief Delete a specific snapshot.
    void deleteSnapshot(int index);

    /// @brief Fetches thumbnails of all snapshots and current image.
    /// @param size Desired thumbnail size in pixels (square).
    /// @return thumbnails and associated labels
    std::pair<QVector<QPixmap>, QVector<QString>> snapshotThumbnails(int size) const;

    /// @param size Desired thumbnail size in pixels (square).
    QPixmap thumbnail(int size) const;

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
    void onEffectsChanged();

  private:
    void setupUi();

    // Remove the state members
    ImageSession *m_session = nullptr;
};
