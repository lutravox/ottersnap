#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QThread>
#include "core/snapshotdb.h"

static const QString c_connectionName = "snapshots_db";

SnapshotDatabase& SnapshotDatabase::instance() {
    static SnapshotDatabase inst;
    return inst;
}

bool SnapshotDatabase::init(const QString& dbPath) {
    if (m_initialized)
        return true;

    if (!dbPath.isEmpty()) {
        m_dbPath = dbPath;
    } else {
        m_dbPath =
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/snapshots.db";
    }
    // Ensure the directory exists before trying to open the database file
    QFileInfo fileInfo(m_dbPath);
    QString   dirPath = fileInfo.absolutePath();
    QDir      dir(dirPath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "[SnapshotDb] Failed to create database directory:" << dirPath;
            return false;
        }
    }
    // Use the standard connection to initialize tables
    QSqlDatabase db = connection();

    if (!db.isOpen()) {
        qWarning() << "[SnapshotDb] Failed to open database for init:" << db.lastError().text();
        return false;
    }

    QSqlQuery query(db);

    // Images table
    if (!query.exec("CREATE TABLE IF NOT EXISTS images ("
                    "image_key TEXT PRIMARY KEY, "
                    "file_path TEXT NOT NULL UNIQUE)")) {
        qWarning() << "[SnapshotDb] Failed to create images table:" << query.lastError().text();
        return false;
    }

    // Snapshots table
    if (!query.exec("CREATE TABLE IF NOT EXISTS snapshots ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "uuid TEXT NOT NULL, "
                    "parent_uuid TEXT, "
                    "image_key TEXT NOT NULL, "
                    "timestamp TEXT NOT NULL, "
                    "checksum TEXT NOT NULL, "
                    "is_base INTEGER NOT NULL, "
                    "file_name TEXT NOT NULL, "
                    "FOREIGN KEY(image_key) REFERENCES images(image_key) ON DELETE CASCADE)")) {
        qWarning() << "[SnapshotDb] Failed to create snapshots table:"
                   << query.lastError().text();
        return false;
    }

    m_initialized = true;
    return true;
}

bool SnapshotDatabase::registerImage(const QString& key, const QString& path) {
    QSqlQuery query(connection());
    query.prepare("INSERT OR IGNORE INTO images (image_key, file_path) VALUES (?, ?)");
    query.addBindValue(key);
    query.addBindValue(path);
    return query.exec();
}

std::optional<QString> SnapshotDatabase::keyForPath(const QString& path) {
    QSqlQuery query(connection());
    query.prepare("SELECT image_key FROM images WHERE file_path = ?");
    query.addBindValue(path);
    if (!query.exec()) {
        qWarning() << "[SnapshotDb] Failed to look up key for" << path << ":"
                   << query.lastError().text();
        return std::nullopt;
    }
    if (query.next()) {
        return query.value(0).toString();
    }
    return std::nullopt;
}

