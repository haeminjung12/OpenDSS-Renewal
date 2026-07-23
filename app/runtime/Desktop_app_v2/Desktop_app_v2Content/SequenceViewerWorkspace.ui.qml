/* This is a UI file (.ui.qml) intended for Qt Design Studio editing. */
import QtQuick
import QtQuick.Controls
import Desktop_app_v2

Rectangle {
    id: root
    width: Constants.width - Constants.navigationWidth
    height: Constants.height - Constants.shellHeaderHeight
    color: Constants.backgroundColor

    property string presentation: "empty"
    property int currentFrame: 0
    property int totalFrames: 0

    property alias openSequenceButton: openSequenceButton
    property alias previousButton: previousButton
    property alias nextButton: nextButton
    property alias directSeekField: directSeekField
    property alias zoomOutButton: zoomOutButton
    property alias zoomInButton: zoomInButton
    property alias fitButton: fitButton
    property alias actualSizeButton: actualSizeButton
    property alias viewerFocus: viewerFocus

    Column {
        anchors.fill: parent
        anchors.margins: Constants.workspaceMargin
        spacing: Constants.spacing

        Row {
            width: parent.width
            Text { text: qsTr("Data > Sequence Viewer"); font: Constants.headingFont; width: parent.width - openSequenceButton.width }
            Button { id: openSequenceButton; text: qsTr("Open Sequence"); height: Constants.controlHeight }
        }

        Rectangle {
            id: viewerFocus
            width: parent.width
            height: parent.height - navigationPanel.height - Constants.spacing * 2
            color: Constants.viewerColor
            border.color: Constants.borderColor
            focus: true
            Text {
                text: root.presentation === "empty" ? qsTr("No Image Sequence selected") : qsTr("CURRENT FRAME")
                color: Constants.surfaceColor
                font: Constants.headingFont
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: Constants.spacing
            }
            Text {
                visible: root.presentation !== "empty" && root.presentation !== "error"
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
            height: 112
            color: Constants.surfaceColor
            border.color: Constants.borderColor
            Column {
                anchors.fill: parent
                anchors.margins: Constants.spacing
                spacing: 6
                Text { text: root.totalFrames === 0 ? qsTr("No sequence selected") : qsTr("Frame ") + root.currentFrame + qsTr(" of ") + root.totalFrames; font: Constants.headingFont }
                Row {
                    spacing: 6
                    Button { id: previousButton; text: qsTr("Previous"); enabled: root.presentation === "ready"; height: Constants.controlHeight }
                    Button { id: nextButton; text: qsTr("Next"); enabled: root.presentation === "ready"; height: Constants.controlHeight }
                    Text { text: qsTr("Frame"); anchors.verticalCenter: parent.verticalCenter }
                    TextField { id: directSeekField; width: 100; height: Constants.controlHeight; enabled: root.presentation === "ready"; placeholderText: qsTr("Seek") }
                    Button { id: zoomOutButton; text: qsTr("Zoom -"); enabled: root.presentation === "ready"; height: Constants.controlHeight }
                    Button { id: zoomInButton; text: qsTr("Zoom +"); enabled: root.presentation === "ready"; height: Constants.controlHeight }
                    Button { id: fitButton; text: qsTr("Fit"); enabled: root.presentation === "ready"; height: Constants.controlHeight }
                    Button { id: actualSizeButton; text: qsTr("1:1"); enabled: root.presentation === "ready"; height: Constants.controlHeight }
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
