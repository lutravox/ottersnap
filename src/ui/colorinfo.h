#pragma once

#include <QColor>
#include <QQuickImageProvider>
#include <QQuickView>
#include "core/coloranalyzer.h"

/// @brief Overlay that displays color information (swatch and hex value).
class ColorInfo : public QQuickView {
    Q_OBJECT

  signals:
    void clusterSelected(const QVariantMap& clusterData);

  public:
    /// @param anchor Optional widget to position relative to.
    explicit ColorInfo(QWidget *anchor = nullptr);

    /// @brief Update the displayed color.
    /// @param color The color to display.
    void setPickedColor(const QColor& color);

    /// @brief Set the visibility of the overlay.
    /// @param visible True to show, false to hide.
    void setVisibleState(bool visible);

    /// @brief Update the color clusters in the overlay.
    /// @param clusters The list of clusters representing the distribution.
    void setClusters(const QList<ColorAnalyzer::ColorCluster>& clusters);

    /// @brief Reset the current selection in the overlay.
    void resetSelection();

    /// @brief Copy text to the system clipboard.
    /// @param text The text to copy.
    Q_INVOKABLE void copyToClipboard(const QString& text);

    /// @brief Handle cluster selection from the UI.
    Q_INVOKABLE void handleClusterSelected(const QVariantMap& clusterData);

  private slots:
    void onFadeOutFinished();

  private:
    void updatePosition();

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    QWidget *m_parentWidget;
};