bool SnapshotDatabase::setImagePath(const QString& key, const QString& newPath) {
    QSqlQuery query(connection());
    query.prepare("UPDATE images SET file_path = ? WHERE image_key = ?");
    query.addBindValue(newPath);
    query.addBindValue(key);
    if (!query.exec()) {
        qWarning() << "[SnapshotDb] Failed to update path for" << key << ":"
                   << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() == 1;
}

bool SnapshotDatabase::addSnapshot(const QString& key, const ImageSnapshot& s) {
    QSqlQuery query(connection());
    query.prepare(
        "INSERT INTO snapshots (uuid, parent_uuid, image_key, timestamp, checksum, is_base, "
        "file_name) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(s.uuid.toString(QUuid::WithoutBraces));
    query.addBindValue(s.parentUuid.toString(QUuid::WithoutBraces));
    query.addBindValue(key);
    query.addBindValue(s.timestamp.toString(Qt::ISODate));
    query.addBindValue(s.checksum);
    query.addBindValue(s.isBase ? 1 : 0);
    query.addBindValue(s.fileName);

    bool success = query.exec();
    if (!success) {
        qWarning() << "[SnapshotDb] Failed to add snapshot for" << key << ":"
                   << query.lastError().text();
    }

    return success;
}

QVector<ImageSnapshot> SnapshotDatabase::getSnapshots(const QString& key) {
    QVector<ImageSnapshot> snapshots;
    QSqlQuery              query(connection());
    query.prepare("SELECT uuid, parent_uuid, file_name, timestamp, checksum, is_base "
                  "FROM snapshots WHERE image_key = ? ORDER BY id ASC");
    query.addBindValue(key);

    if (!query.exec()) {
        qWarning() << "[SnapshotDb] Failed to load snapshots for" << key << ":"
                   << query.lastError().text();
        return snapshots;
    }

    while (query.next()) {
        ImageSnapshot s;
        s.uuid = QUuid::fromString(query.value(0).toString());
        s.parentUuid = QUuid::fromString(query.value(1).toString());
        s.fileName = query.value(2).toString();
        s.timestamp = QDateTime::fromString(query.value(3).toString(), Qt::ISODate);
        s.checksum = query.value(4).toString();
        s.isBase = query.value(5).toInt() != 0;
        snapshots.append(s);
    }

    return snapshots;
}

bool SnapshotDatabase::removeSnapshot(const QString& key, const QUuid& uuid) {
    QSqlQuery query(connection());
    query.prepare("DELETE FROM snapshots WHERE image_key = ? AND uuid = ?");
    query.addBindValue(key);
    query.addBindValue(uuid.toString(QUuid::WithoutBraces));
    return query.exec();
}

bool SnapshotDatabase::clearImage(const QString& key) {
    QSqlQuery query(connection());
    query.prepare("DELETE FROM images WHERE image_key = ?");
    query.addBindValue(key);
    return query.exec();
}

QVector<SnapshotDatabase::ImageRecord> SnapshotDatabase::getAllSnapshottedImages() {
    QVector<ImageRecord> images;
    QSqlQuery            query(connection());
    // We only want images that actually have snapshots
    if (!query.exec("SELECT DISTINCT i.image_key, i.file_path FROM images i "
                    "JOIN snapshots s ON i.image_key = s.image_key")) {
        qWarning() << "[SnapshotDb] Failed to load snapshotted images:" << query.lastError().text();
        return images;
    }

    while (query.next()) {
        images.append({query.value(0).toString(), query.value(1).toString()});
    }

    return images;
}

void SnapshotDatabase::pruneImage(const QString& key) {
    QSqlQuery query(connection());
    query.prepare("DELETE FROM images WHERE image_key = ? AND NOT EXISTS "
                  "(SELECT 1 FROM snapshots WHERE image_key = ?)");
    query.addBindValue(key);
    query.addBindValue(key);
    query.exec();
}

bool SnapshotDatabase::updateSnapshot(const QString& key, const ImageSnapshot& s) {
    QSqlQuery query(connection());
    query.prepare("UPDATE snapshots SET timestamp = ?, checksum = ?, "
                  "is_base = ?, file_name = ? "
                  "WHERE image_key = ? AND uuid = ?");
    query.addBindValue(s.timestamp.toString(Qt::ISODate));
    query.addBindValue(s.checksum);
    query.addBindValue(s.isBase ? 1 : 0);
    query.addBindValue(s.fileName);
    query.addBindValue(key);
    query.addBindValue(s.uuid.toString(QUuid::WithoutBraces));
    return query.exec();
}

QSqlDatabase SnapshotDatabase::connection() {
    QString threadConnName =
        c_connectionName + "_" + QString::number((quint64)QThread::currentThreadId());

    if (QSqlDatabase::contains(threadConnName)) {
        return QSqlDatabase::database(threadConnName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", threadConnName);
    db.setDatabaseName(m_dbPath);

    if (!db.open()) {
        qWarning() << "[SnapshotDb] Failed to open connection in thread"
                   << QThread::currentThreadId() << ":" << db.lastError().text();
    }

    QSqlQuery query(db);
    query.exec("PRAGMA foreign_keys = ON;");

    return db;
}
