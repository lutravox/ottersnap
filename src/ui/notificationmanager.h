#pragma once

#include <QEvent>
#include <QList>
#include <QObject>
#include <QWindow>
#include "ui/notification.h"

/// @brief Manages a stack of transient notifications overlaying a target window.
class NotificationManager : public QObject {
    Q_OBJECT

  public:
    static constexpr int c_defaultTimeoutMs = 5000;

    /// @brief Construct the manager.
    /// @param targetWindow The window that notifications should be positioned relative to.
    explicit NotificationManager(QWidget *targetWindow, QObject *parent = nullptr);
    ~NotificationManager() override;

    /// @brief Displays a new notification message.
    void notify(const QString& message, int timeoutMs = c_defaultTimeoutMs);

  private slots:
    void onNotificationFinished(Notification *notification);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    void updatePositions();

    QWidget              *m_targetWindow;
    QList<Notification *> m_notifications;
};
