/*
This is a static design-reference tile intended for Qt Design Studio editing.
*/
import QtQuick

Rectangle {
    id: root

    default property alias content: contentHost.data
    property string componentName: ""
    property string purpose: ""
    property string anatomy: ""
    property string variants: ""
    property string stateList: ""
    property string dimensions: ""
    property string accessibilityNote: ""
    property int contentHeight: 170

    width: 1120
    height: headerColumn.height + contentHeight + notesColumn.height + 48
    radius: 6
    color: "#FFFFFF"
    border.color: "#D7DEE7"
    border.width: 1

    Column {
        id: headerColumn
        x: 16
        y: 16
        width: parent.width - 32
        spacing: 6

        Text {
            width: parent.width
            text: root.componentName
            color: "#17202A"
            font.family: "Segoe UI Variable"
            font.pixelSize: 18
            font.weight: Font.DemiBold
        }
        Text {
            width: parent.width
            text: root.purpose
            color: "#5D6978"
            font.family: "Segoe UI Variable"
            font.pixelSize: 14
            lineHeight: 1.35
            wrapMode: Text.WordWrap
        }
    }

    Item {
        id: contentHost
        x: 16
        y: headerColumn.y + headerColumn.height + 16
        width: parent.width - 32
        height: root.contentHeight
    }

    Column {
        id: notesColumn
        x: 16
        y: contentHost.y + contentHost.height + 16
        width: parent.width - 32
        spacing: 5

        Text {
            width: parent.width
            text: qsTr("Anatomy: %1").arg(root.anatomy)
            color: "#17202A"
            font.family: "Segoe UI Variable"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
        Text {
            width: parent.width
            text: qsTr("Variants: %1").arg(root.variants)
            color: "#5D6978"
            font.family: "Segoe UI Variable"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
        Text {
            width: parent.width
            text: qsTr("States: %1").arg(root.stateList)
            color: "#5D6978"
            font.family: "Segoe UI Variable"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
        Text {
            width: parent.width
            text: qsTr("Minimum and alignment: %1").arg(root.dimensions)
            color: "#5D6978"
            font.family: "Segoe UI Variable"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
        Text {
            width: parent.width
            text: qsTr("Accessibility: %1").arg(root.accessibilityNote)
            color: "#276DA3"
            font.family: "Segoe UI Variable"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }
}
