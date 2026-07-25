/*
This is a UI file (.ui.qml) intended for Qt Design Studio editing.
*/
import QtQuick
import QtQuick.Controls.Basic
import Desktop_app_v2

SpinBox {
    id: root

    implicitHeight: Constants.appStandardControlHeight
    font: Constants.appBodyFont
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true

    contentItem: TextInput {
        text: root.displayText
        color: root.enabled ? Constants.appPrimaryTextColor : Constants.appDisabledTextColor
        selectionColor: Constants.appFocusColor
        selectedTextColor: Constants.appInverseTextColor
        font: root.font
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        leftPadding: Constants.appStandardControlHeight
        rightPadding: Constants.appStandardControlHeight
        readOnly: !root.editable
        validator: root.validator
        inputMethodHints: root.inputMethodHints
        selectByMouse: true
    }

    down.indicator: Rectangle {
        x: 0
        height: root.height
        implicitWidth: Constants.appStandardControlHeight
        color: !root.enabled ? Constants.appDisabledColor
                             : root.down.pressed || root.down.hovered ? Constants.appHoverColor : Constants.appSurfaceColor
        border.color: root.activeFocus ? Constants.appFocusColor : Constants.appBorderSubtleColor

        Text {
            anchors.centerIn: parent
            text: "−"
            color: root.enabled ? Constants.appPrimaryTextColor : Constants.appDisabledTextColor
            font: Constants.appButtonFont
        }
    }

    up.indicator: Rectangle {
        x: root.width - width
        height: root.height
        implicitWidth: Constants.appStandardControlHeight
        color: !root.enabled ? Constants.appDisabledColor
                             : root.up.pressed || root.up.hovered ? Constants.appHoverColor : Constants.appSurfaceColor
        border.color: root.activeFocus ? Constants.appFocusColor : Constants.appBorderSubtleColor

        Text {
            anchors.centerIn: parent
            text: "+"
            color: root.enabled ? Constants.appPrimaryTextColor : Constants.appDisabledTextColor
            font: Constants.appButtonFont
        }
    }

    background: Rectangle {
        radius: Constants.appControlRadius
        color: root.enabled ? Constants.appSurfaceColor : Constants.appDisabledColor
        border.color: root.activeFocus ? Constants.appFocusColor : Constants.appBorderDefaultColor
        border.width: root.activeFocus ? 2 : 1
    }
}
