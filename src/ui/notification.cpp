#include "ui/notification.h"

#include <QEvent>
#include <QFile>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>
#include <QTimer>

Notification::Notification(QWidget *parent)
    : QLabel(parent), m_hideTimer(nullptr), m_parent(parent) {
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setWordWrap(true);
    setFixedWidth(300);
    setAlignment(Qt::AlignCenter);
    setObjectName("notification");

    {
        QFile qss(":/qss/notification.qss");
        if (qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
            setStyleSheet(QString::fromUtf8(qss.readAll()));
        }
    }

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, &Notification::hideAfterTimeout);

    if (m_parent) {
        QWidget *mainWindow = m_parent->window();
        if (mainWindow) {
            mainWindow->installEventFilter(this);
        }
    }

    hide();
}

void Notification::notify(const QString& message, int timeoutMs) {
    setText(message);
    adjustSize();

    show();
    raise();
    updatePosition();
    m_hideTimer->start(timeoutMs);
}

void Notification::updatePosition() {
    if (!isVisible())
        return;

    if (m_parent) {
        QWidget *mainWindow = m_parent->window();
        if (mainWindow) {
            int marginX = 20;
            int marginY = 60;

            QRect geom = mainWindow->geometry();
            int   x = geom.x() + geom.width() - width() - marginX;
            int   y = geom.y() + geom.height() - height() - marginY;

            move(x, y);
        }
    }
}

void Notification::paintEvent(QPaintEvent *event) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QLabel::paintEvent(event);
}

void Notification::hideAfterTimeout() {
    hide();
    emit finished();
}

bool Notification::eventFilter(QObject *watched, QEvent *event) {
    return QLabel::eventFilter(watched, event);
}
