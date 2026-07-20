#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QTimer>

/// @brief Monitors a file for changes and ensures it is stable before signaling.
class ImageMonitor : public QObject {
    Q_OBJECT
  public:
    explicit ImageMonitor(QObject *parent = nullptr);
    ~ImageMonitor();

    /// @brief Start monitoring the specified file.
    void watch(const QString& filePath);
    /// @brief Stop monitoring the current file.
    void stop();

  signals:
    /// @brief Emitted when the file has changed and is stable.
    void fileChanged(const QString& filePath);

  private slots:
    void onFileChanged();
    void checkStability();

  private:
    QFileSystemWatcher *m_watcher;
    QTimer             *m_debounceTimer;
    QString             m_filePath;
    qint64              m_lastSize = -1;
};
