import QtQuick 2.15

// A single toast pill (a delegate in the notification stack).
Item {
    id: root
    width: 380
    height: messageText.implicitHeight + 20

    property string message: ""
    property bool fadeOut: false
    signal fadeOutFinished()

    SystemPalette {
        id: sysPalette
        colorGroup: SystemPalette.Active
    }

    Rectangle {
        id: pill
        anchors.fill: parent
        opacity: 0
        color: sysPalette.window
        radius: 8
        border.color: sysPalette.midlight
        border.width: 1

        Text {
            id: messageText
            anchors.centerIn: parent
            text: message
            color: sysPalette.windowText
            font.pixelSize: 14
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            width: parent.width - 16
            horizontalAlignment: Text.AlignHCenter
        }
    }

    NumberAnimation {
        id: fadeInAnim
        target: pill
        property: "opacity"
        from: 0
        to: 1
        duration: 300
        easing.type: Easing.OutCubic
    }

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

    onFadeOutChanged: {
        if (fadeOut)
            fadeOutAnim.start();
        else
            fadeInAnim.start();
    }

    Component.onCompleted: fadeInAnim.start()
}
