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
    property bool hasRuns: false
    property bool runsPanelExpanded: true
    property bool rightPanelExpanded: true
    property bool notesEditing: false
    property alias runsPanelToggleButton: runsSection.headingButton
    property alias rightPanelToggleButton: rightPanelToggleButton
    property alias loadSelectedRunButton: loadSelectedRunButton
    property alias notesEditor: notesEditor
    property alias editNotesButton: editNotesButton
    property alias saveNotesButton: saveNotesButton
    property alias cancelNotesButton: cancelNotesButton
    property alias loadedRunStatusText: loadedRunStatusText.text
    property alias loadedRunStopReasonText: loadedRunStopReasonText.text
    property alias run042RowStatusText: run042RowStatusText.text
    property alias runsRowsHost: runsRowsHost

    Text {
        id: workspaceHeading
        text: qsTr("Runs")
        font: Constants.largeFont
        color: Constants.textColor
        height: Constants.controlHeight
        verticalAlignment: Text.AlignVCenter
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: Constants.workspaceMargin
    }

    SplitView {
        font: Constants.font
        anchors.top: workspaceHeading.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Constants.workspaceMargin
        anchors.topMargin: Constants.spacing

        Rectangle {
            SplitView.fillWidth: true
            color: Constants.surfaceColor
            border.color: Constants.borderColor

            ScrollView {
                id: runDetailsScroll
                anchors.fill: parent
                anchors.margins: Constants.spacing * 2
                clip: true
                contentWidth: availableWidth

                Column {
                    width: runDetailsScroll.availableWidth
                    height: implicitHeight
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
                            Text { id: loadedRunStatusText; text: qsTr("Live Sorting  •  Completed  •  2026-07-23 10:41"); color: Constants.textColor; font: Constants.smallFont }
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

                    Flow {
                        visible: root.loadedRunId !== "" && !root.runsError
                        width: parent.width
                        height: childrenRect.height
                        spacing: Constants.spacing

                        Rectangle {
                            width: parent.width >= Math.round(900 * Constants.textScale) ? (parent.width - parent.spacing * 2) / 3 : parent.width
                            height: Math.round(142 * Constants.textScale)
                            color: Constants.backgroundColor
                            border.color: Constants.borderColor
                            Column { anchors.fill: parent; anchors.margins: Constants.spacing; spacing: 5
                                Text { text: qsTr("Experimental Information"); font: Constants.headingFont; color: Constants.textColor; width: parent.width; wrapMode: Text.WordWrap }
                                Text { text: qsTr("Experiment Type: Sorting Run"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Started: 2026-07-23 10:41"); color: Constants.textColor; font: Constants.smallFont }
                                Text { id: loadedRunStopReasonText; text: qsTr("Stop Reason: Completed duration"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Save Location: C:/OpenDSS/Runs/Run-042"); elide: Text.ElideMiddle; color: Constants.mutedTextColor; font: Constants.smallFont; width: parent.width }
                            }
                        }
                        Rectangle {
                            width: parent.width >= Math.round(900 * Constants.textScale) ? (parent.width - parent.spacing * 2) / 3 : parent.width
                            height: Math.round(142 * Constants.textScale)
                            color: Constants.backgroundColor
                            border.color: Constants.borderColor
                            Column { anchors.fill: parent; anchors.margins: Constants.spacing; spacing: 5
                                Text { text: qsTr("Routing / Configuration Snapshot"); font: Constants.headingFont; color: Constants.textColor; width: parent.width; wrapMode: Text.WordWrap }
                                Text { text: qsTr("Trigger Every Droplet: Off"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Hit Class: Class 1"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Hit Boundary: Top is Hit"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Model: DropletNet-04"); color: Constants.mutedTextColor; font: Constants.smallFont }
                            }
                        }
                        Rectangle {
                            width: parent.width >= Math.round(900 * Constants.textScale) ? (parent.width - parent.spacing * 2) / 3 : parent.width
                            height: Math.round(142 * Constants.textScale)
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

                    Flow {
                        visible: root.loadedRunId !== "" && !root.runsError
                        width: parent.width
                        height: childrenRect.height
                        spacing: Constants.spacing

                        Rectangle {
                            width: parent.width >= Math.round(680 * Constants.textScale) ? (parent.width - parent.spacing) * 0.5 : parent.width
                            height: Math.round(118 * Constants.textScale)
                            color: Constants.backgroundColor
                            border.color: Constants.borderColor
                            Column { anchors.fill: parent; anchors.margins: Constants.spacing; spacing: 5
                                Text { text: qsTr("Counts"); font: Constants.headingFont; color: Constants.textColor; width: parent.width; wrapMode: Text.WordWrap }
                                Text { text: qsTr("Predicted Class 0: 714\nPredicted Class 1: 534"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Decision Hit: 702  •  Decision Waste: 546"); color: Constants.mutedTextColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            }
                        }
                        Rectangle {
                            width: parent.width >= Math.round(680 * Constants.textScale) ? (parent.width - parent.spacing) * 0.5 : parent.width
                            height: Math.round(118 * Constants.textScale)
                            color: Constants.backgroundColor
                            border.color: Constants.borderColor
                            Column { anchors.fill: parent; anchors.margins: Constants.spacing; spacing: 5
                                Text { text: qsTr("Observed Route"); font: Constants.headingFont; color: Constants.textColor; width: parent.width; wrapMode: Text.WordWrap }
                                Text { text: qsTr("Observed Hit: 688\nObserved Waste: 536\nUnresolved: 24"); color: Constants.textColor; font: Constants.smallFont }
                            }
                        }
                    }

                    Rectangle {
                        visible: root.loadedRunId !== "" && !root.runsError
                        width: parent.width
                        height: routeMatrixContent.implicitHeight + Constants.spacing * 2
                        color: Constants.backgroundColor
                        border.color: Constants.borderColor
                        Column { id: routeMatrixContent; anchors.fill: parent; anchors.margins: Constants.spacing; spacing: 7
                            Text { text: qsTr("Decision-versus-Observed Route"); font: Constants.headingFont; color: Constants.textColor; width: parent.width; wrapMode: Text.WordWrap }
                            Grid { id: routeMatrix; width: parent.width; columns: 4; columnSpacing: Constants.spacing; rowSpacing: 5
                                readonly property real cellWidth: (width - columnSpacing * 3) / 4
                                Text { width: routeMatrix.cellWidth; text: qsTr("Decision"); font: Constants.smallFont; color: Constants.mutedTextColor; wrapMode: Text.WordWrap }
                                Text { width: routeMatrix.cellWidth; text: qsTr("Observed Hit"); font: Constants.smallFont; color: Constants.mutedTextColor; wrapMode: Text.WordWrap }
                                Text { width: routeMatrix.cellWidth; text: qsTr("Observed Waste"); font: Constants.smallFont; color: Constants.mutedTextColor; wrapMode: Text.WordWrap }
                                Text { width: routeMatrix.cellWidth; text: qsTr("Unresolved"); font: Constants.smallFont; color: Constants.mutedTextColor; wrapMode: Text.WordWrap }
                                Text { width: routeMatrix.cellWidth; text: qsTr("Hit"); color: Constants.textColor; font: Constants.smallFont; wrapMode: Text.WordWrap }
                                Text { width: routeMatrix.cellWidth; text: qsTr("681"); color: Constants.textColor; font: Constants.smallFont; wrapMode: Text.WordWrap }
                                Text { width: routeMatrix.cellWidth; text: qsTr("7"); color: Constants.textColor; font: Constants.smallFont; wrapMode: Text.WordWrap }
                                Text { width: routeMatrix.cellWidth; text: qsTr("14"); color: Constants.textColor; font: Constants.smallFont; wrapMode: Text.WordWrap }
                                Text { width: routeMatrix.cellWidth; text: qsTr("Waste"); color: Constants.textColor; font: Constants.smallFont; wrapMode: Text.WordWrap }
                                Text { width: routeMatrix.cellWidth; text: qsTr("7"); color: Constants.textColor; font: Constants.smallFont; wrapMode: Text.WordWrap }
                                Text { width: routeMatrix.cellWidth; text: qsTr("529"); color: Constants.textColor; font: Constants.smallFont; wrapMode: Text.WordWrap }
                                Text { width: routeMatrix.cellWidth; text: qsTr("10"); color: Constants.textColor; font: Constants.smallFont; wrapMode: Text.WordWrap }
                            }
                        }
                    }

                    Rectangle {
                        visible: root.loadedRunId !== "" && !root.runsError
                        width: parent.width
                        height: filesNotesFlow.childrenRect.height + Constants.spacing * 2
                        color: Constants.backgroundColor
                        border.color: Constants.borderColor
                        Flow { id: filesNotesFlow; anchors.fill: parent; anchors.margins: Constants.spacing; spacing: Constants.spacing
                            Column { width: parent.width >= Math.round(680 * Constants.textScale) ? (parent.width - parent.spacing) * 0.5 : parent.width; height: implicitHeight; spacing: 6
                                Text { text: qsTr("Files and Notes"); font: Constants.headingFont; color: Constants.textColor; width: parent.width; wrapMode: Text.WordWrap }
                                AppButton { text: qsTr("Open Droplet Log"); width: parent.width; height: Constants.appStandardControlHeight }
                                AppButton { text: qsTr("Open Run Folder"); width: parent.width; height: Constants.appStandardControlHeight }
                                Row { width: parent.width; spacing: 6
                                    AppButton { text: qsTr("Open Droplet Crop"); width: (parent.width - parent.spacing) / 2; height: Constants.appStandardControlHeight }
                                    AppButton { text: qsTr("Open Saved Sequence"); width: (parent.width - parent.spacing) / 2; height: Constants.appStandardControlHeight }
                                }
                            }
                            Column { width: parent.width >= Math.round(680 * Constants.textScale) ? (parent.width - parent.spacing) * 0.5 : parent.width; height: implicitHeight; spacing: 6
                                Text { text: qsTr("Notes"); font: Constants.headingFont; color: Constants.textColor; width: parent.width; wrapMode: Text.WordWrap }
                                AppTextArea {
                                    id: notesEditor
                                    width: parent.width
                                    height: Math.max(implicitHeight, Math.round(80 * Constants.textScale))
                                    text: qsTr("Factual Run notes are shown here.")
                                    readOnly: !root.notesEditing
                                    wrapMode: TextEdit.Wrap
                                }
                                Row { spacing: 6
                                    AppButton { id: editNotesButton; visible: !root.notesEditing; text: qsTr("Edit Notes"); height: Constants.appStandardControlHeight }
                                    AppButton { id: saveNotesButton; visible: root.notesEditing; text: qsTr("Save Notes"); height: Constants.appStandardControlHeight }
                                    AppButton { id: cancelNotesButton; visible: root.notesEditing; text: qsTr("Cancel"); height: Constants.appStandardControlHeight }
                                }
                            }
                        }
                    }
                }
            }
        }

        Item {
            id: runsPanel
            SplitView.preferredWidth: Constants.operationPanelWidth
            SplitView.minimumWidth: Constants.collapsedOperationPanelWidth
            SplitView.maximumWidth: root.rightPanelExpanded ? Math.max(Constants.collapsedOperationPanelWidth, parent.width * 0.75) : Constants.collapsedOperationPanelWidth

            Rectangle {
                id: panelTopStrip
                height: Constants.controlHeight
                color: Constants.backgroundColor
                border.color: Constants.borderColor
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                Text {
                    text: qsTr("Runs")
                    visible: root.rightPanelExpanded
                    font: Constants.headingFont
                    color: Constants.textColor
                    anchors.left: parent.left
                    anchors.leftMargin: Constants.spacing
                    anchors.right: parent.right
                    anchors.rightMargin: rightPanelToggleButton.width + Constants.spacing
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                }
            }
            AppInspectorRail {
                id: rightPanelToggleButton
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: root.rightPanelExpanded ? "›" : "‹"
                z: 1
                Accessible.name: root.rightPanelExpanded ? qsTr("Collapse Runs panel") : qsTr("Expand Runs panel")
            }

            AppAccordion {
                id: runsSection
                visible: root.rightPanelExpanded
                anchors.top: panelTopStrip.bottom
                anchors.topMargin: Constants.spacing
                anchors.left: parent.left
                anchors.leftMargin: Constants.spacing
                anchors.right: parent.right
                anchors.rightMargin: Constants.spacing
                sectionTitle: qsTr("Runs")
                expanded: root.runsPanelExpanded
                useIntrinsicBodyHeight: true

                Item {
                    width: parent.width
                    height: runsPanelContent.implicitHeight + Constants.spacing * 2
                    Column {
                        id: runsPanelContent
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: Constants.spacing
                        spacing: 6

                    Rectangle { visible: !root.hasRuns && !root.runsError; width: parent.width; height: 70; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { text: qsTr("No Runs found"); color: Constants.mutedTextColor; font: Constants.smallFont; anchors.centerIn: parent } }
                    Column {
                        id: runsRowsHost
                        width: parent.width
                        spacing: 6

                        Text {
                            id: run042RowStatusText
                            visible: false
                        }
                    }
                    Rectangle { visible: root.runsError; width: parent.width; height: 46; color: Constants.errorSurfaceColor; border.color: Constants.faultColor; Text { text: qsTr("Error"); color: Constants.faultColor; font: Constants.headingFont; anchors.centerIn: parent } }

                        AppButton {
                            id: loadSelectedRunButton
                            width: parent.width
                            height: Constants.appPrimaryButtonHeight
                            text: qsTr("Load selected Run")
                            visualRole: "primary"
                            enabled: root.selectedRunId !== "" && !root.runsError
                        }
                    }
                }
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
