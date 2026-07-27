pragma ComponentBehavior: Bound
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
    property var libraryModelOptions: []
    property var libraryModelCompatibility: ({
        "hasCompatibleModels": true,
        "reasons": []
    })
    property string selectedLibraryModelName: ""
    property string selectedLibraryModelArchitecture: ""
    property string selectedLibraryModelStartingWeights: ""
    property string saveLocationText: ""
    property string resultPath: qsTr("C:/OpenDSS/Models/DropletNet-04.opendssmodel")
    property string modelOnnxPath: ""
    property string metadataPath: ""
    property bool startEnabled: false
    property bool showRunning: false
    property bool showCompleted: false
    property bool showError: false
    property bool showInterrupted: false
    property string requestedDeviceText: ""
    property string effectiveDeviceText: ""
    property string stageText: ""
    property string elapsedText: qsTr("00:04:12")
    property string remainingText: qsTr("00:08:36")
    property int currentEpoch: 12
    property int totalEpochs: 40
    property real overallProgress: 0.3
    property string errorText: qsTr("Error")
    property var lossSeries: []
    property var accuracySeries: []
    property var resultMetrics: []
    property bool showMetrics: true
    property bool showTiming: true
    property bool showActiveModelConfirmation: true
    property bool showRetrySave: true
    property bool serviceFactsOnly: false
    property bool operationPanelExpanded: true
    property bool trainingSetupExpanded: true
    property bool trainingStatusExpanded: true
    property alias selectDatasetButton: selectDatasetButton
    property alias libraryModelSelector: libraryModelSelector
    property alias libraryModelNameText: libraryModelNameText
    property alias libraryModelArchitectureText: libraryModelArchitectureText
    property alias libraryModelStartingWeightsText: libraryModelStartingWeightsText
    property alias saveLocationField: saveLocationField
    property alias browseButton: browseButton
    property alias startButton: startButton
    property alias stopButton: stopButton
    property alias retrySaveButton: retrySaveButton
    property alias openInModelTestButton: openInModelTestButton
    property alias trainingDeviceSelector: trainingDeviceSelector
    property alias datasetClassesPlaceholder: datasetClassesPlaceholder
    property alias eligibleCropsPlaceholder: eligibleCropsPlaceholder
    property alias lossPlotHost: lossPlotHost
    property alias accuracyPlotHost: accuracyPlotHost
    property alias overallResultsHost: overallResultsHost
    property alias perClassResultsHost: perClassResultsHost
    property alias macroF1Host: macroF1Host
    property alias perClassAccuracyHost: perClassAccuracyHost
    property alias operationPanelToggleButton: operationPanelToggleButton
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

    SplitView {
        font: Constants.font
        anchors.top: headingColumn.bottom
        anchors.topMargin: Constants.spacing
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Constants.workspaceMargin
        anchors.rightMargin: Constants.workspaceMargin
        anchors.bottomMargin: Constants.workspaceMargin

        Item {
            SplitView.fillWidth: true

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
                    Text { text: root.datasetText; font: Constants.appBodyFont; wrapMode: Text.WordWrap; width: parent.width }
                    Text { id: datasetClassesPlaceholder; visible: !root.serviceFactsOnly; text: qsTr("Classes: 2 or 3"); font: Constants.appCaptionFont; color: Constants.mutedTextColor }
                    Text { id: eligibleCropsPlaceholder; visible: !root.serviceFactsOnly; text: qsTr("Eligible labeled crops: factual count"); font: Constants.appCaptionFont; color: Constants.mutedTextColor }
                    AppButton { id: selectDatasetButton; text: qsTr("Select Dataset"); height: Constants.appStandardControlHeight }
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
                    Flow {
                        visible: root.showMetrics
                        width: parent.width
                        height: childrenRect.height
                        spacing: Constants.spacing
                        Rectangle {
                            id: lossPlotHost
                            width: parent.width >= Math.round(760 * Constants.textScale) ? (parent.width - parent.spacing) / 2 : parent.width
                            height: Math.round(180 * Constants.textScale)
                            color: Constants.backgroundColor
                            border.color: Constants.borderColor
                            Text { text: qsTr("Training Loss / Validation Loss"); font: Constants.headingFont; width: parent.width - Constants.spacing * 2; wrapMode: Text.WordWrap; anchors.top: parent.top; anchors.left: parent.left; anchors.margins: Constants.spacing }
                            Rectangle { width: 1; color: Constants.mutedTextColor; anchors.top: parent.top; anchors.topMargin: Constants.spacing * 4; anchors.bottom: parent.bottom; anchors.bottomMargin: Constants.spacing * 2; anchors.left: parent.left; anchors.leftMargin: Constants.spacing * 3 }
                            Rectangle { height: 1; color: Constants.mutedTextColor; anchors.left: parent.left; anchors.leftMargin: Constants.spacing * 3; anchors.right: parent.right; anchors.rightMargin: Constants.spacing; anchors.bottom: parent.bottom; anchors.bottomMargin: Constants.spacing * 2 }
                            Column { anchors.fill: parent; anchors.topMargin: Constants.spacing * 5; anchors.leftMargin: Constants.spacing * 3; anchors.rightMargin: Constants.spacing; anchors.bottomMargin: Constants.spacing * 2; spacing: Math.max(8, height / 4)
                                Repeater { model: 3; Rectangle { required property int index; width: parent.width; height: 1; color: Constants.borderColor } }
                            }
                            Text { text: qsTr("Waiting for training data"); color: Constants.mutedTextColor; font: Constants.smallFont; anchors.centerIn: parent }
                        }
                        Rectangle {
                            id: accuracyPlotHost
                            width: parent.width >= Math.round(760 * Constants.textScale) ? (parent.width - parent.spacing) / 2 : parent.width
                            height: Math.round(180 * Constants.textScale)
                            color: Constants.backgroundColor
                            border.color: Constants.borderColor
                            Text { text: qsTr("Validation Accuracy"); font: Constants.headingFont; width: parent.width - Constants.spacing * 2; wrapMode: Text.WordWrap; anchors.top: parent.top; anchors.left: parent.left; anchors.margins: Constants.spacing }
                            Rectangle { width: 1; color: Constants.mutedTextColor; anchors.top: parent.top; anchors.topMargin: Constants.spacing * 4; anchors.bottom: parent.bottom; anchors.bottomMargin: Constants.spacing * 2; anchors.left: parent.left; anchors.leftMargin: Constants.spacing * 3 }
                            Rectangle { height: 1; color: Constants.mutedTextColor; anchors.left: parent.left; anchors.leftMargin: Constants.spacing * 3; anchors.right: parent.right; anchors.rightMargin: Constants.spacing; anchors.bottom: parent.bottom; anchors.bottomMargin: Constants.spacing * 2 }
                            Column { anchors.fill: parent; anchors.topMargin: Constants.spacing * 5; anchors.leftMargin: Constants.spacing * 3; anchors.rightMargin: Constants.spacing; anchors.bottomMargin: Constants.spacing * 2; spacing: Math.max(8, height / 4)
                                Repeater { model: 3; Rectangle { required property int index; width: parent.width; height: 1; color: Constants.borderColor } }
                            }
                            Text { text: qsTr("Waiting for training data"); color: Constants.mutedTextColor; font: Constants.smallFont; anchors.centerIn: parent }
                        }
                    }
                    Row {
                        visible: root.showCompleted && root.showMetrics
                        width: parent.width
                        spacing: Constants.spacing
                        Rectangle { id: overallResultsHost; width: (parent.width - parent.spacing) / 2; height: 52; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { anchors.centerIn: parent; text: qsTr("Overall results"); font: Constants.appBodyFont } }
                        Rectangle { id: perClassResultsHost; width: (parent.width - parent.spacing) / 2; height: 52; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { anchors.centerIn: parent; text: qsTr("Per-class results"); font: Constants.appBodyFont } }
                    }
                    Row {
                        visible: root.showCompleted && root.showMetrics
                        width: parent.width
                        spacing: Constants.spacing
                        Rectangle { id: macroF1Host; width: (parent.width - parent.spacing) / 2; height: 52; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { anchors.centerIn: parent; text: qsTr("Macro F1"); font: Constants.appBodyFont } }
                        Rectangle { id: perClassAccuracyHost; width: (parent.width - parent.spacing) / 2; height: 52; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { anchors.centerIn: parent; text: qsTr("Per-class validation accuracy"); font: Constants.appBodyFont } }
                    }
                    Text { visible: root.showCompleted; text: root.serviceFactsOnly ? qsTr("Training output: %1").arg(root.resultPath) : qsTr("Saved: %1").arg(root.resultPath); font: Constants.appCaptionFont; wrapMode: Text.WordWrap; width: parent.width }
                    Text { visible: root.showCompleted && (root.showActiveModelConfirmation || root.modelOnnxPath !== "" || root.metadataPath !== ""); text: root.showActiveModelConfirmation ? qsTr("Active Model confirmed") : qsTr("Model: %1\nMetadata: %2").arg(root.modelOnnxPath).arg(root.metadataPath); font: Constants.appCaptionFont; color: root.showActiveModelConfirmation ? Constants.readyColor : Constants.textColor; wrapMode: Text.WordWrap; width: parent.width }
                    AppButton { id: openInModelTestButton; visible: root.showCompleted && root.showActiveModelConfirmation; text: qsTr("Open in Model Test"); height: Constants.appStandardControlHeight }
                }
            }
        }

        Rectangle {
            id: operationPanel
            SplitView.preferredWidth: Constants.operationPanelWidth
            SplitView.minimumWidth: Constants.collapsedOperationPanelWidth
            SplitView.maximumWidth: root.operationPanelExpanded ? Math.max(Constants.collapsedOperationPanelWidth, parent.width * 0.75) : Constants.collapsedOperationPanelWidth
            color: Constants.surfaceColor
            border.color: Constants.borderColor

            Rectangle {
                id: panelTopStrip
                height: Constants.controlHeight
                color: Constants.backgroundColor
                border.color: Constants.borderColor
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                Text {
                    text: qsTr("Train")
                    visible: root.operationPanelExpanded
                    font: Constants.headingFont
                    color: Constants.textColor
                    anchors.left: parent.left
                    anchors.leftMargin: Constants.spacing
                    anchors.right: parent.right
                    anchors.rightMargin: operationPanelToggleButton.width + Constants.spacing
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                }
            }
            AppInspectorRail {
                id: operationPanelToggleButton
                text: root.operationPanelExpanded ? "›" : "‹"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                z: 1
            }

            Column {
                visible: root.operationPanelExpanded
                anchors.top: panelTopStrip.bottom
                anchors.topMargin: Constants.spacing
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Constants.spacing
                anchors.leftMargin: Constants.spacing
                spacing: 2

                AppAccordion {
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
                            Text { text: qsTr("Library Model"); font: Constants.appLabelFont }
                            AppComboBox {
                                id: libraryModelSelector
                                width: parent.width
                                height: Constants.appStandardControlHeight
                                enabled: root.libraryModelOptions.length > 0
                                model: root.libraryModelOptions
                                delegate: ItemDelegate {
                                    required property int index
                                    required property var modelData
                                    readonly property string compatibilityReason:
                                        index < root.libraryModelCompatibility.reasons.length
                                        ? root.libraryModelCompatibility.reasons[index]
                                        : ""
                                    width: libraryModelSelector.width
                                    enabled: compatibilityReason === ""
                                    highlighted: libraryModelSelector.highlightedIndex === index
                                    text: compatibilityReason === ""
                                          ? modelData
                                          : qsTr("%1 — %2").arg(modelData).arg(compatibilityReason)
                                }
                            }
                            Text {
                                visible: root.libraryModelOptions.length === 0
                                      || !root.libraryModelCompatibility.hasCompatibleModels
                                text: root.libraryModelOptions.length === 0
                                      ? qsTr("No Library models are available")
                                      : qsTr("No compatible Library models are available")
                                font: Constants.appCaptionFont
                                color: Constants.mutedTextColor
                                wrapMode: Text.WordWrap
                                width: parent.width
                            }
                            Text { text: qsTr("Name"); font: Constants.appLabelFont }
                            Text {
                                id: libraryModelNameText
                                text: root.selectedLibraryModelName
                                font: Constants.appCaptionFont
                                color: Constants.mutedTextColor
                                wrapMode: Text.WordWrap
                                width: parent.width
                            }
                            Text { text: qsTr("Architecture"); font: Constants.appLabelFont }
                            Text {
                                id: libraryModelArchitectureText
                                text: root.selectedLibraryModelArchitecture
                                font: Constants.appCaptionFont
                                color: Constants.mutedTextColor
                                wrapMode: Text.WordWrap
                                width: parent.width
                            }
                            Text { text: qsTr("Starting Weights"); font: Constants.appLabelFont }
                            Text {
                                id: libraryModelStartingWeightsText
                                text: root.selectedLibraryModelStartingWeights
                                font: Constants.appCaptionFont
                                color: Constants.mutedTextColor
                                wrapMode: Text.WordWrap
                                width: parent.width
                            }
                            Text { text: qsTr("Compute Device"); font: Constants.appLabelFont }
                            AppComboBox {
                                id: trainingDeviceSelector
                                width: parent.width
                                height: Constants.appStandardControlHeight
                                enabled: true
                                model: [qsTr("GPU"), qsTr("CPU")]
                                currentIndex: 0
                            }
                            Text {
                                text: qsTr("Training uses a qualified configuration. Split: 70% Training, 15% Validation, 15% Internal Test. Seed: 1729.")
                                font: Constants.appCaptionFont
                                color: Constants.mutedTextColor
                                wrapMode: Text.WordWrap
                                width: parent.width
                            }
                            Text { text: qsTr("Output Location"); font: Constants.appLabelFont }
                            Row { width: parent.width; spacing: Constants.spacing; AppTextField { id: saveLocationField; text: root.saveLocationText; height: Constants.appStandardControlHeight; width: parent.width - browseButton.width - Constants.spacing } AppButton { id: browseButton; text: qsTr("Browse"); height: Constants.appStandardControlHeight } }
                            AppButton { id: startButton; text: qsTr("Start Training"); visualRole: "primary"; enabled: root.startEnabled; height: Constants.appPrimaryButtonHeight; width: parent.width }
                        }
                    }
                }

                AppAccordion {
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
                            Text { text: root.serviceFactsOnly ? root.effectiveDeviceText === "" ? qsTr("Requested device: %1").arg(root.requestedDeviceText !== "" ? root.requestedDeviceText : root.deviceText) : qsTr("Device: requested %1; effective %2").arg(root.requestedDeviceText).arg(root.effectiveDeviceText) : root.requestedDeviceText === "" ? qsTr("Device: %1").arg(root.effectiveDeviceText !== "" ? root.effectiveDeviceText : root.deviceText) : qsTr("Device: requested %1; effective %2").arg(root.requestedDeviceText).arg(root.effectiveDeviceText !== "" ? root.effectiveDeviceText : root.deviceText); font: Constants.appCaptionFont }
                            Text { visible: root.showTiming; text: qsTr("Elapsed: %1").arg(root.elapsedText); font: Constants.appCaptionFont }
                            Text { visible: root.showTiming; text: qsTr("Estimated Remaining: %1").arg(root.remainingText); font: Constants.appCaptionFont }
                            Text { text: root.totalEpochs > 0 ? qsTr("Epoch: %1 of %2").arg(root.currentEpoch).arg(root.totalEpochs) : qsTr("Epoch: %1").arg(root.currentEpoch); font: Constants.appCaptionFont }
                            AppProgressBar { value: root.overallProgress; width: parent.width }
                            Text { text: root.stageText === "" ? qsTr("Overall progress") : qsTr("Stage: %1").arg(root.stageText); font: Constants.appCaptionFont }
                            AppButton { id: stopButton; text: qsTr("Stop Training"); visualRole: "destructive"; height: Constants.appPrimaryButtonHeight; width: parent.width }
                        }
                    }
                }

                Rectangle {
                    visible: root.showError
                    width: parent.width
                    height: 150
                    color: Constants.errorSurfaceColor
                    border.color: Constants.faultColor
                    Column { anchors.fill: parent; anchors.margins: Constants.spacing * 2; spacing: Constants.spacing; Text { text: root.errorText; font: Constants.headingFont; color: Constants.faultColor } AppButton { id: retrySaveButton; visible: root.showRetrySave; text: qsTr("Retry Save"); height: Constants.appStandardControlHeight } }
                }
            }
        }
    }

    states: [
        State { name: "empty"; PropertyChanges { root.presentation: "empty"; root.showRunning: false; root.showCompleted: false; root.showError: false; root.showInterrupted: false; root.startEnabled: false; root.datasetText: qsTr("No Dataset selected"); root.disabledReason: qsTr("No dataset selected") } },
        State { name: "unavailable"; PropertyChanges { root.presentation: "unavailable"; root.showInterrupted: false; root.startEnabled: false; root.disabledReason: qsTr("No Labeled Droplet Crops") } },
        State { name: "readyCpu"; PropertyChanges { root.presentation: "readyCpu"; root.showInterrupted: false; root.startEnabled: true; root.deviceText: qsTr("CPU (automatic)"); root.disabledReason: "" } },
        State { name: "readyGpu"; PropertyChanges { root.presentation: "readyGpu"; root.showInterrupted: false; root.startEnabled: true; root.deviceText: qsTr("GPU (automatic)"); root.disabledReason: "" } },
        State { name: "running"; PropertyChanges { root.presentation: "running"; root.showRunning: true; root.showCompleted: false; root.showError: false; root.showInterrupted: false; root.deviceText: qsTr("GPU (automatic)") } },
        State { name: "completed"; PropertyChanges { root.presentation: "completed"; root.showRunning: false; root.showCompleted: true; root.showError: false; root.showInterrupted: false } },
        State { name: "interrupted"; PropertyChanges { root.presentation: "interrupted"; root.showRunning: false; root.showCompleted: false; root.showError: false; root.showInterrupted: true; root.startEnabled: false } },
        State { name: "error"; PropertyChanges { root.presentation: "error"; root.showRunning: false; root.showCompleted: false; root.showError: true; root.showInterrupted: false } }
    ]
}
