/*
This is a UI file (.ui.qml) intended for Qt Design Studio editing.
*/
import QtQuick
import QtQuick.Controls.Basic
import Desktop_app_v2

RadioButton {
    id: root

    implicitHeight: Constants.appStandardControlHeight
    spacing: Constants.spacing
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    font: Constants.appBodyFont

    indicator: Rectangle {
        implicitWidth: Math.round(Constants.appStandardControlHeight * 0.625)
        implicitHeight: implicitWidth
        x: root.leftPadding
        y: Math.round((root.height - height) / 2)
        radius: width / 2
        color: !root.enabled ? Constants.appDisabledColor
                             : root.hovered ? Constants.appHoverColor : Constants.appSurfaceColor
        border.color: !root.enabled ? Constants.appBorderSubtleColor
                                    : root.activeFocus || root.checked ? Constants.appFocusColor : Constants.appBorderDefaultColor
        border.width: root.activeFocus ? 2 : 1

        Rectangle {
            anchors.centerIn: parent
            width: Math.round(parent.width * 0.5)
            height: width
            radius: width / 2
            visible: root.checked
            color: root.enabled ? Constants.appPrimaryColor : Constants.appDisabledTextColor
        }
    }

    contentItem: Text {
        leftPadding: root.indicator.width + root.spacing
        text: root.text
        color: root.enabled ? Constants.appPrimaryTextColor : Constants.appDisabledTextColor
        font: root.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
