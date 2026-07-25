/*
This is a UI file (.ui.qml) intended for Qt Design Studio editing.
*/
import QtQuick
import QtQuick.Controls.Basic
import Desktop_app_v2

Button {
    id: root

    property string visualRole: "secondary"
    property color identityColor: Constants.appPrimaryColor

    implicitHeight: root.visualRole === "primary" ? Constants.appPrimaryButtonHeight : Constants.appStandardControlHeight
    leftPadding: Constants.appControlHorizontalPadding
    rightPadding: Constants.appControlHorizontalPadding
    topPadding: 4
    bottomPadding: 4
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    font: Constants.appButtonFont

    background: Rectangle {
        radius: Constants.appControlRadius
        color: !root.enabled ? Constants.appDisabledColor
                             : root.visualRole === "destructive" ? Constants.appErrorColor
                             : root.visualRole === "identity" ? root.identityColor
                             : root.visualRole === "primary" ? (root.down ? Constants.appPrimaryPressedColor
                                                                         : root.hovered ? Constants.appPrimaryHoverColor
                                                                                        : Constants.appPrimaryColor)
                                                               : root.down || root.hovered ? Constants.appHoverColor
                                                                                           : Constants.appSurfaceColor
        border.color: root.activeFocus ? Constants.appFocusColor
                                       : root.visualRole === "primary" || root.visualRole === "destructive" || root.visualRole === "identity"
                                         ? color : Constants.appBorderDefaultColor
        border.width: root.activeFocus ? 2 : 1
    }

    contentItem: Text {
        text: root.text
        color: !root.enabled ? Constants.appDisabledTextColor
                             : root.visualRole === "primary" || root.visualRole === "destructive" || root.visualRole === "identity"
                               ? Constants.appInverseTextColor : Constants.appPrimaryTextColor
        font: root.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
