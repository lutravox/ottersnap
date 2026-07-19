#pragma once

#include <QLabel>
#include <QPropertyAnimation>
#include <QTimer>

/// @brief Status bar that shows transient messages with a fade-out animation.
class NotificationBar : public QLabel {
    Q_OBJECT

  public:
    /// @brief Construct the notification bar.
    /// @param parent Optional parent widget.
    explicit NotificationBar(QWidget *parent = nullptr);

    /// @brief Display a message that fades out after a timeout.
    /// @param message Text to display.
    /// @param timeoutMs Time in milliseconds before fading out (default 3500).
    void notify(const QString& message, int timeoutMs = 3500);

  private slots:
    void hideAfterTimeout();
    void onFadeOutFinished();

  private:
    QPropertyAnimation *m_fadeAnim;
    QTimer             *m_hideTimer;
};
