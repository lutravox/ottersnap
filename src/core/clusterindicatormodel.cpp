#include "core/clusterindicatormodel.h"

ClusterIndicatorModel::ClusterIndicatorModel(QObject *parent) : QObject(parent) {
}

void ClusterIndicatorModel::setPosition(const QPointF& pos) {
    if (m_position != pos) {
        m_position = pos;
        emit positionChanged();
    }
}

void ClusterIndicatorModel::setColor(const QColor& color) {
    if (m_color != color) {
        m_color = color;
        emit colorChanged();
    }
}

void ClusterIndicatorModel::setVisible(bool visible) {
    if (m_visible != visible) {
        m_visible = visible;
        emit visibleChanged();
    }
}

void ClusterIndicatorModel::setDiameter(qreal diameter) {
    if (m_diameter != diameter) {
        m_diameter = diameter;
        emit diameterChanged();
    }
}
