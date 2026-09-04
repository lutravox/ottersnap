import QtQuick
import "qrc:/ui/qml"

Item {
    // The viewer is rendered by a C++ QQuickRhiItem added to this item (z = -1).

    // Cluster indicator
    Rectangle {
        width: indicator.diameter
        height: indicator.diameter
        radius: indicator.diameter / 2
        x: indicator.position.x - indicator.diameter / 2
        y: indicator.position.y - indicator.diameter / 2
        color: indicator.color
        border.color: "white"
        border.width: indicator.diameter * 0.15
        antialiasing: true
        opacity: indicator.visible ? 1.0 : 0
        z: 10
    }

    // Color info overlay (lower-left)
    ColorInfo {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 20
        anchors.bottomMargin: 20
        z: 100
    }

    // Toast notifications (lower-right)
    NotificationLayer {
        anchors.fill: parent
        z: 200
    }
}
