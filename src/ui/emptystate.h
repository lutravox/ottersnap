#pragma once

#include <QWidget>

class QQuickView;
class NotificationModel;

/// @brief No images opened state.
class EmptyState : public QWidget {
    Q_OBJECT

  public:
    /// @brief Construct the main menu widget.
    /// @param parent Optional parent widget.
    /// @param notificationModel The notification model hosting the toast
    ///        layer for this state.
    explicit EmptyState(QWidget *parent = nullptr,
                        NotificationModel *notificationModel = nullptr);

    /// @brief Destructor. Cleans up the embedded QML view.
    ~EmptyState() override;

  signals:
    /// @brief Signal emitted when opening an image is requested.
    void openRequested();

  protected:
    /// @brief Keeps the toast layer sized to the widget.
    void resizeEvent(QResizeEvent *event) override;

  private:
    QQuickView *m_notificationView = nullptr;
    QWidget    *m_notificationContainer = nullptr;
};
