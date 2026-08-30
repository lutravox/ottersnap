#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include "controllers/appsettingscontroller.h"
#include "core/imagesession.h"

/// @brief Coordinates the lifecycle and opening of image sessions.
class ImageSessionController : public QObject {
    Q_OBJECT

  public:
    explicit ImageSessionController(AppSettingsController *settings, QObject *parent = nullptr);
    ~ImageSessionController();

    /// @brief Open an image or return the existing session if already open.
    /// @param path Absolute path to the image file.
    /// @param snapshotOnly Whether to open in snapshot-only mode (skipping disk load).
    /// @return The session associated with the image, or nullptr on failure.
    ImageSession *openImage(const QString& path, bool snapshotOnly = false);

    /// @brief Close the session associated with the given path.
    /// @param path Absolute path to the image file.
    void closeSession(const QString& path);

    /// @brief Re-point the active session to a new location of its image file.
    /// @param newPath New absolute path to the image file.
    /// @return True on success, false if there is no active session, the new
    /// path is already open, or the update failed.
    bool changeActiveSessionPath(const QString& newPath);

    /// @brief Select a snapshot in the active session.
    void selectSnapshot(const QString& uuid);

    /// @brief Trigger a snapshot of the active session.
    void saveSnapshot();

    /// @brief Delete a snapshot in the active session.
    void deleteSnapshot(const QUuid& uuid, bool silent = false);

    /// @brief Delete multiple snapshots in the active session.
    void deleteSnapshots(const QVector<QUuid>& uuids, bool silent = false);

    /// @brief Delete all snapshots for the active session.
    void deleteAllSnapshots();

    /// @brief Return the session associated with the given path.
    ImageSession *sessionForPath(const QString& path) const;

    /// @brief Return the currently active session.
    ImageSession *activeSession() const {
        return m_activeSession;
    }

    /// @brief Set the currently active session.
    void setActiveSession(ImageSession *session);

    /// @brief Return a list of all currently open image paths.
    QStringList openPaths() const;

    /// @brief Check if mirroring is enabled for the active session.
    bool isMirrorEnabled() const {
        return m_activeSession ? m_activeSession->mirrorEnabled() : false;
    }

    /// @brief Notify the controller that snapshots have changed for a given image.
    void notifySnapshotChanged(const QString& filePath, bool sessionAlreadyUpdated = false);

  signals:
    /// @brief Emitted when the active session changes.
    void activeSessionChanged(ImageSession *session);

    /// @brief Emitted when a session becomes invalid (e.g., all snapshots deleted in snapshot-only
    /// mode).
    void sessionInvalidated(const QString& filePath);

    /// @brief Emitted when the active session's effects are changed.
    void activeSessionEffectsChanged();

    /// @brief Emitted when the active session's snapshots are changed.
    void activeSessionSnapshotsChanged();

    /// @brief Emitted when the active session's color clusters are changed.
    void activeSessionColorClustersChanged();

  private:
    void handleEffectsChanged();
    void handleSnapshotsChanged();
    void handleColorClustersChanged();

    AppSettingsController        *m_settings;
    QMap<QString, ImageSession *> m_sessions;
    ImageSession                 *m_activeSession = nullptr;
};
