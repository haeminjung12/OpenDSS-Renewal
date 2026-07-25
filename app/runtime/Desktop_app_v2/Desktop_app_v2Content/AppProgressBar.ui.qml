/*
This is a UI file (.ui.qml) intended for Qt Design Studio editing.
*/
import QtQuick
import QtQuick.Controls.Basic
import Desktop_app_v2

ProgressBar {
    id: root

    implicitHeight: Math.round(Constants.appStandardControlHeight * 0.25)

    background: Rectangle {
        implicitWidth: 160
        implicitHeight: root.implicitHeight
        radius: height / 2
        color: root.enabled ? Constants.appHoverColor : Constants.appDisabledColor
        border.color: Constants.appBorderSubtleColor
    }

    contentItem: Item {
        implicitWidth: 160
        implicitHeight: root.implicitHeight

        Rectangle {
            x: root.indeterminate ? Math.round(parent.width / 3) : 0
            width: root.indeterminate ? Math.round(parent.width / 3)
                                      : Math.round(root.visualPosition * parent.width)
            height: parent.height
            radius: height / 2
            color: root.enabled ? Constants.appPrimaryColor : Constants.appDisabledTextColor
        }
    }
}
