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
    property string activeModelText: qsTr("No Active Model")
    property string datasetText: qsTr("No Dataset selected")
    property string deviceText: qsTr("CPU (automatic)")
    property string outputLocationText: ""
    property string blockerText: qsTr("No Active Model")
    property bool startEnabled: false
    property bool showRunning: false
    property bool showCompleted: false
    property bool showError: false
    property bool threeClassResult: false
    property alias selectDatasetButton: selectDatasetButton
    property alias outputLocationField: outputLocationField
    property alias browseButton: browseButton
    property alias startButton: startButton
    property alias stopButton: stopButton
    property alias startAnotherButton: startAnotherButton

    Column {
        anchors.fill: parent
        anchors.margins: Constants.workspaceMargin
        spacing: Constants.spacing
        Text { text: qsTr("Model Test"); font: Constants.largeFont; color: Constants.textColor; height: Constants.controlHeight; verticalAlignment: Text.AlignVCenter }
        Rectangle {
            width: parent.width; height: 100; color: Constants.surfaceColor; border.color: Constants.borderColor
            Column { anchors.fill: parent; anchors.margins: Constants.spacing; spacing: 4; Text { text: qsTr("Active Model (read-only)"); font: Constants.headingFont } Text { text: root.activeModelText } Text { text: qsTr("Dataset: ") + root.datasetText } }
        }
        Rectangle {
            visible: !root.showRunning && !root.showCompleted && !root.showError; width: parent.width; height: 210; color: Constants.surfaceColor; border.color: Constants.borderColor
            Column { anchors.fill: parent; anchors.margins: Constants.spacing * 2; spacing: Constants.spacing; Text { text: qsTr("Test Setup"); font: Constants.headingFont } Button { id: selectDatasetButton; text: qsTr("Select Dataset"); height: Constants.controlHeight } Text { text: qsTr("Output Location") } Row { width: parent.width; spacing: Constants.spacing; TextField { id: outputLocationField; text: root.outputLocationText; width: parent.width - browseButton.width - Constants.spacing; height: Constants.controlHeight } Button { id: browseButton; text: qsTr("Browse"); height: Constants.controlHeight } } Text { visible: !root.startEnabled; text: root.blockerText; color: Constants.warningColor } Text { visible: root.startEnabled; text: qsTr("Device: ") + root.deviceText; color: Constants.mutedTextColor } Button { id: startButton; text: qsTr("Start Model Test"); enabled: root.startEnabled; height: Constants.controlHeight } }
        }
        Rectangle {
            visible: root.showRunning; width: parent.width; height: 230; color: Constants.surfaceColor; border.color: Constants.borderColor
            Column { anchors.fill: parent; anchors.margins: Constants.spacing * 2; spacing: Constants.spacing; Text { text: qsTr("Model Test Running"); font: Constants.headingFont } Text { text: qsTr("Device: ") + root.deviceText } Text { text: qsTr("Processed: 360 of 1,200") } ProgressBar { value: 0.3; width: parent.width } Button { id: stopButton; text: qsTr("Stop Model Test"); height: Constants.controlHeight } }
        }
        Rectangle {
            visible: root.showCompleted; width: parent.width; height: 300; color: Constants.surfaceColor; border.color: Constants.borderColor
            Column { anchors.fill: parent; anchors.margins: Constants.spacing * 2; spacing: Constants.spacing; Text { text: qsTr("Model Test Completed"); font: Constants.headingFont } Text { text: qsTr("Overall Accuracy: 0.94") } Text { text: root.threeClassResult ? qsTr("Per-Class Accuracy: Class 0 0.95    Class 1 0.93    Class 2 0.92") : qsTr("Per-Class Accuracy: Class 0 0.95    Class 1 0.93") } Text { text: root.threeClassResult ? qsTr("Confusion Matrix: 3 classes") : qsTr("Confusion Matrix: 2 classes") } Text { text: qsTr("Output: C:/OpenDSS/ModelTests/Test-042") } Row { spacing: Constants.spacing; Button { id: openPredictionsButton; text: qsTr("Open Predictions CSV") } Button { id: openSummaryButton; text: qsTr("Open Summary") } Button { id: startAnotherButton; text: qsTr("Start Another") } } }
        }
        Rectangle {
            visible: root.showError; width: parent.width; height: 130; color: Constants.errorSurfaceColor; border.color: Constants.faultColor
            Column { anchors.fill: parent; anchors.margins: Constants.spacing * 2; spacing: Constants.spacing; Text { text: qsTr("Error"); font: Constants.headingFont; color: Constants.faultColor } Text { text: root.presentation === "interrupted" ? qsTr("Model Test was interrupted.") : root.blockerText } Button { text: qsTr("Start Model Test"); enabled: root.presentation === "interrupted" } }
        }
    }

    states: [
        State { name: "empty"; PropertyChanges { root.presentation: "empty"; root.activeModelText: qsTr("No Active Model"); root.datasetText: qsTr("No Dataset selected"); root.startEnabled: false; root.showRunning: false; root.showCompleted: false; root.showError: false; root.blockerText: qsTr("No Active Model") } },
        State { name: "modelOnly"; PropertyChanges { root.presentation: "modelOnly"; root.activeModelText: qsTr("DropletNet-04"); root.datasetText: qsTr("No Dataset selected"); root.startEnabled: false; root.blockerText: qsTr("No dataset selected") } },
        State { name: "datasetOnly"; PropertyChanges { root.presentation: "datasetOnly"; root.activeModelText: qsTr("No Active Model"); root.datasetText: qsTr("Dataset-042"); root.startEnabled: false; root.blockerText: qsTr("No Active Model") } },
        State { name: "classMismatch"; PropertyChanges { root.presentation: "classMismatch"; root.activeModelText: qsTr("DropletNet-04 (2 classes)"); root.datasetText: qsTr("Dataset-042 (3 classes)"); root.startEnabled: false; root.blockerText: qsTr("The selected model has 2 output classes, but the selected Dataset defines 3 classes") } },
        State { name: "noLabeled"; PropertyChanges { root.presentation: "noLabeled"; root.activeModelText: qsTr("DropletNet-04"); root.datasetText: qsTr("Dataset-042"); root.startEnabled: false; root.blockerText: qsTr("No Labeled Droplet Crops") } },
        State { name: "readyCpu"; PropertyChanges { root.presentation: "readyCpu"; root.activeModelText: qsTr("DropletNet-04"); root.datasetText: qsTr("Dataset-042"); root.deviceText: qsTr("CPU (automatic)"); root.startEnabled: true; root.blockerText: "" } },
        State { name: "readyGpu"; PropertyChanges { root.presentation: "readyGpu"; root.activeModelText: qsTr("DropletNet-04"); root.datasetText: qsTr("Dataset-042"); root.deviceText: qsTr("GPU (automatic)"); root.startEnabled: true; root.blockerText: "" } },
        State { name: "running"; PropertyChanges { root.presentation: "running"; root.showRunning: true; root.showCompleted: false; root.showError: false; root.deviceText: qsTr("GPU (automatic)") } },
        State { name: "completedTwoClass"; PropertyChanges { root.presentation: "completedTwoClass"; root.showRunning: false; root.showCompleted: true; root.showError: false; root.threeClassResult: false } },
        State { name: "completedThreeClass"; PropertyChanges { root.presentation: "completedThreeClass"; root.showRunning: false; root.showCompleted: true; root.showError: false; root.threeClassResult: true } },
        State { name: "interrupted"; PropertyChanges { root.presentation: "interrupted"; root.showRunning: false; root.showCompleted: false; root.showError: true } },
        State { name: "error"; PropertyChanges { root.presentation: "error"; root.showRunning: false; root.showCompleted: false; root.showError: true; root.blockerText: qsTr("Output folder is not writable") } }
    ]
}
