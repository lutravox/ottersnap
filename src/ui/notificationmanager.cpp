#include <QCoreApplication>
#include <QQuickItem>
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
    int timeout = (timeoutMs == -1) ? c_defaultTimeoutMs : timeoutMs;

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
    int spacing = 5;

    QPoint globalPos = mainWindow->mapToGlobal(QPoint(mainWindow->width(), mainWindow->height()));
    int    base_x = globalPos.x() - marginX;
    int    base_y = globalPos.y() - marginY;

    int current_y = base_y;
    for (int i = m_notifications.size() - 1; i >= 0; --i) {
        Notification *n = m_notifications[i];
        if (!n)
            continue;

        // Use root object dimensions for precise positioning
        int w = n->width();
        int h = n->height();
        if (n->rootObject()) {
            w = n->rootObject()->width();
            h = n->rootObject()->height();
        }

        int x = base_x - w;
        int y = current_y - h;

        n->setPosition(QPoint(x, y));
        current_y -= (h + spacing);
    }
}
