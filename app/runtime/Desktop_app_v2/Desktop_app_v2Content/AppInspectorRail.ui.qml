/*
This is a UI file (.ui.qml) intended for Qt Design Studio editing.
*/
import QtQuick
import QtQuick.Controls.Basic
import Desktop_app_v2

Button {
    id: root

    width: 28
    height: 36
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    background: Rectangle {
        color: root.hovered ? Constants.appHoverColor : Constants.appSubtleSurfaceColor
        border.color: root.activeFocus ? Constants.appFocusColor : Constants.appBorderDefaultColor
        border.width: root.activeFocus ? 2 : 1
    }

    contentItem: Text {
        text: root.text
        color: root.enabled ? Constants.appPrimaryTextColor : Constants.appDisabledTextColor
        font: Constants.appInspectorRailGlyphFont
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
