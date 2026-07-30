/*
This is a UI file (.ui.qml) intended for Qt Design Studio editing.
*/
import QtQuick
import QtQuick.Controls.Basic
import Desktop_app_v2

Button {
    id: root

    property bool selected: false

    implicitHeight: Constants.appStandardControlHeight
    leftPadding: Constants.appControlHorizontalPadding
    rightPadding: Constants.appControlHorizontalPadding
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    font: Constants.appButtonFont

    background: Rectangle {
        radius: Constants.appControlRadius
        color: !root.enabled ? Constants.appDisabledColor
                             : root.selected || root.down || root.hovered ? Constants.appHoverColor
                                                                         : Constants.appSurfaceColor
        border.color: !root.enabled ? Constants.appBorderSubtleColor
                                    : root.activeFocus ? Constants.appFocusColor
                                                       : root.selected ? Constants.appPrimaryColor : Constants.appBorderSubtleColor
        border.width: root.activeFocus || root.selected ? 2 : 1

        Rectangle {
            width: 3
            height: parent.height - Constants.spacing
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            visible: root.selected
            color: Constants.appPrimaryColor
        }
    }

    contentItem: Text {
        text: root.text
        color: root.enabled ? Constants.appPrimaryTextColor : Constants.appDisabledTextColor
        font: root.font
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
