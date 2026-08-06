#pragma once

#include <QWidget>
#include "core/imagesession.h"

/// @brief A tab for an opened image.
class ImageTab : public QWidget {
    Q_OBJECT

  public:
    /// @brief Construct an empty tab.
    /// @param parent Optional parent widget.
    /// @param session The session to associate with this tab.
    explicit ImageTab(QWidget *parent, ImageSession *session);
    ~ImageTab();

    /// @brief Close the current image and release resources.
    void close();
    /// @brief Notify the tab that the associated image has been opened or changed.
    void notifyImageOpened();
    /// @brief Set whether the tab is a snapshot-only tab.
    void setSnapshotOnly(bool snapshotOnly);
    bool isSnapshotOnly() const {
        return m_isSnapshotOnly;
    }

    /// @brief The session associated with this tab.
    ImageSession *session() const {
        return m_session;
    }

    /// @brief Absolute path to the opened image file.
    /// @return File path, or empty string if no image is open.
    QString filePath() const {
        return m_session ? m_session->filePath() : QString();
    }

    /// @brief Returns the image currently selected in the session.
    const QImage& diskImage() const;

    /// @brief Select a snapshot by index.
    void selectSnapshot(int index);

    /// @brief Create a manual snapshot of the current image on disk.
    void saveSnapshot();

    /// @brief Delete a specific snapshot.
    void deleteSnapshot(int index);

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

    /// @brief Emitted when a new snapshot is created.
    /// @param snapshotIndex The unique index of the new snapshot.
    void snapshotCreated(int snapshotIndex);

    /// @brief Emitted when the list of available snapshots changes.
    void snapshotsChanged();

    /// @brief Emitted when a specific thumbnail has been updated.
    void thumbnailUpdated(int index, const QPixmap& pixmap);

    /// @brief Emitted when the tab icon thumbnail has updated.
    void tabIconChanged(const QPixmap& pixmap);

  private slots:
    void onImageChanged();
    void onSnapshotsChanged();
    void onEffectsChanged();
    void onThumbnailChanged(int index);
    void onThumbnailTimerTimeout();

  private:
    void setupUi();

    QTimer        m_thumbnailUpdateTimer;
    bool          m_isSnapshotOnly = false;
    ImageSession *m_session = nullptr;
};
