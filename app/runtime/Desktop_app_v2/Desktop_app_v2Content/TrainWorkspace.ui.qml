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
    property string modelNameText: ""
    property string saveLocationText: ""
    property string resultPath: qsTr("C:/OpenDSS/Models/DropletNet-04.opendssmodel")
    property bool startEnabled: false
    property bool showRunning: false
    property bool showCompleted: false
    property bool showError: false
    property bool showInterrupted: false
    property bool operationPanelExpanded: true
    property bool trainingSetupExpanded: true
    property bool trainingStatusExpanded: true
    property alias selectDatasetButton: selectDatasetButton
    property alias modelNameField: modelNameField
    property alias saveLocationField: saveLocationField
    property alias browseButton: browseButton
    property alias startButton: startButton
    property alias stopButton: stopButton
    property alias retrySaveButton: retrySaveButton
    property alias openInModelTestButton: openInModelTestButton
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
                    Flow {
                        width: parent.width
                        height: childrenRect.height
                        spacing: Constants.spacing
                        Rectangle {
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
                        visible: root.showCompleted
                        width: parent.width
                        spacing: Constants.spacing
                        Rectangle { width: (parent.width - parent.spacing) / 2; height: 52; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { anchors.centerIn: parent; text: qsTr("Overall results") } }
                        Rectangle { width: (parent.width - parent.spacing) / 2; height: 52; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { anchors.centerIn: parent; text: qsTr("Per-class results") } }
                    }
                    Row {
                        visible: root.showCompleted
                        width: parent.width
                        spacing: Constants.spacing
                        Rectangle { width: (parent.width - parent.spacing) / 2; height: 52; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { anchors.centerIn: parent; text: qsTr("Macro F1") } }
                        Rectangle { width: (parent.width - parent.spacing) / 2; height: 52; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { anchors.centerIn: parent; text: qsTr("Per-class validation accuracy") } }
                    }
                    Text { visible: root.showCompleted; text: qsTr("Saved: %1").arg(root.resultPath); wrapMode: Text.WordWrap; width: parent.width }
                    Text { visible: root.showCompleted; text: qsTr("Active Model confirmed"); color: Constants.readyColor }
                    Button { id: openInModelTestButton; visible: root.showCompleted; text: qsTr("Open in Model Test"); height: Constants.controlHeight }
                }
            }
        }

        Rectangle {
            id: operationPanel
            SplitView.preferredWidth: Constants.operationPanelWidth
            SplitView.minimumWidth: Constants.collapsedOperationPanelWidth
            SplitView.maximumWidth: root.operationPanelExpanded ? parent.width * 0.75 : Constants.collapsedOperationPanelWidth
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
                    anchors.right: operationPanelToggleButton.left
                    anchors.rightMargin: Constants.spacing
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                }
            }
            Button {
                id: operationPanelToggleButton
                text: root.operationPanelExpanded ? "›" : "‹"
                width: Math.round(30 * Constants.textScale)
                height: panelTopStrip.height
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                z: 1
                background: Rectangle { color: Constants.backgroundColor; border.color: operationPanelToggleButton.activeFocus ? Constants.accentColor : Constants.borderColor; border.width: operationPanelToggleButton.activeFocus ? 2 : 1 }
                contentItem: Text { text: operationPanelToggleButton.text; color: Constants.textColor; font: Constants.headingFont; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
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
                            Text { text: qsTr("Architecture"); font: Constants.font }
                            ComboBox {
                                id: architectureSelector
                                width: parent.width
                                height: Math.round(54 * Constants.textScale)
                                enabled: true
                                model: [qsTr("MobileNet"), qsTr("EfficientNet")]
                                background: Rectangle {
                                    color: Constants.surfaceColor
                                    border.color: architectureSelector.activeFocus ? Constants.accentColor : Constants.borderColor
                                    border.width: architectureSelector.activeFocus ? 2 : 1
                                }
                                contentItem: Item {
                                    Text {
                                        id: selectedArchitectureName
                                        text: architectureSelector.currentText
                                        color: Constants.textColor
                                        font: Constants.font
                                        anchors.left: parent.left
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Text {
                                        text: architectureSelector.currentIndex === 0 ? qsTr("— Faster") : qsTr("— More Accurate")
                                        color: Constants.mutedTextColor
                                        font: Constants.smallFont
                                        anchors.left: selectedArchitectureName.right
                                        anchors.leftMargin: Constants.spacing
                                        anchors.right: parent.right
                                        anchors.rightMargin: Math.round(28 * Constants.textScale)
                                        anchors.verticalCenter: parent.verticalCenter
                                        elide: Text.ElideRight
                                    }
                                }
                                delegate: ItemDelegate {
                                    id: architectureOptionDelegate
                                    required property int index
                                    required property string modelData
                                    width: architectureSelector.width
                                    height: Math.round(54 * Constants.textScale)
                                    highlighted: architectureSelector.highlightedIndex === index
                                    hoverEnabled: true
                                    contentItem: Item {
                                        Text {
                                            id: architectureOptionName
                                            text: architectureOptionDelegate.modelData
                                            color: Constants.textColor
                                            font: Constants.font
                                            anchors.left: parent.left
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        Text {
                                            text: architectureOptionDelegate.index === 0 ? qsTr("— Faster") : qsTr("— More Accurate")
                                            color: Constants.mutedTextColor
                                            font: Constants.smallFont
                                            anchors.left: architectureOptionName.right
                                            anchors.leftMargin: Constants.spacing
                                            anchors.right: parent.right
                                            anchors.verticalCenter: parent.verticalCenter
                                            elide: Text.ElideRight
                                        }
                                    }
                                    background: Rectangle {
                                        color: architectureOptionDelegate.index === architectureSelector.currentIndex || architectureOptionDelegate.hovered || architectureOptionDelegate.highlighted ? Constants.backgroundColor : Constants.surfaceColor
                                        border.color: Constants.borderColor
                                    }
                                }
                            }
                            Text { text: qsTr("Weights"); font: Constants.font }
                            ComboBox {
                                id: weightsSelector
                                width: parent.width
                                height: Constants.controlHeight
                                enabled: true
                                model: [qsTr("ImageNet-pretrained"), qsTr("OpenDSS droplet checkpoint — bundled"), qsTr("OpenDSS droplet checkpoint — user-added")]
                                contentItem: Text {
                                    text: weightsSelector.currentText
                                    color: Constants.textColor
                                    font: Constants.font
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                    leftPadding: Constants.spacing
                                    rightPadding: Math.round(28 * Constants.textScale)
                                }
                                delegate: ItemDelegate {
                                    id: weightsOptionDelegate
                                    required property int index
                                    required property string modelData
                                    width: weightsSelector.width
                                    height: Constants.controlHeight
                                    highlighted: weightsSelector.highlightedIndex === index
                                    hoverEnabled: true
                                    contentItem: Text {
                                        text: weightsOptionDelegate.modelData
                                        color: Constants.textColor
                                        font: Constants.font
                                        verticalAlignment: Text.AlignVCenter
                                        elide: Text.ElideRight
                                    }
                                    background: Rectangle {
                                        color: weightsOptionDelegate.index === weightsSelector.currentIndex || weightsOptionDelegate.hovered || weightsOptionDelegate.highlighted ? Constants.backgroundColor : Constants.surfaceColor
                                        border.color: Constants.borderColor
                                    }
                                }
                            }
                            Button { id: loadWeightsButton; text: qsTr("Load Weights"); width: parent.width; height: Constants.controlHeight }
                            Text { text: qsTr("Compute Device"); font: Constants.font }
                            ComboBox {
                                id: trainingDeviceSelector
                                width: parent.width
                                enabled: true
                                model: [qsTr("GPU"), qsTr("CPU")]
                                currentIndex: 0
                            }
                            Text { text: qsTr("Model Name"); font: Constants.font }
                            TextField { id: modelNameField; text: root.modelNameText; height: Constants.controlHeight; width: parent.width }
                            Text { text: qsTr("Save Location"); font: Constants.font }
                            Row { width: parent.width; spacing: Constants.spacing; TextField { id: saveLocationField; text: root.saveLocationText; height: Constants.controlHeight; width: parent.width - browseButton.width - Constants.spacing } Button { id: browseButton; text: qsTr("Browse"); height: Constants.controlHeight } }
                            Button { id: startButton; text: qsTr("Start Training"); enabled: root.startEnabled; height: Constants.controlHeight; width: parent.width; palette.buttonText: Constants.surfaceColor; background: Rectangle { color: startButton.enabled ? Constants.accentColor : Constants.borderColor } }
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
                            Button { id: stopButton; text: qsTr("Stop Training"); height: Constants.controlHeight; width: parent.width; palette.buttonText: Constants.surfaceColor; background: Rectangle { color: Constants.faultColor } }
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
