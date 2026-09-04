#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QList>

class QTimer;

/// @brief Reactive list model of active toast notifications.
///
/// Each row is one notification (id, message, fade-out flag).
class NotificationModel : public QAbstractListModel {
    Q_OBJECT

  public:
    /// @brief Auto fade-out delay used when a notification is added without
    ///        an explicit timeout.
    static constexpr int c_defaultTimeoutMs = 5000;

    /// @brief QML roles exposed per notification row.
    enum Roles {
        IdRole = Qt::UserRole + 1, ///< Unique notification id (int).
        MessageRole,               ///< Message text (QString).
        FadeOutRole,               ///< True once the fade-out should start (bool).
    };

    /// @brief Construct the model.
    /// @param parent Optional parent object (the owning MainWindow).
    explicit NotificationModel(QObject *parent = nullptr);

    int                    rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant               data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// @brief Append a notification that fades out after @p timeoutMs.
    void addMessage(const QString& message, int timeoutMs = c_defaultTimeoutMs);

    /// @brief Remove a notification by id (called from the overlay once its
    ///        fade-out finishes).
    Q_INVOKABLE void removeItem(int id);

  private:
    /// @brief Flip FadeOutRole for the row with @p id.
    void onTimerFired(int id);

    /// @brief One toast row.
    struct Item {
        int     id;      ///< Unique id, assigned on insertion.
        QString message; ///< Toast text.
        bool    fadeOut; ///< True once the fade-out animation should start.
    };

    QList<Item>          m_items;
    QHash<int, QTimer *> m_timers; ///< Active fade-out timers by notification id.
    int                  m_nextId = 0;
};
