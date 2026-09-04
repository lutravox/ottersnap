import QtQuick
import "qrc:/ui/qml"

// Always-present notification layer. Stacks toasts in the lower-right corner.
Item {
    Column {
        id: stack
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 20
        anchors.bottomMargin: 20
        spacing: 5

        Repeater {
            model: notificationModel
            delegate: Notification {
                message: model.message
                fadeOut: model.fadeOut
                onFadeOutFinished: notificationModel.removeItem(model.id)
            }
        }
    }
}
