#include <QFileInfo>
#include "controllers/imagesessioncontroller.h"
#include "controllers/appsettingscontroller.h"
#include "core/snapshotmanager.h"

ImageSessionController::ImageSessionController(AppSettingsController *settings, QObject *parent)
    : QObject(parent), m_settings(settings) {
}

ImageSessionController::~ImageSessionController() {
}

ImageSession *ImageSessionController::openImage(const QString& path, bool snapshotOnly) {
    if (auto *existing = m_sessions.value(path)) {
        if (QFileInfo(path).lastModified() > existing->lastModified()) {
            if (m_settings->shouldSaveSnapshotOnReopen()) {
                qDebug() << "[ImageSessionController] Triggering saveSnapshot() on reopen";
                existing->saveSnapshot();
            }
            existing->reloadImage();
        }
        return existing;
    }

    // Create a new session
    ImageSession *session = new ImageSession(this);
    session->setSnapshotOnly(snapshotOnly);

    if (!session->openImage(path)) {
        delete session;
        return nullptr;
    }

    m_sessions.insert(path, session);
    return session;
}

bool ImageSessionController::changeActiveSessionPath(const QString& newPath) {
    if (!m_activeSession)
        return false;

    const QString normalized = SnapshotManager::normalizePath(newPath);
    const QString oldPath = m_activeSession->filePath();
    if (normalized.isEmpty() || normalized == oldPath)
        return false;

    if (m_sessions.contains(normalized))
        return false;

    if (!m_activeSession->setFilePath(normalized))
        return false;

    ImageSession *session = m_sessions.take(oldPath);
    m_sessions.insert(normalized, session);
    return true;
}

void ImageSessionController::closeSession(const QString& path) {
    if (auto *session = m_sessions.take(path)) {
        if (m_activeSession == session) {
            m_activeSession = nullptr;
        }
        session->close();
        delete session;
    }
}

void ImageSessionController::setActiveSession(ImageSession *session) {
    if (m_activeSession == session)
        return;

    if (m_activeSession) {
        disconnect(m_activeSession,
                   &ImageSession::effectsChanged,
                   this,
                   &ImageSessionController::handleEffectsChanged);
        disconnect(m_activeSession,
                   &ImageSession::snapshotsChanged,
                   this,
                   &ImageSessionController::handleSnapshotsChanged);
        disconnect(m_activeSession,
                   &ImageSession::colorClustersChanged,
                   this,
                   &ImageSessionController::handleColorClustersChanged);
    }

    m_activeSession = session;

    if (m_activeSession) {
        connect(m_activeSession,
                &ImageSession::effectsChanged,
                this,
                &ImageSessionController::handleEffectsChanged);
        connect(m_activeSession,
                &ImageSession::snapshotsChanged,
                this,
                &ImageSessionController::handleSnapshotsChanged);
        connect(m_activeSession,
                &ImageSession::colorClustersChanged,
                this,
                &ImageSessionController::handleColorClustersChanged);
    }

    emit activeSessionChanged(m_activeSession);
}

void ImageSessionController::handleEffectsChanged() {
    emit activeSessionEffectsChanged();
}

void ImageSessionController::handleSnapshotsChanged() {
    emit activeSessionSnapshotsChanged();
}

void ImageSessionController::handleColorClustersChanged() {
    emit activeSessionColorClustersChanged();
}

void ImageSessionController::selectSnapshot(const QString& uuid) {
    if (m_activeSession) {
        m_activeSession->selectSnapshot(uuid);
    }
}

void ImageSessionController::saveSnapshot() {
    if (m_activeSession) {
        m_activeSession->saveSnapshot();
    }
}

void ImageSessionController::deleteSnapshot(const QUuid& uuid, bool silent) {
    if (m_activeSession) {
        m_activeSession->deleteSnapshot(uuid, silent);
    }
}

void ImageSessionController::deleteAllSnapshots() {
    if (m_activeSession) {
        SnapshotManager::deleteAllSnapshots(m_activeSession->filePath());
        m_activeSession->rebuildSnapshotList();

        notifySnapshotChanged(m_activeSession->filePath());
    }
}

ImageSession *ImageSessionController::sessionForPath(const QString& path) const {
    return m_sessions.value(path);
}

QStringList ImageSessionController::openPaths() const {
    return m_sessions.keys();
}

void ImageSessionController::notifySnapshotChanged(const QString& filePath) {
    ImageSession *session = sessionForPath(filePath);
    if (!session)
        return;

    session->rebuildSnapshotList();

    if (session->isSnapshotOnly() && session->snapshots().isEmpty()) {
        emit sessionInvalidated(filePath);
    }

    if (m_activeSession == session) {
        emit activeSessionSnapshotsChanged();
    }
}
