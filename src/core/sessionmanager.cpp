#include "core/sessionmanager.h"

#include <QDebug>
#include <QSettings>
#include <QTabWidget>

#include "ui/imagetab.h"

SessionManager::SessionManager() = default;

void SessionManager::save(QTabWidget *tabBar, QSettings settings) {
    QStringList paths = collectPaths(tabBar);
    qDebug() << "SessionManager::save" << paths;
    settings.beginGroup("Session");
    settings.setValue("openFiles", paths);
    settings.endGroup();
}

void SessionManager::load(QSettings settings) {
    settings.beginGroup("Session");
    m_restorePaths = settings.value("openFiles").toStringList();
    settings.endGroup();
    qDebug() << "SessionManager::load" << m_restorePaths;
}

QStringList SessionManager::restorePaths() {
    return std::move(m_restorePaths);
}

QStringList SessionManager::collectPaths(QTabWidget *tabBar) {
    QStringList paths;
    if (!tabBar)
        return paths;

    for (int i = 0; i < tabBar->count(); ++i) {
        auto *tab = qobject_cast<ImageTab *>(tabBar->widget(i));
        if (tab && !tab->filePath().isEmpty())
            paths << tab->filePath();
    }
    return paths;
}
