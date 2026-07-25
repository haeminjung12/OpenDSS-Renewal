/*
This is a UI file (.ui.qml) intended for Qt Design Studio editing.
*/
import QtQuick
import QtQuick.Controls.Basic
import Desktop_app_v2

CheckBox {
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
        radius: Constants.appControlRadius
        color: !root.enabled ? Constants.appDisabledColor
                             : root.checkState !== Qt.Unchecked ? Constants.appPrimaryColor
                                                               : root.hovered ? Constants.appHoverColor : Constants.appSurfaceColor
        border.color: root.activeFocus ? Constants.appFocusColor : Constants.appBorderDefaultColor
        border.width: root.activeFocus ? 2 : 1

        Text {
            anchors.centerIn: parent
            text: root.checkState === Qt.PartiallyChecked ? "−" : "✓"
            visible: root.checkState !== Qt.Unchecked
            color: root.enabled ? Constants.appInverseTextColor : Constants.appDisabledTextColor
            font: Constants.appButtonFont
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
