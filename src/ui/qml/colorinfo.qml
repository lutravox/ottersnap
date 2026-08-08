import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root
    width: overlayWidth
    height: overlayHeight

    property string hexColor: "#FFFFFF"
    property real alphaValue: 1.0
    property bool visibleState: false
    signal fadeOutFinished()

    // Layout constants
    readonly property int overlayWidth: 260
    readonly property int overlayHeight: 150
    readonly property int containerWidth: 250
    readonly property int containerHeight: 140
    readonly property int wheelSize: 80
    readonly property int wheelMarginLeft: 20
    readonly property int indicatorSize: 8
    readonly property int infoWidth: 140
    readonly property int infoHeight: 120
    readonly property int infoMarginRight: 10
    readonly property int wheelInfoGap: 20
    readonly property int infoSpacing: 10
    readonly property int fontSizeSmall: 11
    readonly property int fontSizeBold: 14

    function getRGB(hex, alpha) {
        return {
            r: parseInt(hex.slice(1, 3), 16).toString().padStart(3, '0'),
            g: parseInt(hex.slice(3, 5), 16).toString().padStart(3, '0'),
            b: parseInt(hex.slice(5, 7), 16).toString().padStart(3, '0'),
            a: Math.round(alpha * 255).toString().padStart(3, '0')
        };
    }

    function getHSV(hex) {
        var r = parseInt(hex.slice(1, 3), 16) / 255;
        var g = parseInt(hex.slice(3, 5), 16) / 255;
        var b = parseInt(hex.slice(5, 7), 16) / 255;

        var max = Math.max(r, g, b), min = Math.min(r, g, b);
        var h, s, v = max;
        var d = max - min;
        s = max === 0 ? 0 : d / max;

        if (max === min) {
            h = 0;
        } else {
            switch (max) {
                case r: h = (g - b) / d + (g < b ? 6 : 0); break;
                case g: h = 2 + (b - r) / d; break;
                case b: h = 4 + (r - g) / d; break;
            }
            h /= 6;
        }
        return {
            h: Math.round(h * 360).toString().padStart(3, '0'),
            s: Math.round(s * 100).toString().padStart(3, '0'),
            v: Math.round(v * 100).toString().padStart(3, '0')
        };
    }

    function getHSVForIndicator(hex) {
        var r = parseInt(hex.slice(1, 3), 16) / 255;
        var g = parseInt(hex.slice(3, 5), 16) / 255;
        var b = parseInt(hex.slice(5, 7), 16) / 255;

        var max = Math.max(r, g, b), min = Math.min(r, g, b);
        var h, s, v = max;
        var d = max - min;
        s = max === 0 ? 0 : d / max;

        if (max === min) {
            h = 0;
        } else {
            switch (max) {
                case r: h = (g - b) / d + (g < b ? 6 : 0); break;
                case g: h = 2 + (b - r) / d; break;
                case b: h = 4 + (r - g) / d; break;
            }
            h /= 6;
        }
        return { h: h, s: s, v: v };
    }

    property var currentHSVIndicator: getHSVForIndicator(hexColor)
    property var rgbValues: getRGB(hexColor, alphaValue)
    property var hsvValues: getHSV(hexColor)

    onHexColorChanged: {
        currentHSVIndicator = getHSVForIndicator(hexColor);
        rgbValues = getRGB(hexColor, alphaValue);
        hsvValues = getHSV(hexColor);
    }

    onAlphaValueChanged: {
        rgbValues = getRGB(hexColor, alphaValue);
    }

    SystemPalette {
        id: sysPalette
        colorGroup: SystemPalette.Active
    }

    Menu {
        id: copyMenu
        MenuItem {
            text: "Copy"
            onTriggered: {
                colorInfo.copyToClipboard(currentSelectedText);
            }
        }
    }

    property string currentSelectedText: ""

    Component {
        id: valueFieldComponent
        Row {
            spacing: 2
            Text {
                text: modelData.label; color: "white"; font.pixelSize: fontSizeSmall; font.family: "monospace"
            }
            TextEdit {
                id: te; text: modelData.value; color: "white"; font.pixelSize: fontSizeSmall; font.family: "monospace"; readOnly: true; selectByMouse: true; cursorVisible: false
                MouseArea {
                    anchors.fill: parent; acceptedButtons: Qt.RightButton;
                    onClicked: { root.currentSelectedText = te.text; copyMenu.popup(); }
                }
            }
        }
    }

    // Main container for the overlay
    Item {
        id: container
        anchors.centerIn: parent
        width: containerWidth
        height: containerHeight
        opacity: 0

        Behavior on opacity {
            NumberAnimation {
                duration: 200
                easing.type: Easing.OutCubic
            }
        }

        onOpacityChanged: {
            if (opacity <= 0 && !root.visibleState) {
                root.fadeOutFinished();
            }
        }

        // Background capsule
        Rectangle {
            anchors.fill: parent
            radius: 8
            color: Qt.rgba(0.1, 0.1, 0.1, 0.8)
            border.color: Qt.rgba(0.4, 0.4, 0.4, 0.5)
            border.width: 1
        }

        // The Color Wheel
        Rectangle {
            id: wheelContainer
            width: wheelSize
            height: wheelSize
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: wheelMarginLeft
            radius: wheelSize / 2
            color: "transparent"
            layer.enabled: true
            opacity: container.opacity

            ShaderEffect {
                id: wheelEffect
                anchors.fill: parent
                fragmentShader: "qrc:/shaders/colorwheel.frag.qsb"
            }

            // Indicator for the current color
            Rectangle {
                id: indicator
                width: indicatorSize
                height: indicatorSize
                radius: indicatorSize / 2
                color: "white"
                border.color: "black"
                border.width: 1

                x: (wheelSize / 2) + Math.cos((root.currentHSVIndicator.h - 0.5) * Math.PI * 2) * (root.currentHSVIndicator.s * (wheelSize / 2)) - (indicatorSize / 2)
                y: (wheelSize / 2) + Math.sin((root.currentHSVIndicator.h - 0.5) * Math.PI * 2) * (root.currentHSVIndicator.s * (wheelSize / 2)) - (indicatorSize / 2)
            }
        }

        // The info section
        Item {
            id: pill
            width: infoWidth
            height: infoHeight
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: wheelContainer.right
            anchors.leftMargin: wheelInfoGap

            Column {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: infoSpacing

                Row {
                    spacing: 20

                    Column {
                        spacing: 4
                        Repeater {
                            model: [
                                {label: "R: ", value: root.rgbValues.r},
                                {label: "G: ", value: root.rgbValues.g},
                                {label: "B: ", value: root.rgbValues.b},
                                {label: "A: ", value: root.rgbValues.a},
                            ]
                            delegate: valueFieldComponent
                        }
                    }

                    Column {
                        spacing: 4
                        Repeater {
                            model: [
                                {label: "H: ", value: root.hsvValues.h},
                                {label: "S: ", value: root.hsvValues.s},
                                {label: "V: ", value: root.hsvValues.v},
                            ]
                            delegate: valueFieldComponent
                        }
                    }
                }

                Item {
                    width: teHex.width; height: teHex.height
                    TextEdit {
                        id: teHex
                        text: root.hexColor
                        color: "white"
                        font.pixelSize: fontSizeBold
                        font.weight: Font.Bold
                        font.family: "monospace"
                        readOnly: true
                        selectByMouse: true
                        cursorVisible: false
                        MouseArea { anchors.fill: parent; acceptedButtons: Qt.RightButton; onClicked: { root.currentSelectedText = teHex.text; copyMenu.popup(); } }
                    }
                }
            }
        }
    }

    onVisibleStateChanged: {
        container.opacity = visibleState ? 1 : 0;
    }

    Component.onCompleted: {
        if (visibleState) {
            container.opacity = 1;
        }
    }
}
