/*
This is a UI file (.ui.qml) intended for Qt Design Studio editing.
*/
import QtQuick
import QtQuick.Controls.Basic
import Desktop_app_v2

Item {
    id: root

    default property alias content: bodyContent.data
    property string sectionTitle: ""
    property string bodyText: ""
    property bool expanded: false
    property bool headingEnabled: true
    property int bodyHeight: Constants.compactBodyHeight
    property bool useIntrinsicBodyHeight: false
    readonly property real intrinsicBodyContentHeight: bodyContent.childrenRect.height > 0 ? bodyContent.childrenRect.y + bodyContent.childrenRect.height : (bodyTextLabel.visible ? bodyTextLabel.implicitHeight : root.bodyHeight)
    property alias headingButton: headingButton

    width: 320
    height: headingButton.height + (expanded ? body.height : 0)

    Button {
        id: headingButton

        width: parent.width
        height: Constants.appAccordionHeaderHeight
        font: Constants.appSectionFont
        enabled: root.headingEnabled
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true

        background: Rectangle {
            color: !root.headingEnabled ? Constants.appDisabledColor
                                        : headingButton.hovered ? Constants.appHoverColor
                                                                : Constants.appSubtleSurfaceColor
            border.color: headingButton.activeFocus ? Constants.appFocusColor : Constants.appBorderSubtleColor
            border.width: headingButton.activeFocus ? 2 : 1
        }

        contentItem: Item {
            Text {
                id: disclosureIndicator

                text: root.expanded ? "⌄" : "›"
                color: root.headingEnabled ? Constants.appPrimaryTextColor : Constants.appDisabledTextColor
                font: Constants.appSectionFont
                anchors.left: parent.left
                anchors.leftMargin: Constants.appControlHorizontalPadding
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                id: disclosureStatus

                visible: !root.headingEnabled
                text: qsTr("Disabled")
                color: Constants.appDisabledTextColor
                font: Constants.appCaptionFont
                anchors.right: parent.right
                anchors.rightMargin: Constants.appControlHorizontalPadding
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: root.sectionTitle
                color: root.headingEnabled ? Constants.appPrimaryTextColor : Constants.appDisabledTextColor
                font: Constants.appSectionFont
                elide: Text.ElideRight
                anchors.left: disclosureIndicator.right
                anchors.leftMargin: Constants.spacing
                anchors.right: disclosureStatus.visible ? disclosureStatus.left : parent.right
                anchors.rightMargin: Constants.appControlHorizontalPadding
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    Rectangle {
        id: body

        visible: root.expanded
        width: parent.width
        height: root.useIntrinsicBodyHeight ? root.intrinsicBodyContentHeight : root.bodyHeight
        color: Constants.appSurfaceColor
        border.color: Constants.appBorderSubtleColor
        anchors.top: headingButton.bottom

        Item {
            id: bodyScroll

            anchors.fill: parent
            clip: true

            Item {
                id: bodyContent

                width: bodyScroll.width
                height: root.useIntrinsicBodyHeight ? root.intrinsicBodyContentHeight : Math.max(root.bodyHeight, childrenRect.height)
            }
        }

        Text {
            id: bodyTextLabel

            visible: root.bodyText !== ""
            text: root.bodyText
            color: Constants.appSecondaryTextColor
            font: Constants.appCaptionFont
            anchors.centerIn: parent
        }
    }
}
