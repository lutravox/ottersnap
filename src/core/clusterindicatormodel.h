#pragma once

#include <QColor>
#include <QObject>
#include <QPointF>

/// @brief Reactive state for the cluster indicator, exposed to QML.
class ClusterIndicatorModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QPointF position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(qreal diameter READ diameter WRITE setDiameter NOTIFY diameterChanged)

  public:
    /// @brief Construct the model.
    /// @param parent Optional parent object (the owning viewer's QQuickView).
    explicit ClusterIndicatorModel(QObject *parent = nullptr);

    /// @brief Get the indicator center in viewer coordinates (top-left origin).
    /// @return The center position.
    QPointF position() const {
        return m_position;
    }

    /// @brief Set the indicator center in viewer coordinates.
    /// @param pos The new center position.
    void setPosition(const QPointF& pos);

    /// @brief Get the indicator (cluster) color.
    /// @return The color to render the disc in.
    QColor color() const {
        return m_color;
    }

    /// @brief Set the indicator (cluster) color.
    /// @param color The color to render the disc in.
    void setColor(const QColor& color);

    /// @brief Get whether the indicator should be shown.
    /// @return True if the indicator is visible.
    bool visible() const {
        return m_visible;
    }

    /// @brief Set whether the indicator should be shown.
    /// @param visible True to show, false to hide.
    void setVisible(bool visible);

    /// @brief Get the indicator diameter in viewer pixels.
    /// @return The diameter.
    qreal diameter() const {
        return m_diameter;
    }

    /// @brief Set the indicator diameter in viewer pixels.
    /// @param diameter The new diameter.
    void setDiameter(qreal diameter);

  signals:
    /// @brief Emitted when the indicator center changes.
    void positionChanged();

    /// @brief Emitted when the indicator color changes.
    void colorChanged();

    /// @brief Emitted when the indicator visibility changes.
    void visibleChanged();

    /// @brief Emitted when the indicator diameter changes.
    void diameterChanged();

  private:
    QPointF m_position;
    QColor  m_color;
    bool    m_visible = false;
    qreal   m_diameter = 25.0;
};
