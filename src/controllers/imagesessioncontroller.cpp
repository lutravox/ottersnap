#include <QFileInfo>
#include "controllers/imagesessioncontroller.h"
#include "controllers/appsettingscontroller.h"
#include "core/diskutils.h"

ImageSessionController::ImageSessionController(AppSettingsController *settings, QObject *parent)
    : QObject(parent), m_settings(settings) {
}

ImageSessionController::~ImageSessionController() {
    for (auto session : m_sessions) {
        delete session;
    }
    m_sessions.clear();
}

ImageSession *ImageSessionController::openImage(const QString& path) {
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
    if (!session->openImage(path)) {
        delete session;
        return nullptr;
    }

    m_sessions.insert(path, session);
    return session;
}

void ImageSessionController::closeSession(const QString& path) {
    if (auto *session = m_sessions.take(path)) {
        session->close();
        delete session;
    }
}

ImageSession *ImageSessionController::sessionForPath(const QString& path) const {
    return m_sessions.value(path);
}

QStringList ImageSessionController::openPaths() const {
    return m_sessions.keys();
}
