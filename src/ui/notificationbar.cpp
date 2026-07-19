#include "ui/notificationbar.h"

#include <QFile>

NotificationBar::NotificationBar(QWidget *parent)
    : QLabel(parent), m_fadeAnim(nullptr), m_hideTimer(nullptr) {
    setFixedHeight(32);
    setAlignment(Qt::AlignCenter);
    setObjectName("notificationBar");

    {
        QFile qss(":/qss/notificationbar.qss");
        if (qss.open(QIODevice::ReadOnly | QIODevice::Text))
            setStyleSheet(QString::fromUtf8(qss.readAll()));
    }

    m_fadeAnim = new QPropertyAnimation(this, "windowOpacity", this);
    m_fadeAnim->setDuration(200);

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, &NotificationBar::hideAfterTimeout);

    hide();
}

void NotificationBar::notify(const QString& message, int timeoutMs) {
    setText(message);
    show();
    setWindowOpacity(0);

    m_fadeAnim->setStartValue(0.0);
    m_fadeAnim->setEndValue(1.0);
    m_fadeAnim->start();

    m_hideTimer->start(timeoutMs);
}

void NotificationBar::hideAfterTimeout() {
    m_fadeAnim->setStartValue(1.0);
    m_fadeAnim->setEndValue(0.0);
    connect(m_fadeAnim,
            &QPropertyAnimation::finished,
            this,
            &NotificationBar::onFadeOutFinished,
            Qt::UniqueConnection);
    m_fadeAnim->start();
}

void NotificationBar::onFadeOutFinished() {
    hide();
    setWindowOpacity(1.0);
}
