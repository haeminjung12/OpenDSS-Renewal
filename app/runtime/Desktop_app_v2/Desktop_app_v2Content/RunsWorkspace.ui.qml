/* This is a UI file (.ui.qml) intended for Qt Design Studio editing. */
import QtQuick
import QtQuick.Controls
import Desktop_app_v2

Rectangle {
    id: root
    width: Constants.width
    height: Constants.height
    color: Constants.backgroundColor

    property string selectedRunId: ""
    property string loadedRunId: ""
    property bool runsError: false
    property bool runsPanelExpanded: true
    property bool notesEditing: false
    property alias runsPanelToggleButton: runsPanelToggleButton
    property alias loadSelectedRunButton: loadSelectedRunButton
    property alias notesEditor: notesEditor
    property alias editNotesButton: editNotesButton
    property alias saveNotesButton: saveNotesButton
    property alias cancelNotesButton: cancelNotesButton

    Text {
        id: workspaceHeading
        text: qsTr("Results > Runs")
        font: Constants.largeFont
        color: Constants.textColor
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: Constants.workspaceMargin
    }

    Row {
        anchors.top: workspaceHeading.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Constants.workspaceMargin
        anchors.topMargin: Constants.spacing
        spacing: Constants.spacing

        Rectangle {
            width: parent.width - runsPanel.width - parent.spacing
            height: parent.height
            color: Constants.surfaceColor
            border.color: Constants.borderColor

            ScrollView {
                anchors.fill: parent
                anchors.margins: Constants.spacing * 2
                clip: true
                contentWidth: availableWidth

                Column {
                    width: parent.width
                    spacing: Constants.spacing

                    Rectangle {
                        visible: root.runsError
                        width: parent.width
                        height: 52
                        color: Constants.errorSurfaceColor
                        border.color: Constants.faultColor

                        Text {
                            text: qsTr("Error")
                            color: Constants.faultColor
                            font: Constants.headingFont
                            anchors.centerIn: parent
                        }
                    }

                    Text {
                        visible: !root.runsError
                        text: root.loadedRunId === "" ? qsTr("No Run loaded") : qsTr("Loaded Run")
                        font: Constants.headingFont
                        color: Constants.textColor
                    }

                    Rectangle {
                        visible: root.loadedRunId !== "" && !root.runsError
                        width: parent.width
                        height: 112
                        color: Constants.backgroundColor
                        border.color: Constants.borderColor

                        Column {
                            anchors.fill: parent
                            anchors.margins: Constants.spacing
                            spacing: 4

                            Text { text: qsTr("Run-042"); font: Constants.headingFont; color: Constants.textColor }
                            Text { text: qsTr("Live Sorting  •  Completed  •  2026-07-23 10:41"); color: Constants.textColor; font: Constants.smallFont }
                            Text { text: qsTr("Total Droplets: 1,248  •  Model: DropletNet-04"); color: Constants.mutedTextColor; font: Constants.smallFont }
                            Text { text: qsTr("Duration: 00:03:12"); color: Constants.mutedTextColor; font: Constants.smallFont }
                        }
                    }

                    Rectangle {
                        visible: root.loadedRunId === "" && !root.runsError
                        width: parent.width
                        height: 112
                        color: Constants.backgroundColor
                        border.color: Constants.borderColor

                        Text { text: qsTr("Select a Run and choose Load selected Run."); color: Constants.mutedTextColor; font: Constants.smallFont; anchors.centerIn: parent }
                    }

                    Row {
                        visible: root.loadedRunId !== "" && !root.runsError
                        width: parent.width
                        spacing: Constants.spacing

                        Rectangle {
                            width: (parent.width - parent.spacing * 2) / 3
                            height: 142
                            color: Constants.backgroundColor
                            border.color: Constants.borderColor
                            Column { anchors.fill: parent; anchors.margins: Constants.spacing; spacing: 5
                                Text { text: qsTr("Experimental Information"); font: Constants.headingFont; color: Constants.textColor }
                                Text { text: qsTr("Experiment Type: Sorting Run"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Started: 2026-07-23 10:41"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Stop Reason: Completed duration"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Save Location: C:/OpenDSS/Runs/Run-042"); elide: Text.ElideMiddle; color: Constants.mutedTextColor; font: Constants.smallFont; width: parent.width }
                            }
                        }
                        Rectangle {
                            width: (parent.width - parent.spacing * 2) / 3
                            height: 142
                            color: Constants.backgroundColor
                            border.color: Constants.borderColor
                            Column { anchors.fill: parent; anchors.margins: Constants.spacing; spacing: 5
                                Text { text: qsTr("Routing / Configuration Snapshot"); font: Constants.headingFont; color: Constants.textColor }
                                Text { text: qsTr("Trigger Every Droplet: Off"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Hit Class: Class 1"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Hit Boundary: Top is Hit"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Model: DropletNet-04"); color: Constants.mutedTextColor; font: Constants.smallFont }
                            }
                        }
                        Rectangle {
                            width: (parent.width - parent.spacing * 2) / 3
                            height: 142
                            color: Constants.backgroundColor
                            border.color: Constants.borderColor
                            Column { anchors.fill: parent; anchors.margins: Constants.spacing; spacing: 5
                                Text { text: qsTr("Hardware / Fixed Processing Snapshot"); font: Constants.headingFont; color: Constants.textColor; wrapMode: Text.WordWrap; width: parent.width }
                                Text { text: qsTr("Camera: Illustrative Camera A"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("DAQ Output: On"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Resolution: 2048 × 2048, 8-bit"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Processing: Qualified fixed configuration"); color: Constants.mutedTextColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            }
                        }
                    }

                    Row {
                        visible: root.loadedRunId !== "" && !root.runsError
                        width: parent.width
                        spacing: Constants.spacing

                        Rectangle {
                            width: (parent.width - parent.spacing) * 0.5
                            height: 118
                            color: Constants.backgroundColor
                            border.color: Constants.borderColor
                            Column { anchors.fill: parent; anchors.margins: Constants.spacing; spacing: 5
                                Text { text: qsTr("Counts"); font: Constants.headingFont; color: Constants.textColor }
                                Text { text: qsTr("Predicted Class 0: 714\nPredicted Class 1: 534"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Decision Hit: 702  •  Decision Waste: 546"); color: Constants.mutedTextColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            }
                        }
                        Rectangle {
                            width: (parent.width - parent.spacing) * 0.5
                            height: 118
                            color: Constants.backgroundColor
                            border.color: Constants.borderColor
                            Column { anchors.fill: parent; anchors.margins: Constants.spacing; spacing: 5
                                Text { text: qsTr("Observed Route"); font: Constants.headingFont; color: Constants.textColor }
                                Text { text: qsTr("Observed Hit: 688\nObserved Waste: 536\nUnresolved: 24"); color: Constants.textColor; font: Constants.smallFont }
                            }
                        }
                    }

                    Rectangle {
                        visible: root.loadedRunId !== "" && !root.runsError
                        width: parent.width
                        height: 154
                        color: Constants.backgroundColor
                        border.color: Constants.borderColor
                        Column { anchors.fill: parent; anchors.margins: Constants.spacing; spacing: 7
                            Text { text: qsTr("Decision-versus-Observed Route"); font: Constants.headingFont; color: Constants.textColor }
                            Grid { columns: 4; columnSpacing: 22; rowSpacing: 5
                                Text { text: qsTr("Decision"); font: Constants.smallFont; color: Constants.mutedTextColor }
                                Text { text: qsTr("Observed Hit"); font: Constants.smallFont; color: Constants.mutedTextColor }
                                Text { text: qsTr("Observed Waste"); font: Constants.smallFont; color: Constants.mutedTextColor }
                                Text { text: qsTr("Unresolved"); font: Constants.smallFont; color: Constants.mutedTextColor }
                                Text { text: qsTr("Hit"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("681"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("7"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("14"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Waste"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("7"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("529"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("10"); color: Constants.textColor; font: Constants.smallFont }
                            }
                        }
                    }

                    Rectangle {
                        visible: root.loadedRunId !== "" && !root.runsError
                        width: parent.width
                        height: 190
                        color: Constants.backgroundColor
                        border.color: Constants.borderColor
                        Row { anchors.fill: parent; anchors.margins: Constants.spacing; spacing: Constants.spacing
                            Column { width: (parent.width - parent.spacing) * 0.5; spacing: 6
                                Text { text: qsTr("Files and Notes"); font: Constants.headingFont; color: Constants.textColor }
                                Button { text: qsTr("Open Droplet Log"); width: parent.width; height: Constants.controlHeight }
                                Button { text: qsTr("Open Run Folder"); width: parent.width; height: Constants.controlHeight }
                                Row { width: parent.width; spacing: 6
                                    Button { text: qsTr("Open Droplet Crop"); width: (parent.width - parent.spacing) / 2; height: Constants.controlHeight }
                                    Button { text: qsTr("Open Saved Sequence"); width: (parent.width - parent.spacing) / 2; height: Constants.controlHeight }
                                }
                            }
                            Column { width: (parent.width - parent.spacing) * 0.5; spacing: 6
                                Text { text: qsTr("Notes"); font: Constants.headingFont; color: Constants.textColor }
                                TextArea {
                                    id: notesEditor
                                    width: parent.width
                                    height: 64
                                    text: qsTr("Factual Run notes are shown here.")
                                    readOnly: !root.notesEditing
                                    wrapMode: TextEdit.Wrap
                                }
                                Row { spacing: 6
                                    Button { id: editNotesButton; visible: !root.notesEditing; text: qsTr("Edit Notes"); height: Constants.controlHeight }
                                    Button { id: saveNotesButton; visible: root.notesEditing; text: qsTr("Save Notes"); height: Constants.controlHeight }
                                    Button { id: cancelNotesButton; visible: root.notesEditing; text: qsTr("Cancel"); height: Constants.controlHeight }
                                }
                                Text { text: qsTr("Provenance  ›  Collapsed"); color: Constants.textColor; font: Constants.smallFont }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            id: runsPanel
            width: Constants.operationPanelWidth
            height: parent.height
            color: Constants.surfaceColor
            border.color: Constants.borderColor

            Button {
                id: runsPanelToggleButton
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Constants.spacing
                height: Constants.controlHeight
                text: root.runsPanelExpanded ? qsTr("Runs  ⌄") : qsTr("Runs  ›")
            }

            Column {
                visible: root.runsPanelExpanded
                anchors.top: runsPanelToggleButton.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Constants.spacing
                spacing: 6

                    Rectangle { visible: root.selectedRunId === "" && root.loadedRunId === "" && !root.runsError; width: parent.width; height: 70; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { text: qsTr("No Runs found"); color: Constants.mutedTextColor; font: Constants.smallFont; anchors.centerIn: parent } }
                    Rectangle {
                        visible: root.selectedRunId !== "" || root.loadedRunId !== ""
                        width: parent.width
                        height: 94
                        color: root.selectedRunId === "Run-042" ? "#e8f0fa" : Constants.backgroundColor
                        border.color: root.selectedRunId === "Run-042" ? Constants.accentColor : Constants.borderColor
                        Column {
                            anchors.fill: parent
                            anchors.margins: 7
                            Text { text: qsTr("Run Name: Run-042"); color: Constants.textColor; font: Constants.smallFont }
                            Text { text: qsTr("Live Sorting  |  Completed"); color: Constants.textColor; font: Constants.smallFont }
                            Text { text: qsTr("Started: 2026-07-23 10:41  |  Duration: 00:03:12"); color: Constants.mutedTextColor; font: Constants.smallFont }
                            Text { text: qsTr("Total Droplets: 1,248  |  Model: DropletNet-04"); color: Constants.mutedTextColor; font: Constants.smallFont }
                        }
                    }
                    Rectangle {
                        visible: root.selectedRunId !== "" || root.loadedRunId !== ""
                        width: parent.width
                        height: 94
                        color: Constants.backgroundColor
                        border.color: Constants.borderColor
                        Column {
                            anchors.fill: parent
                            anchors.margins: 7
                            Text { text: qsTr("Run Name: Run-043"); color: Constants.textColor; font: Constants.smallFont }
                            Text { text: qsTr("Sequence Test  |  Stopped"); color: Constants.textColor; font: Constants.smallFont }
                            Text { text: qsTr("Started: 2026-07-23 11:08  |  Duration: 00:02:26"); color: Constants.mutedTextColor; font: Constants.smallFont }
                            Text { text: qsTr("Total Droplets: 876  |  Model: No model"); color: Constants.mutedTextColor; font: Constants.smallFont }
                        }
                    }
                    Rectangle { visible: root.runsError; width: parent.width; height: 46; color: Constants.errorSurfaceColor; border.color: Constants.faultColor; Text { text: qsTr("Error"); color: Constants.faultColor; font: Constants.headingFont; anchors.centerIn: parent } }
            }

            Button {
                id: loadSelectedRunButton
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: Constants.spacing
                height: Constants.controlHeight
                text: qsTr("Load selected Run")
                enabled: root.selectedRunId !== "" && !root.runsError
            }
        }
    }

    states: [
        State { name: "runsEmpty"; PropertyChanges { root.selectedRunId: ""; root.loadedRunId: ""; root.runsError: false; root.notesEditing: false } },
        State { name: "runsSelected"; PropertyChanges { root.selectedRunId: "Run-042"; root.runsError: false; root.notesEditing: false } },
        State { name: "runsLoaded"; PropertyChanges { root.selectedRunId: "Run-042"; root.loadedRunId: "Run-042"; root.runsError: false; root.notesEditing: false } },
        State { name: "runsNotesEditing"; PropertyChanges { root.selectedRunId: "Run-042"; root.loadedRunId: "Run-042"; root.runsError: false; root.notesEditing: true } },
        State { name: "runsError"; PropertyChanges { root.selectedRunId: ""; root.loadedRunId: ""; root.runsError: true; root.notesEditing: false } }
    ]
}
