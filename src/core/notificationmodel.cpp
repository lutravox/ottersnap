#include "core/notificationmodel.h"

#include <QDebug>
#include <QTimer>

NotificationModel::NotificationModel(QObject *parent) : QAbstractListModel(parent) {
}

int NotificationModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return m_items.size();
}

QVariant NotificationModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};

    const Item& item = m_items.at(index.row());
    switch (role) {
        case IdRole:
            return item.id;
        case MessageRole:
            return item.message;
        case FadeOutRole:
            return item.fadeOut;
        default:
            return {};
    }
}

QHash<int, QByteArray> NotificationModel::roleNames() const {
    return {
        {IdRole, "id"},
        {MessageRole, "message"},
        {FadeOutRole, "fadeOut"},
    };
}

void NotificationModel::addMessage(const QString& message, int timeoutMs) {
    qDebug() << "[Notify] addMessage:" << message;
    int id = m_nextId++;

    beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
    m_items.append(Item{id, message, false});
    endInsertRows();

    int     timeout = (timeoutMs == -1) ? c_defaultTimeoutMs : timeoutMs;
    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);
    m_timers.insert(id, timer);
    connect(timer, &QTimer::timeout, this, [this, id]() { onTimerFired(id); });
    timer->start(timeout);
}

void NotificationModel::onTimerFired(int id) {
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == id) {
            m_items[i].fadeOut = true;
            QModelIndex idx = index(i);
            emit        dataChanged(idx, idx, {FadeOutRole});
            break;
        }
    }
}

void NotificationModel::removeItem(int id) {
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == id) {
            beginRemoveRows(QModelIndex(), i, i);
            m_items.removeAt(i);
            endRemoveRows();
            break;
        }
    }

    if (QTimer *timer = m_timers.take(id)) {
        timer->stop();
        timer->deleteLater();
    }
}
