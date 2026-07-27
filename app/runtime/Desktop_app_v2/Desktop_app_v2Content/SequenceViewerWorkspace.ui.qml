/* This is a UI file (.ui.qml) intended for Qt Design Studio editing. */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Desktop_app_v2

Rectangle {
    id: root
    width: Constants.width - Constants.navigationWidth
    height: Constants.height - Constants.shellHeaderHeight
    color: Constants.backgroundColor

    property string presentation: "empty"
    property int currentFrame: 0
    property int totalFrames: 0
    property url currentFrameSource: ""
    property real zoomScale: 1.0
    property bool actualSize: false
    property int nativeImageWidth: 0
    property int nativeImageHeight: 0

    property alias openSequenceButton: openSequenceButton
    property alias jumpBack50Button: jumpBack50Button
    property alias jumpBack10Button: jumpBack10Button
    property alias previousButton: previousButton
    property alias nextButton: nextButton
    property alias jumpForward10Button: jumpForward10Button
    property alias jumpForward50Button: jumpForward50Button
    property alias frameSlider: frameSlider
    property alias directSeekField: directSeekField
    property alias zoomOutButton: zoomOutButton
    property alias zoomInButton: zoomInButton
    property alias fitButton: fitButton
    property alias actualSizeButton: actualSizeButton
    property alias viewerViewport: viewerViewport
    property alias currentFrameImage: currentFrameImage

    Column {
        anchors.fill: parent
        anchors.margins: Constants.workspaceMargin
        spacing: Constants.spacing

        Row {
            id: workspaceHeader
            width: parent.width
            Text { text: qsTr("Sequence Viewer"); font: Constants.largeFont; color: Constants.textColor; height: Constants.controlHeight; verticalAlignment: Text.AlignVCenter; width: parent.width - openSequenceButton.width }
            AppButton { id: openSequenceButton; text: qsTr("Open Sequence"); height: Constants.appStandardControlHeight }
        }

        Rectangle {
            id: viewerFocus
            width: parent.width
            height: parent.height - workspaceHeader.height - navigationPanel.height - Constants.spacing * 2
            color: Constants.viewerColor
            border.color: Constants.borderColor
            focus: true
            clip: true
            Flickable {
                id: viewerViewport
                anchors.fill: parent
                clip: true
                contentWidth: Math.max(width, currentFrameImage.width)
                contentHeight: Math.max(height, currentFrameImage.height)
                boundsBehavior: Flickable.StopAtBounds

                Image {
                    id: currentFrameImage
                    x: Math.max(0, (viewerViewport.width - width) / 2)
                    y: Math.max(0, (viewerViewport.height - height) / 2)
                    width: (root.actualSize && root.nativeImageWidth > 0
                            ? root.nativeImageWidth : viewerViewport.width)
                           * root.zoomScale
                    height: (root.actualSize && root.nativeImageHeight > 0
                             ? root.nativeImageHeight : viewerViewport.height)
                            * root.zoomScale
                    source: root.currentFrameSource
                    sourceSize.width: root.nativeImageWidth
                    sourceSize.height: root.nativeImageHeight
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: false
                    visible: root.currentFrameSource !== ""
                }
            }
            Text {
                text: root.presentation === "empty" ? qsTr("No Image Sequence selected") : qsTr("CURRENT FRAME")
                color: Constants.surfaceColor
                font: Constants.headingFont
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: Constants.spacing
            }
            Text {
                visible: root.presentation !== "empty" && root.presentation !== "error" && root.currentFrameSource === ""
                text: qsTr("Frame ") + root.currentFrame
                color: Constants.surfaceColor
                font: Constants.largeFont
                anchors.centerIn: parent
            }
            Text {
                visible: root.presentation === "error"
                text: qsTr("Error")
                color: Constants.surfaceColor
                font: Constants.largeFont
                anchors.centerIn: parent
            }
        }

        Rectangle {
            id: navigationPanel
            width: parent.width
            height: Constants.controlHeight * 2 + Constants.spacing * 3
            color: Constants.surfaceColor
            border.color: Constants.borderColor
            ScrollView {
                id: sequenceControlsScroll
                anchors.fill: parent
                contentWidth: Math.max(width, 900 * Constants.textScale + Constants.spacing * 2)
                contentHeight: availableHeight
                clip: true
                font: Constants.font

            Column {
                id: sequenceControls
                x: Constants.spacing
                y: Constants.spacing
                width: sequenceControlsScroll.contentWidth - Constants.spacing * 2
                height: implicitHeight
                spacing: Constants.spacing
                RowLayout {
                    width: parent.width
                    spacing: 6
                    AppButton { id: jumpBack50Button; text: qsTr("-50"); enabled: root.presentation === "ready"; Layout.preferredHeight: Constants.appStandardControlHeight }
                    AppButton { id: jumpBack10Button; text: qsTr("-10"); enabled: root.presentation === "ready"; Layout.preferredHeight: Constants.appStandardControlHeight }
                    AppButton { id: previousButton; text: qsTr("Previous"); enabled: root.presentation === "ready"; Layout.preferredHeight: Constants.appStandardControlHeight }
                    AppButton { id: nextButton; text: qsTr("Next"); enabled: root.presentation === "ready"; Layout.preferredHeight: Constants.appStandardControlHeight }
                    AppButton { id: jumpForward10Button; text: qsTr("+10"); enabled: root.presentation === "ready"; Layout.preferredHeight: Constants.appStandardControlHeight }
                    AppButton { id: jumpForward50Button; text: qsTr("+50"); enabled: root.presentation === "ready"; Layout.preferredHeight: Constants.appStandardControlHeight }
                    Slider { id: frameSlider; from: 1; to: Math.max(1, root.totalFrames); value: root.currentFrame; enabled: root.presentation === "ready"; Layout.fillWidth: true; Layout.minimumWidth: 120 * Constants.textScale }
                    Text { text: root.totalFrames === 0 ? qsTr("No sequence selected") : root.currentFrame + qsTr(" / ") + root.totalFrames; Layout.alignment: Qt.AlignVCenter }
                }
                RowLayout {
                    width: parent.width
                    spacing: 6
                    Text { text: qsTr("Go to frame"); Layout.alignment: Qt.AlignVCenter }
                    AppTextField { id: directSeekField; enabled: root.presentation === "ready"; placeholderText: qsTr("Frame"); Layout.preferredWidth: 84 * Constants.textScale; Layout.preferredHeight: Constants.appStandardControlHeight }
                    Item { Layout.fillWidth: true }
                    AppButton { id: zoomOutButton; text: qsTr("Zoom -"); enabled: root.presentation === "ready"; Layout.preferredHeight: Constants.appStandardControlHeight }
                    AppButton { id: zoomInButton; text: qsTr("Zoom +"); enabled: root.presentation === "ready"; Layout.preferredHeight: Constants.appStandardControlHeight }
                    AppButton { id: fitButton; text: qsTr("Fit"); enabled: root.presentation === "ready"; Layout.preferredHeight: Constants.appStandardControlHeight }
                    AppButton { id: actualSizeButton; text: qsTr("Actual Pixels"); enabled: root.presentation === "ready"; Layout.preferredHeight: Constants.appStandardControlHeight }
                }
            }
            }
        }
    }

    states: [
        State { name: "empty"; PropertyChanges { root.presentation: "empty"; root.currentFrame: 0; root.totalFrames: 0 } },
        State { name: "firstFrame"; PropertyChanges { root.presentation: "ready"; root.currentFrame: 1; root.totalFrames: 120 } },
        State { name: "middleFrame"; PropertyChanges { root.presentation: "ready"; root.currentFrame: 60; root.totalFrames: 120 } },
        State { name: "finalFrame"; PropertyChanges { root.presentation: "ready"; root.currentFrame: 120; root.totalFrames: 120 } },
        State { name: "oneFrame"; PropertyChanges { root.presentation: "ready"; root.currentFrame: 1; root.totalFrames: 1 } },
        State { name: "largeCount"; PropertyChanges { root.presentation: "ready"; root.currentFrame: 50000; root.totalFrames: 100000 } },
        State { name: "missingSkipped"; PropertyChanges { root.presentation: "ready"; root.currentFrame: 43; root.totalFrames: 120 } },
        State { name: "error"; PropertyChanges { root.presentation: "error"; root.currentFrame: 0; root.totalFrames: 0 } }
    ]
}
