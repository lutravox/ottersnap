import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root
    width: 400
    height: pill.height + 4

    property string message: ""
    property bool fadeOut: false
    signal fadeOutFinished()

    SystemPalette {
        id: sysPalette
        colorGroup: SystemPalette.Active
    }

    Rectangle {
        id: pill
        width: 380
        height: messageText.implicitHeight + 20
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 2

        // Initial opacity for fade-in
        opacity: 0

        color: sysPalette.window
        radius: 8
        border.color: sysPalette.midlight
        border.width: 1

        Text {
            id: messageText
            anchors.centerIn: parent
            text: "Notification"
            color: sysPalette.windowText
            font.pixelSize: 14
            wrapMode: Text.WordWrap
            width: parent.width - 16
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // Fade-in animation
    NumberAnimation {
        id: fadeInAnim
        target: pill
        property: "opacity"
        from: 0
        to: 1
        duration: 300
        easing.type: Easing.OutCubic
    }

    // Fade-out animation
    NumberAnimation {
        id: fadeOutAnim
        target: pill
        property: "opacity"
        from: 1
        to: 0
        duration: 300
        easing.type: Easing.InCubic
        onStopped: root.fadeOutFinished()
    }

    // Trigger animations based on fadeOut property
    onFadeOutChanged: {
        if (fadeOut) {
            fadeOutAnim.start();
        } else {
            fadeInAnim.start();
        }
    }

    Component.onCompleted: {
        fadeInAnim.start();
    }

    onMessageChanged: messageText.text = message
}
