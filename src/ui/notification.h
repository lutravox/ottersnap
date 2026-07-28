#pragma once

#include <QQuickView>
#include <QTimer>

/// @brief Notification that shows transient messages with a fade-out animation.
class Notification : public QQuickView {
    Q_OBJECT

  public:
    /// @brief Construct the notification.
    /// @param parent Optional parent window.
    explicit Notification(QWindow *parent = nullptr);
    ~Notification();

    /// @brief Display a message that fades out after a timeout.
    /// @param message Text to display.
    /// @param timeoutMs Time in milliseconds before fading out (default 3500).
    void notify(const QString& message, int timeoutMs = 3500);

  signals:
    void finished();

  private slots:
    void onFadeOutFinished();

  private:
    void hideAfterTimeout();
    void updatePosition();

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    QTimer  *m_hideTimer;
    QWindow *m_parent;
};
