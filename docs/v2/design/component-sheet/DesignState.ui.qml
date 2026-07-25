/*
This is a static design-reference state sample intended for Qt Design Studio editing.
*/
import QtQuick

Rectangle {
    id: root

    property string label: ""
    property string sampleText: ""
    property color fillColor: "#FFFFFF"
    property color outlineColor: "#AAB7C5"
    property color textColor: "#17202A"
    property int outlineWidth: 1
    property bool showMarker: false

    width: 144
    height: 76
    radius: 4
    color: fillColor
    border.color: outlineColor
    border.width: outlineWidth

    Rectangle {
        visible: root.showMarker
        width: 4
        height: parent.height - 8
        x: 4
        y: 4
        radius: 2
        color: "#276DA3"
    }

    Text {
        x: root.showMarker ? 16 : 10
        y: 9
        width: parent.width - x - 10
        text: root.sampleText
        color: root.textColor
        font.family: "Segoe UI Variable"
        font.pixelSize: 14
        font.weight: Font.DemiBold
        elide: Text.ElideRight
    }

    Text {
        x: root.showMarker ? 16 : 10
        y: 46
        width: parent.width - x - 10
        text: root.label
        color: root.textColor
        font.family: "Segoe UI Variable"
        font.pixelSize: 12
        elide: Text.ElideRight
    }
}
