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

    /// @brief Return the session associated with the given path.
    ImageSession *sessionForPath(const QString& path) const;

    /// @brief Return a list of all currently open image paths.
    QStringList openPaths() const;

  private:
    AppSettingsController        *m_settings;
    QMap<QString, ImageSession *> m_sessions;
};
