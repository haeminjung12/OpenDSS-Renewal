/* This is a UI file (.ui.qml) intended for Qt Design Studio editing. */
import QtQuick
import QtQuick.Controls
import Desktop_app_v2

Item {
    id: root
    property string presentation: "empty"
    property string activeModelText: qsTr("No Active Model")
    property bool sequenceTestExpanded: true
    property alias loadSequenceButton: loadSequenceButton
    property alias loadToMemoryButton: loadToMemoryButton
    property alias startStopButton: startStopButton
    property alias physicalDaqOutputControl: physicalDaqOutputControl
    property alias sequenceTestHeadingButton: sequenceTestSection.headingButton
    readonly property bool loaded: presentation === "ready" || presentation === "running" || presentation === "completed"
    readonly property bool running: presentation === "running"
    readonly property bool unavailable: presentation === "unavailable"
    readonly property bool error: presentation === "error"

    Rectangle {
        anchors.fill: parent
        color: Constants.backgroundColor

        Text {
            id: workspaceTitle
            text: qsTr("Sequence Test")
            font: Constants.largeFont
            color: Constants.textColor
            height: Constants.controlHeight
            verticalAlignment: Text.AlignVCenter
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: Constants.workspaceMargin
        }

        Row {
            anchors.top: workspaceTitle.bottom
            anchors.topMargin: Constants.spacing
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: Constants.workspaceMargin
            spacing: Constants.workspaceMargin

            Column {
                width: parent.width * 0.62
                height: parent.height
                spacing: Constants.spacing

                Rectangle {
                    width: parent.width
                    height: parent.height * 0.48
                    color: Constants.viewerColor
                    border.color: Constants.borderColor
                    Text { anchors.centerIn: parent; text: qsTr("First-frame preview"); color: "#ffffff"; font: Constants.headingFont }
                }

                Rectangle {
                    width: parent.width
                    height: parent.height * 0.48
                    color: Constants.surfaceColor
                    border.color: Constants.borderColor
                    Column {
                        anchors.fill: parent
                        anchors.margins: Constants.spacing * 2
                        spacing: Constants.spacing
                        Text { text: qsTr("Results"); color: Constants.textColor; font: Constants.headingFont }
                        Text { visible: !root.error; text: root.unavailable ? qsTr("Unavailable") : (root.running ? qsTr("Processing") : (root.presentation === "completed" ? qsTr("Completed") : qsTr("No results"))); color: Constants.mutedTextColor; font: Constants.smallFont }
                        Rectangle {
                            visible: root.error
                            width: parent.width
                            height: Constants.controlHeight
                            color: Constants.errorSurfaceColor
                            border.color: Constants.faultColor
                            Text { anchors.centerIn: parent; text: qsTr("Error"); color: Constants.faultColor; font: Constants.headingFont }
                        }
                    }
                }
            }

            ScrollView {
                width: parent.width * 0.38 - parent.spacing
                height: parent.height
                clip: true
                contentWidth: availableWidth

                Column {
                    width: parent.width
                    spacing: Constants.spacing

                    CollapsibleSection {
                        id: sequenceTestSection
                        sectionTitle: qsTr("Sequence Test")
                        expanded: root.sequenceTestExpanded
                        useIntrinsicBodyHeight: true
                        Item {
                            width: parent.width
                            height: sequenceTestContent.implicitHeight + Constants.spacing * 2
                            Column {
                                id: sequenceTestContent
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.margins: Constants.spacing
                                spacing: Constants.spacing
                                Text { text: qsTr("Active Model: %1").arg(root.activeModelText); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: root.loaded ? qsTr("Sequence loaded") : (root.presentation === "selected" ? qsTr("Sequence selected") : qsTr("No sequence selected")); color: Constants.mutedTextColor; font: Constants.smallFont }
                                Text { text: root.running ? qsTr("Processing progress") : qsTr("Load status"); color: Constants.mutedTextColor; font: Constants.smallFont }
                                Button { id: loadSequenceButton; text: qsTr("Load Sequence"); enabled: !root.running && !root.error }
                                Button { id: loadToMemoryButton; text: qsTr("Load to Memory"); enabled: root.presentation === "selected" }
                                CheckBox { id: physicalDaqOutputControl; text: qsTr("Physical DAQ Output"); enabled: !root.running }
                                Button { id: startStopButton; text: root.running ? qsTr("Stop") : qsTr("Start Sequence Test"); enabled: root.running || root.presentation === "ready" }
                            }
                        }
                    }
                }
            }
        }
    }
}
