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
    property bool operationPanelExpanded: true
    property bool modelTestSetupExpanded: true
    property bool modelTestStatusExpanded: true
    property alias selectDatasetButton: selectDatasetButton
    property alias outputLocationField: outputLocationField
    property alias browseButton: browseButton
    property alias startButton: startButton
    property alias stopButton: stopButton
    property alias openPredictionsButton: openPredictionsButton
    property alias openSummaryButton: openSummaryButton
    property alias startAnotherButton: startAnotherButton
    property alias operationPanelToggleButton: operationPanelToggleButton
    property alias modelTestSetupHeadingButton: modelTestSetupSection.headingButton
    property alias modelTestStatusHeadingButton: modelTestStatusSection.headingButton

    Text {
        id: workspaceTitle
        text: qsTr("Model Test")
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
                Column { anchors.fill: parent; anchors.margins: Constants.spacing * 2; spacing: Constants.spacing; Text { text: qsTr("Dataset Summary"); font: Constants.headingFont } Text { text: qsTr("Active Model (read-only): ") + root.activeModelText } Text { text: qsTr("Dataset: ") + root.datasetText } }
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
                    Row {
                        width: parent.width
                        spacing: Constants.spacing
                        Rectangle { width: (parent.width - parent.spacing) / 2; height: 92; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { anchors.centerIn: parent; text: qsTr("Overall Accuracy") } }
                        Rectangle { width: (parent.width - parent.spacing) / 2; height: 92; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { anchors.centerIn: parent; text: qsTr("Per-Class Accuracy") } }
                    }
                    Rectangle { width: parent.width; height: 112; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { anchors.centerIn: parent; text: root.threeClassResult ? qsTr("Confusion Matrix (3 classes)") : qsTr("Confusion Matrix (2 classes)") } }
                    Rectangle { width: parent.width; height: 72; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { anchors.centerIn: parent; text: qsTr("Prediction summary") } }
                    Row {
                        visible: root.showCompleted
                        spacing: Constants.spacing
                        Button { id: openPredictionsButton; text: qsTr("Open Predictions CSV"); height: Constants.controlHeight }
                        Button { id: openSummaryButton; text: qsTr("Open Summary"); height: Constants.controlHeight }
                        Button { id: startAnotherButton; text: qsTr("Start Another"); height: Constants.controlHeight }
                    }
                }
            }
        }

        Rectangle {
            id: operationPanel
            width: root.operationPanelExpanded ? Constants.operationPanelWidth : Constants.collapsedOperationPanelWidth
            height: parent.height
            color: Constants.surfaceColor
            border.color: Constants.borderColor

            Button {
                id: operationPanelToggleButton
                text: root.operationPanelExpanded ? qsTr("‹ Model Test panel") : qsTr("›")
                width: parent.width - Constants.spacing * 2
                height: Constants.controlHeight
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: Constants.spacing
            }

            Column {
                visible: root.operationPanelExpanded
                anchors.top: operationPanelToggleButton.bottom
                anchors.topMargin: Constants.spacing
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Constants.spacing
                spacing: 2

                CollapsibleSection {
                    id: modelTestSetupSection
                    visible: !root.showRunning && !root.showCompleted && !root.showError
                    width: parent.width
                    sectionTitle: qsTr("Test Setup")
                    expanded: root.modelTestSetupExpanded
                    useIntrinsicBodyHeight: true

                    Item {
                        width: parent.width
                        height: modelTestSetupContent.implicitHeight + Constants.spacing * 2
                        Column {
                            id: modelTestSetupContent
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.margins: Constants.spacing
                            spacing: Constants.spacing
                            Button { id: selectDatasetButton; text: qsTr("Select Dataset"); height: Constants.controlHeight }
                            Text { text: qsTr("Output Location") }
                            Row { width: parent.width; spacing: Constants.spacing; TextField { id: outputLocationField; text: root.outputLocationText; width: parent.width - browseButton.width - Constants.spacing; height: Constants.controlHeight } Button { id: browseButton; text: qsTr("Browse"); height: Constants.controlHeight } }
                            Text { visible: !root.startEnabled; text: root.blockerText; color: Constants.warningColor }
                            Text { visible: root.startEnabled; text: qsTr("Device: ") + root.deviceText; color: Constants.mutedTextColor }
                            Button { id: startButton; text: qsTr("Start Model Test"); enabled: root.startEnabled; height: Constants.controlHeight }
                        }
                    }
                }

                CollapsibleSection {
                    id: modelTestStatusSection
                    visible: root.showRunning
                    width: parent.width
                    sectionTitle: qsTr("Model Test Running")
                    expanded: root.modelTestStatusExpanded
                    useIntrinsicBodyHeight: true

                    Item {
                        width: parent.width
                        height: modelTestStatusContent.implicitHeight + Constants.spacing * 2
                        Column {
                            id: modelTestStatusContent
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.margins: Constants.spacing
                            spacing: Constants.spacing
                            Text { text: qsTr("Device: ") + root.deviceText }
                            Text { text: qsTr("Processed: 360 of 1,200") }
                            ProgressBar { value: 0.3; width: parent.width }
                            Button { id: stopButton; text: qsTr("Stop Model Test"); height: Constants.controlHeight }
                        }
                    }
                }

                Rectangle {
                    visible: root.showError
                    width: parent.width
                    height: 130
                    color: Constants.errorSurfaceColor
                    border.color: Constants.faultColor
                    Column { anchors.fill: parent; anchors.margins: Constants.spacing * 2; spacing: Constants.spacing; Text { text: qsTr("Error"); font: Constants.headingFont; color: Constants.faultColor } Text { text: root.presentation === "interrupted" ? qsTr("Model Test was interrupted.") : root.blockerText } Button { text: qsTr("Start Model Test"); enabled: root.presentation === "interrupted" } }
                }
            }
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
