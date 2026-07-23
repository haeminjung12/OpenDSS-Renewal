/* This is a UI file (.ui.qml) intended for Qt Design Studio editing. */
import QtQuick
import QtQuick.Controls
import Desktop_app_v2

Rectangle {
    id: root
    width: 1200
    height: 680
    color: Constants.backgroundColor
    property string presentation: "empty"
    property string datasetText: qsTr("No Dataset selected")
    property string deviceText: qsTr("CPU (automatic)")
    property string disabledReason: qsTr("No dataset selected")
    property string modelNameText: ""
    property string saveLocationText: ""
    property bool startEnabled: false
    property bool showRunning: false
    property bool showError: false
    property bool showInterrupted: false
    property bool trainingSetupExpanded: true
    property bool trainingStatusExpanded: true
    property alias selectDatasetButton: selectDatasetButton
    property alias modelNameField: modelNameField
    property alias saveLocationField: saveLocationField
    property alias browseButton: browseButton
    property alias startButton: startButton
    property alias stopButton: stopButton
    property alias retrySaveButton: retrySaveButton
    property alias trainingSetupHeadingButton: trainingSetupSection.headingButton
    property alias trainingStatusHeadingButton: trainingStatusSection.headingButton

    Column {
        id: headingColumn
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Constants.workspaceMargin
        spacing: 4

        Text { text: qsTr("Train"); font: Constants.largeFont; color: Constants.textColor; height: Constants.controlHeight; verticalAlignment: Text.AlignVCenter }
        Text { text: root.disabledReason; visible: root.presentation === "empty" || root.presentation === "unavailable"; color: Constants.warningColor; font: Constants.smallFont }
        Text { text: qsTr("Training stopped"); visible: root.showInterrupted; color: Constants.warningColor; font: Constants.headingFont }
    }

    Row {
        anchors.top: headingColumn.bottom
        anchors.topMargin: Constants.spacing
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Constants.workspaceMargin
        anchors.rightMargin: Constants.workspaceMargin
        anchors.bottomMargin: Constants.workspaceMargin
        spacing: Constants.spacing

        Item {
            width: parent.width - operationPanel.width - parent.spacing
            height: parent.height

            Rectangle {
                id: datasetSummary
                width: parent.width
                height: parent.height * 0.3
                color: Constants.surfaceColor
                border.color: Constants.borderColor
                Column {
                    anchors.fill: parent
                    anchors.margins: Constants.spacing * 2
                    spacing: Constants.spacing
                    Text { text: qsTr("Dataset Summary"); font: Constants.headingFont }
                    Text { text: root.datasetText; wrapMode: Text.WordWrap; width: parent.width }
                    Text { text: qsTr("Classes: 2 or 3"); color: Constants.mutedTextColor }
                    Text { text: qsTr("Eligible labeled crops: factual count"); color: Constants.mutedTextColor }
                    Button { id: selectDatasetButton; text: qsTr("Select Dataset"); height: Constants.controlHeight }
                }
            }

            Rectangle {
                anchors.top: datasetSummary.bottom
                anchors.topMargin: Constants.spacing
                width: parent.width
                height: parent.height - datasetSummary.height - Constants.spacing
                color: Constants.surfaceColor
                border.color: Constants.borderColor
                Column {
                    anchors.fill: parent
                    anchors.margins: Constants.spacing * 2
                    spacing: Constants.spacing
                    Text { text: qsTr("Results"); font: Constants.headingFont }
                    Rectangle { width: parent.width; height: 92; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { anchors.centerIn: parent; text: qsTr("Training Loss / Validation Loss") } }
                    Rectangle { width: parent.width; height: 92; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { anchors.centerIn: parent; text: qsTr("Validation Accuracy") } }
                    Row {
                        width: parent.width
                        spacing: Constants.spacing
                        Rectangle { width: (parent.width - parent.spacing) / 2; height: 72; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { anchors.centerIn: parent; text: qsTr("Overall results") } }
                        Rectangle { width: (parent.width - parent.spacing) / 2; height: 72; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { anchors.centerIn: parent; text: qsTr("Per-class results") } }
                    }
                }
            }
        }

        Rectangle {
            id: operationPanel
            width: Constants.operationPanelWidth
            height: parent.height
            color: Constants.surfaceColor
            border.color: Constants.borderColor

            Column {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Constants.spacing
                spacing: 2

                CollapsibleSection {
                    id: trainingSetupSection
                    visible: !root.showRunning && !root.showCompleted && !root.showError
                    width: parent.width
                    sectionTitle: qsTr("Training Setup")
                    expanded: root.trainingSetupExpanded
                    useIntrinsicBodyHeight: true

                    Item {
                        width: parent.width
                        height: trainingSetupContent.implicitHeight + Constants.spacing * 2
                        Column {
                            id: trainingSetupContent
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.margins: Constants.spacing
                            spacing: Constants.spacing
                            Text { text: qsTr("Model Type") }
                            Row { spacing: Constants.spacing; Button { id: fasterButton; text: qsTr("Faster"); checkable: true; checked: true } Button { id: moreAccurateButton; text: qsTr("More Accurate"); checkable: true } }
                            Text { text: qsTr("Compute Device: %1").arg(root.deviceText); color: Constants.mutedTextColor }
                            Text { text: qsTr("Split: 70 / 15 / 15    Seed: 1729"); color: Constants.mutedTextColor }
                            Text { text: qsTr("Model Name") }
                            TextField { id: modelNameField; text: root.modelNameText; height: Constants.controlHeight; width: parent.width }
                            Text { text: qsTr("Save Location") }
                            Row { width: parent.width; spacing: Constants.spacing; TextField { id: saveLocationField; text: root.saveLocationText; height: Constants.controlHeight; width: parent.width - browseButton.width - Constants.spacing } Button { id: browseButton; text: qsTr("Browse"); height: Constants.controlHeight } }
                            Button { id: startButton; text: qsTr("Start Training"); enabled: root.startEnabled; height: Constants.controlHeight; width: parent.width }
                        }
                    }
                }

                CollapsibleSection {
                    id: trainingStatusSection
                    visible: root.showRunning
                    width: parent.width
                    sectionTitle: qsTr("Training Status")
                    expanded: root.trainingStatusExpanded
                    useIntrinsicBodyHeight: true

                    Item {
                        width: parent.width
                        height: trainingStatusContent.implicitHeight + Constants.spacing * 2
                        Column {
                            id: trainingStatusContent
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.margins: Constants.spacing
                            spacing: Constants.spacing
                            Text { text: qsTr("Device: %1").arg(root.deviceText) }
                            Text { text: qsTr("Elapsed: 00:04:12") }
                            Text { text: qsTr("Estimated Remaining: 00:08:36") }
                            Text { text: qsTr("Epoch: 12 of 40") }
                            ProgressBar { value: 0.3; width: parent.width }
                            Text { text: qsTr("Overall progress") }
                            Button { id: stopButton; text: qsTr("Stop Training"); height: Constants.controlHeight; width: parent.width }
                        }
                    }
                }

                Rectangle {
                    visible: root.showError
                    width: parent.width
                    height: 150
                    color: Constants.errorSurfaceColor
                    border.color: Constants.faultColor
                    Column { anchors.fill: parent; anchors.margins: Constants.spacing * 2; spacing: Constants.spacing; Text { text: qsTr("Error"); font: Constants.headingFont; color: Constants.faultColor } Button { id: retrySaveButton; text: qsTr("Retry Save"); height: Constants.controlHeight } }
                }
            }
        }
    }

    states: [
        State { name: "empty"; PropertyChanges { root.presentation: "empty"; root.showRunning: false; root.showError: false; root.showInterrupted: false; root.startEnabled: false; root.datasetText: qsTr("No Dataset selected"); root.disabledReason: qsTr("No dataset selected") } },
        State { name: "unavailable"; PropertyChanges { root.presentation: "unavailable"; root.showInterrupted: false; root.startEnabled: false; root.disabledReason: qsTr("No Labeled Droplet Crops") } },
        State { name: "readyCpu"; PropertyChanges { root.presentation: "readyCpu"; root.showInterrupted: false; root.startEnabled: true; root.deviceText: qsTr("CPU (automatic)"); root.disabledReason: "" } },
        State { name: "readyGpu"; PropertyChanges { root.presentation: "readyGpu"; root.showInterrupted: false; root.startEnabled: true; root.deviceText: qsTr("GPU (automatic)"); root.disabledReason: "" } },
        State { name: "running"; PropertyChanges { root.presentation: "running"; root.showRunning: true; root.showError: false; root.showInterrupted: false; root.deviceText: qsTr("GPU (automatic)") } },
        State { name: "completed"; PropertyChanges { root.presentation: "completed"; root.showRunning: false; root.showError: false; root.showInterrupted: false } },
        State { name: "interrupted"; PropertyChanges { root.presentation: "interrupted"; root.showRunning: false; root.showError: false; root.showInterrupted: true; root.startEnabled: false } },
        State { name: "error"; PropertyChanges { root.presentation: "error"; root.showRunning: false; root.showError: true; root.showInterrupted: false } }
    ]
}
