#pragma once

#include <optional>

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVector>
#include "core/snapshotmanager.h"

/// @brief Manages the SQLite database for snapshot metadata.
///
/// Database Structure:
///
/// Table: images
/// - image_key: TEXT PRIMARY KEY (Unique identifier for the session)
/// - file_path: TEXT NOT NULL (Path to original image file)
///
/// Table: snapshots
/// - id: INTEGER PRIMARY KEY AUTOINCREMENT
/// - uuid: TEXT NOT NULL (Unique identifier)
/// - parent_uuid: TEXT (Parent snapshot UUID for delta chaining)
/// - image_key: TEXT NOT NULL (FK -> images.image_key, ON DELETE CASCADE)
/// - timestamp: TEXT NOT NULL (ISO 8601 date string)
/// - checksum: TEXT NOT NULL (Data integrity checksum)
/// - is_base: INTEGER NOT NULL (1 = Full base image, 0 = Delta)
/// - file_name: TEXT NOT NULL (Filename of the snapshot on disk)
///
/// `images.file_path` is unique: a path is registered under at most one image key.
class SnapshotDatabase {
  public:
    static SnapshotDatabase& instance();

    /// @brief Initialize the database and create tables if they don't exist.
    bool init(const QString& dbPath = QString());

    /// @brief Store an image and its associated path.
    bool registerImage(const QString& key, const QString& path);

    /// @brief Look up the image key registered for a file path.
    /// @param path Absolute path of the image file.
    /// @return The registered image key, or std::nullopt if the path is unknown.
    std::optional<QString> keyForPath(const QString& path);

    /// @brief Re-point a registered image to a new file path.
    /// @param key The image key to update.
    /// @param newPath New absolute path for the image.
    /// @return True on success; false if the key is unknown or newPath is already
    /// registered under a different key.
    bool setImagePath(const QString& key, const QString& newPath);

    /// @brief Add a snapshot record for an image.
    bool addSnapshot(const QString& key, const ImageSnapshot& s);

    /// @brief Load all snapshots for a given image key.
    QVector<ImageSnapshot> getSnapshots(const QString& key);

    /// @brief Delete a specific snapshot record.
    bool removeSnapshot(const QString& key, const QUuid& uuid);

    /// @brief Update an existing snapshot record.
    bool updateSnapshot(const QString& key, const ImageSnapshot& s);

    /// @brief Delete all snapshots for a given image.
    bool clearImage(const QString& key);

    /// @brief Retrieve all images that have at least one snapshot.
    struct ImageRecord {
        QString key;
        QString path;
    };
    QVector<ImageRecord> getAllSnapshottedImages();

    /// @brief Remove an image from the registry if it no longer has snapshots.
    void pruneImage(const QString& key);

    /// @brief Get the database connection for the current thread.
    QSqlDatabase connection();

  private:
    SnapshotDatabase() = default;
    ~SnapshotDatabase() = default;
    SnapshotDatabase(const SnapshotDatabase&) = delete;
    SnapshotDatabase& operator=(const SnapshotDatabase&) = delete;

    bool    m_initialized = false;
    QString m_dbPath;
};
