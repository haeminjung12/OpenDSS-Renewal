/*
This is a UI file (.ui.qml) intended for Qt Design Studio editing.
*/
import QtQuick
import QtQuick.Controls.Basic
import Desktop_app_v2

TextArea {
    id: root

    implicitHeight: Constants.appStandardControlHeight * 3
    leftPadding: Constants.appControlHorizontalPadding
    rightPadding: Constants.appControlHorizontalPadding
    topPadding: Constants.spacing
    bottomPadding: Constants.spacing
    color: root.enabled ? Constants.appPrimaryTextColor : Constants.appDisabledTextColor
    placeholderTextColor: Constants.appSecondaryTextColor
    selectionColor: Constants.appFocusColor
    selectedTextColor: Constants.appInverseTextColor
    font: Constants.appBodyFont
    focusPolicy: Qt.StrongFocus
    selectByMouse: true
    wrapMode: TextEdit.Wrap

    background: Rectangle {
        radius: Constants.appControlRadius
        color: root.enabled ? Constants.appSurfaceColor : Constants.appDisabledColor
        border.color: root.activeFocus ? Constants.appFocusColor : Constants.appBorderDefaultColor
        border.width: root.activeFocus ? 2 : 1
    }
}
