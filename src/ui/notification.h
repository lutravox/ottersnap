#pragma once

#include <QLabel>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>

/// @brief Notification that shows transient messages with a fade-out animation.
class Notification : public QLabel {
    Q_OBJECT

  public:
    /// @brief Construct the notification.
    /// @param parent Optional parent widget.
    explicit Notification(QWidget *parent = nullptr);

    /// @brief Display a message that fades out after a timeout.
    /// @param message Text to display.
    /// @param timeoutMs Time in milliseconds before fading out (default 3500).
    void notify(const QString& message, int timeoutMs = 3500);

  signals:
    void finished();

  private:
    void hideAfterTimeout();
    void paintEvent(QPaintEvent *event) override;

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    void     updatePosition();
    QTimer  *m_hideTimer;
    QWidget *m_parent;
};
