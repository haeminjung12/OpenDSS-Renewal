/*
This is a UI file (.ui.qml) intended for Qt Design Studio editing.
*/
import QtQuick
import QtQuick.Controls.Basic
import Desktop_app_v2

TextField {
    id: root

    implicitHeight: Constants.appStandardControlHeight
    leftPadding: Constants.appControlHorizontalPadding
    rightPadding: Constants.appControlHorizontalPadding
    color: root.enabled ? Constants.appPrimaryTextColor : Constants.appDisabledTextColor
    placeholderTextColor: Constants.appSecondaryTextColor
    font: Constants.appBodyFont
    focusPolicy: Qt.StrongFocus
    selectByMouse: true

    background: Rectangle {
        radius: Constants.appControlRadius
        color: root.enabled ? Constants.appSurfaceColor : Constants.appDisabledColor
        border.color: root.activeFocus ? Constants.appFocusColor : Constants.appBorderDefaultColor
        border.width: root.activeFocus ? 2 : 1
    }
}
