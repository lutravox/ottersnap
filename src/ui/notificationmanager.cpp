#include <QTimer>
#include <QWidget>
#include "ui/notificationmanager.h"

NotificationManager::NotificationManager(QWidget *targetWindow, QObject *parent)
    : QObject(parent), m_targetWindow(targetWindow) {
    if (m_targetWindow) {
        m_targetWindow->installEventFilter(this);
    }
}

NotificationManager::~NotificationManager() {
    qDeleteAll(m_notifications);
}

void NotificationManager::notify(const QString& message, int timeoutMs) {
    int timeout = (timeoutMs == -1) ? 3500 : timeoutMs;

    Notification *n = new Notification(m_targetWindow->windowHandle());
    n->notify(message, timeout);
    m_notifications.append(n);

    connect(n, &Notification::finished, this, [this, n]() { onNotificationFinished(n); });

    updatePositions();
}

void NotificationManager::onNotificationFinished(Notification *notification) {
    if (!notification)
        return;

    m_notifications.removeAll(notification);
    notification->deleteLater();
    updatePositions();
}

bool NotificationManager::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_targetWindow &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Move)) {
        updatePositions();
    }
    return QObject::eventFilter(watched, event);
}

void NotificationManager::updatePositions() {
    if (m_notifications.isEmpty() || !m_targetWindow)
        return;

    QWidget *mainWindow = m_targetWindow->window();
    if (!mainWindow)
        return;

    int marginX = 20;
    int marginY = 60;
    int spacing = 10;

    QPoint globalPos = mainWindow->mapToGlobal(QPoint(mainWindow->width(), mainWindow->height()));
    int    base_x = globalPos.x() - marginX;
    int    base_y = globalPos.y() - marginY;

    for (int i = 0; i < m_notifications.size(); ++i) {
        Notification *n = m_notifications[i];
        if (!n)
            continue;

        int x = base_x - n->width();
        int offset = (m_notifications.size() - 1 - i) * (n->height() + spacing);
        int y = base_y - n->height() - offset;

        n->setPosition(QPoint(x, y));
    }
}
