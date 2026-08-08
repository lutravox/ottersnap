#pragma once

#include <QColor>
#include <QQuickView>

/// @brief Overlay that displays color information (swatch and hex value).
class ColorInfo : public QQuickView {
    Q_OBJECT

  public:
    /// @brief Construct the color info overlay.
    /// @param anchor Optional widget to position relative to.
    explicit ColorInfo(QWidget *anchor = nullptr);

    /// @brief Update the displayed color.
    /// @param color The color to display.
    void setPickedColor(const QColor& color);

    /// @brief Set the visibility of the overlay.
    /// @param visible True to show, false to hide.
    void setVisibleState(bool visible);

    /// @brief Copy text to the system clipboard.
    /// @param text The text to copy.
    Q_INVOKABLE void copyToClipboard(const QString& text);

  private slots:
    void onFadeOutFinished();

  private:
    void updatePosition();

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    QWidget *m_parentWidget;
};
