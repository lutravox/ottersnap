#include <QDebug>
#include <QFile>
#include "core/imagemonitor.h"

ImageMonitor::ImageMonitor(QObject *parent)
    : QObject(parent), m_watcher(new QFileSystemWatcher(this)), m_debounceTimer(new QTimer(this)) {
    m_debounceTimer->setSingleShot(true);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &ImageMonitor::onFileChanged);
    connect(m_debounceTimer, &QTimer::timeout, this, &ImageMonitor::checkStability);
}

ImageMonitor::~ImageMonitor() = default;

void ImageMonitor::watch(const QString& filePath) {
    if (!m_filePath.isEmpty()) {
        m_watcher->removePath(m_filePath);
    }
    m_filePath = filePath;
    m_watcher->addPath(m_filePath);
}

void ImageMonitor::stop() {
    if (!m_filePath.isEmpty()) {
        m_watcher->removePath(m_filePath);
    }
    m_filePath.clear();
}

void ImageMonitor::onFileChanged() {
    // Re-add watcher — Linux drops the path after the event fires.
    if (!m_filePath.isEmpty()) {
        m_watcher->addPath(m_filePath);
    }
    m_debounceTimer->start(200);
}

void ImageMonitor::checkStability() {
    QFile f(m_filePath);
    if (!f.exists())
        return;

    qint64 size = f.size();
    if (size <= 0)
        return;

    if (size == m_lastSize) {
        emit fileChanged(m_filePath);
    } else {
        m_lastSize = size;
        m_debounceTimer->start(200); // Check again after delay
    }
}
