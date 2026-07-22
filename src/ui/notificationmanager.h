#pragma once

#include <QEvent>
#include <QList>
#include <QObject>
#include <QWidget>
#include "ui/notification.h"

/// @brief Manages a stack of transient notifications overlaying a target window.
class NotificationManager : public QObject {
    Q_OBJECT

  public:
    /// @brief Construct the manager.
    /// @param targetWindow The window that notifications should be positioned relative to.
    explicit NotificationManager(QWidget *targetWindow, QObject *parent = nullptr);
    ~NotificationManager() override;

    /// @brief Displays a new notification message.
    void notify(const QString& message, int timeoutMs = 3500);

  private slots:
    void onNotificationFinished(Notification *notification);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    void updatePositions();

    QWidget              *m_targetWindow;
    QList<Notification *> m_notifications;
};
