/*
This is a UI file (.ui.qml) intended for Qt Design Studio editing.
*/
import QtQuick
import QtQuick.Controls.Basic
import Desktop_app_v2

Switch {
    id: root

    implicitHeight: Constants.appStandardControlHeight
    spacing: Constants.spacing
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    font: Constants.appBodyFont

    indicator: Rectangle {
        implicitWidth: Math.round(Constants.appStandardControlHeight * 1.25)
        implicitHeight: Math.round(Constants.appStandardControlHeight * 0.625)
        x: root.leftPadding
        y: Math.round((root.height - height) / 2)
        radius: height / 2
        color: !root.enabled ? Constants.appDisabledColor
                             : root.checked ? Constants.appPrimaryColor
                                            : root.hovered ? Constants.appHoverColor : Constants.appSurfaceColor
        border.color: root.activeFocus ? Constants.appFocusColor : Constants.appBorderDefaultColor
        border.width: root.activeFocus ? 2 : 1

        Rectangle {
            width: parent.height - 6
            height: width
            x: 3 + root.visualPosition * (parent.width - width - 6)
            anchors.verticalCenter: parent.verticalCenter
            radius: width / 2
            color: root.enabled ? (root.checked ? Constants.appInverseTextColor : Constants.appSecondaryTextColor)
                                : Constants.appDisabledTextColor
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
