#include "ui/notification.h"

#include <QEvent>
#include <QGuiApplication>
#include <QPalette>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickItem>
#include <QSurfaceFormat>
#include <QTimer>
#include <QWidget>

Notification::Notification(QWindow *parent)
    : QQuickView(parent), m_hideTimer(nullptr), m_parent(parent) {
    QSurfaceFormat format = this->format();
    format.setAlphaBufferSize(8);
    setFormat(format);

    setColor(Qt::transparent);

    setSource(QUrl("qrc:/ui/qml/notification.qml"));
    setFlags(Qt::ToolTip | Qt::FramelessWindowHint);

    connect(rootObject(), SIGNAL(fadeOutFinished()), this, SLOT(onFadeOutFinished()));

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, &Notification::hideAfterTimeout);

    if (m_parent) {
        m_parent->installEventFilter(this);
    }

    hide();
}

Notification::~Notification() {
}

void Notification::notify(const QString& message, int timeoutMs) {
    if (rootObject()) {
        rootObject()->setProperty("message", message);
        rootObject()->setProperty("fadeOut", false);
    }

    show();
    raise();
    updatePosition();
    m_hideTimer->start(timeoutMs);
}

void Notification::updatePosition() {
    if (!isVisible())
        return;

    if (m_parent) {
        int marginX = 20;
        int marginY = 60;

        QPoint globalPos = m_parent->mapToGlobal(QPoint(m_parent->width(), m_parent->height()));
        int    x = globalPos.x() - width() - marginX;
        int    y = globalPos.y() - height() - marginY;

        setPosition(QPoint(x, y));
    }
}

void Notification::hideAfterTimeout() {
    if (rootObject()) {
        rootObject()->setProperty("fadeOut", true);
    }
}

void Notification::onFadeOutFinished() {
    hide();
    emit finished();
}

bool Notification::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_parent && (event->type() == QEvent::Resize || event->type() == QEvent::Move)) {
        updatePosition();
    }
    return QQuickView::eventFilter(watched, event);
}
