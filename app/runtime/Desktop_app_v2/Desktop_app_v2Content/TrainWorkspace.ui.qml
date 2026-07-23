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
    property string resultPath: qsTr("C:/OpenDSS/Models/DropletNet-04.opendssmodel")
    property bool startEnabled: false
    property bool showRunning: false
    property bool showCompleted: false
    property bool showError: false
    property alias selectDatasetButton: selectDatasetButton
    property alias fasterButton: fasterButton
    property alias moreAccurateButton: moreAccurateButton
    property alias modelNameField: modelNameField
    property alias saveLocationField: saveLocationField
    property alias browseButton: browseButton
    property alias startButton: startButton
    property alias stopButton: stopButton
    property alias retrySaveButton: retrySaveButton
    property alias openInModelTestButton: openInModelTestButton

    Column {
        anchors.fill: parent
        anchors.margins: Constants.workspaceMargin
        spacing: Constants.spacing

        Text { text: qsTr("Train"); font: Constants.largeFont; color: Constants.textColor }
        Text { text: root.disabledReason; visible: root.presentation === "empty" || root.presentation === "unavailable"; color: Constants.warningColor; font: Constants.smallFont }

        Row {
            visible: !root.showRunning && !root.showCompleted && !root.showError
            width: parent.width
            height: parent.height - 70
            spacing: Constants.spacing
            Rectangle {
                width: (parent.width - Constants.spacing) / 2
                height: parent.height
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
                width: (parent.width - Constants.spacing) / 2
                height: parent.height
                color: Constants.surfaceColor
                border.color: Constants.borderColor
                Column {
                    anchors.fill: parent
                    anchors.margins: Constants.spacing * 2
                    spacing: Constants.spacing
                    Text { text: qsTr("Training Setup"); font: Constants.headingFont }
                    Text { text: qsTr("Model Type") }
                    Row { spacing: Constants.spacing; Button { id: fasterButton; text: qsTr("Faster"); checkable: true; checked: true } Button { id: moreAccurateButton; text: qsTr("More Accurate"); checkable: true } }
                    Text { text: qsTr("Compute Device: ") + root.deviceText; color: Constants.mutedTextColor }
                    Text { text: qsTr("Split: 70 / 15 / 15    Seed: 1729"); color: Constants.mutedTextColor }
                    Text { text: qsTr("Model Name") }
                    TextField { id: modelNameField; text: root.modelNameText; height: Constants.controlHeight; width: parent.width }
                    Text { text: qsTr("Save Location") }
                    Row { width: parent.width; spacing: Constants.spacing; TextField { id: saveLocationField; text: root.saveLocationText; height: Constants.controlHeight; width: parent.width - browseButton.width - Constants.spacing } Button { id: browseButton; text: qsTr("Browse"); height: Constants.controlHeight } }
                    Button { id: startButton; text: qsTr("Start Training"); enabled: root.startEnabled; height: Constants.controlHeight; width: parent.width }
                }
            }
        }

        Row {
            visible: root.showRunning
            width: parent.width
            height: parent.height - 70
            spacing: Constants.spacing
            Rectangle {
                width: (parent.width - Constants.spacing) / 2; height: parent.height; color: Constants.surfaceColor; border.color: Constants.borderColor
                Column { anchors.fill: parent; anchors.margins: Constants.spacing * 2; spacing: Constants.spacing; Text { text: qsTr("Training Metrics"); font: Constants.headingFont } Rectangle { width: parent.width; height: 150; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { anchors.centerIn: parent; text: qsTr("Training Loss / Validation Loss") } } Rectangle { width: parent.width; height: 150; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { anchors.centerIn: parent; text: qsTr("Validation Accuracy") } } }
            }
            Rectangle {
                width: (parent.width - Constants.spacing) / 2; height: parent.height; color: Constants.surfaceColor; border.color: Constants.borderColor
                Column { anchors.fill: parent; anchors.margins: Constants.spacing * 2; spacing: Constants.spacing; Text { text: qsTr("Training Status"); font: Constants.headingFont } Text { text: qsTr("Device: ") + root.deviceText } Text { text: qsTr("Elapsed: 00:04:12") } Text { text: qsTr("Estimated Remaining: 00:08:36") } Text { text: qsTr("Epoch: 12 of 40") } ProgressBar { value: 0.3; width: parent.width } Text { text: qsTr("Overall progress") } Button { id: stopButton; text: qsTr("Stop Training"); height: Constants.controlHeight; width: parent.width } }
            }
        }

        Rectangle {
            visible: root.showCompleted; width: parent.width; height: parent.height - 70; color: Constants.surfaceColor; border.color: Constants.borderColor
            Column { anchors.fill: parent; anchors.margins: Constants.spacing * 2; spacing: Constants.spacing; Text { text: qsTr("Training completed"); font: Constants.headingFont } Text { text: qsTr("Overall results"); font: Constants.headingFont } Text { text: qsTr("Accuracy  0.94    Samples  1,200") } Text { text: qsTr("Per-class results"); font: Constants.headingFont } Text { text: qsTr("Class 0  0.95    Class 1  0.93") } Text { text: qsTr("Confusion matrix"); font: Constants.headingFont } Text { text: qsTr("Saved: ") + root.resultPath; wrapMode: Text.WordWrap; width: parent.width } Text { text: qsTr("Active Model confirmed"); color: Constants.readyColor } Button { id: openInModelTestButton; text: qsTr("Open in Model Test"); height: Constants.controlHeight } }
        }

        Rectangle {
            visible: root.showError; width: parent.width; height: 150; color: Constants.errorSurfaceColor; border.color: Constants.faultColor
            Column { anchors.fill: parent; anchors.margins: Constants.spacing * 2; spacing: Constants.spacing; Text { text: qsTr("Error"); font: Constants.headingFont; color: Constants.faultColor } Text { text: qsTr("The model package was not saved.") } Button { id: retrySaveButton; text: qsTr("Retry Save"); height: Constants.controlHeight } }
        }
    }

    states: [
        State { name: "empty"; PropertyChanges { root.presentation: "empty"; root.showRunning: false; root.showCompleted: false; root.showError: false; root.startEnabled: false; root.datasetText: qsTr("No Dataset selected"); root.disabledReason: qsTr("No dataset selected") } },
        State { name: "unavailable"; PropertyChanges { root.presentation: "unavailable"; root.startEnabled: false; root.disabledReason: qsTr("No Labeled Droplet Crops") } },
        State { name: "readyCpu"; PropertyChanges { root.presentation: "readyCpu"; root.startEnabled: true; root.deviceText: qsTr("CPU (automatic)"); root.disabledReason: "" } },
        State { name: "readyGpu"; PropertyChanges { root.presentation: "readyGpu"; root.startEnabled: true; root.deviceText: qsTr("GPU (automatic)"); root.disabledReason: "" } },
        State { name: "running"; PropertyChanges { root.presentation: "running"; root.showRunning: true; root.showCompleted: false; root.showError: false; root.deviceText: qsTr("GPU (automatic)") } },
        State { name: "completed"; PropertyChanges { root.presentation: "completed"; root.showRunning: false; root.showCompleted: true; root.showError: false } },
        State { name: "error"; PropertyChanges { root.presentation: "error"; root.showRunning: false; root.showCompleted: false; root.showError: true } }
    ]
}
