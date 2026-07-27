/* This is a UI file (.ui.qml) intended for Qt Design Studio editing. */
import QtQuick
import QtQuick.Controls
import Desktop_app_v2

Rectangle {
    id: root

    width: Constants.width - Constants.navigationWidth
    height: Constants.height - Constants.shellHeaderHeight
    color: Constants.viewerColor
    border.color: Constants.borderColor
    clip: true

    property url source: ""
    property real zoomScale: 1.0
    property bool actualPixels: false
    property int nativeImageWidth: 0
    property int nativeImageHeight: 0
    property string placeholderText: ""

    property alias viewport: viewport
    property alias image: image
    property alias overlayLayer: overlayLayer
    property alias placeholder: placeholder
    default property alias overlayData: overlayLayer.data

    Flickable {
        id: viewport

        anchors.fill: parent
        clip: true
        contentWidth: Math.max(width, image.width)
        contentHeight: Math.max(height, image.height)
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.horizontal: ScrollBar {}
        ScrollBar.vertical: ScrollBar {}

        Image {
            id: image

            x: Math.max(0, (viewport.width - width) / 2)
            y: Math.max(0, (viewport.height - height) / 2)
            width: (root.actualPixels && root.nativeImageWidth > 0
                    ? root.nativeImageWidth : viewport.width) * root.zoomScale
            height: (root.actualPixels && root.nativeImageHeight > 0
                     ? root.nativeImageHeight : viewport.height) * root.zoomScale
            source: root.source
            sourceSize.width: root.nativeImageWidth > 0
                              ? root.nativeImageWidth : Math.round(width)
            sourceSize.height: root.nativeImageHeight > 0
                               ? root.nativeImageHeight : Math.round(height)
            fillMode: root.actualPixels ? Image.Pad : Image.PreserveAspectFit
            asynchronous: true
            cache: false
            visible: root.source.toString() !== ""

            Item {
                id: overlayLayer

                x: (image.width - image.paintedWidth) / 2
                y: (image.height - image.paintedHeight) / 2
                width: image.paintedWidth
                height: image.paintedHeight
                visible: image.visible
                clip: true
            }
        }
    }

    Text {
        id: placeholder

        anchors.centerIn: parent
        text: root.placeholderText
        color: Constants.surfaceColor
        font: Constants.headingFont
        horizontalAlignment: Text.AlignHCenter
        visible: root.source.toString() === ""
    }
}
