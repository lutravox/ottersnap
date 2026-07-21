#include "core/sessionmanager.h"

#include <QDebug>
#include <QSettings>

SessionManager::SessionManager(QSettings& settings) : m_settings(settings) {
}

void SessionManager::save(const QStringList& paths) {
    qDebug() << "[SessionManager] Paths to save:" << paths;
    m_settings.beginGroup("Session");
    m_settings.setValue("openFiles", paths);
    m_settings.endGroup();
}

void SessionManager::load() {
    m_settings.beginGroup("Session");
    m_restorePaths = m_settings.value("openFiles").toStringList();
    m_settings.endGroup();
    qDebug() << "[SessionManager] Loading paths:" << m_restorePaths;
}

QStringList SessionManager::restorePaths() {
    return std::move(m_restorePaths);
}
