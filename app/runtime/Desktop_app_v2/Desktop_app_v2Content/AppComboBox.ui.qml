pragma ComponentBehavior: Bound

/*
This is a UI file (.ui.qml) intended for Qt Design Studio editing.
*/
import QtQuick
import QtQuick.Controls.Basic
import Desktop_app_v2

ComboBox {
    id: root

    implicitHeight: Constants.appStandardControlHeight
    leftPadding: Constants.appControlHorizontalPadding
    rightPadding: Constants.appControlHorizontalPadding
    font: Constants.appBodyFont
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    delegate: ItemDelegate {
        id: defaultOptionDelegate
        required property int index
        required property string modelData

        width: root.width
        height: Constants.appStandardControlHeight
        leftPadding: Constants.appControlHorizontalPadding
        rightPadding: Constants.appControlHorizontalPadding
        highlighted: root.highlightedIndex === index
        hoverEnabled: true

        contentItem: Text {
            text: defaultOptionDelegate.modelData
            color: defaultOptionDelegate.enabled ? Constants.appPrimaryTextColor : Constants.appDisabledTextColor
            font: Constants.appBodyFont
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            color: defaultOptionDelegate.index === root.currentIndex || defaultOptionDelegate.hovered || defaultOptionDelegate.highlighted
                   ? Constants.appHoverColor : Constants.appSurfaceColor
            border.color: Constants.appBorderSubtleColor
        }
    }

    background: Rectangle {
        radius: Constants.appControlRadius
        color: root.enabled ? (root.hovered ? Constants.appHoverColor : Constants.appSurfaceColor)
                            : Constants.appDisabledColor
        border.color: root.activeFocus ? Constants.appFocusColor : Constants.appBorderDefaultColor
        border.width: root.activeFocus ? 2 : 1
    }
}
