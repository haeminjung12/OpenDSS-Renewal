/* This is a UI file (.ui.qml) intended for Qt Design Studio editing. */
import QtQuick
import QtQuick.Controls
import Desktop_app_v2

Rectangle {
    id: root
    width: Constants.width
    height: Constants.height
    color: Constants.backgroundColor
    property string cameraStatus: qsTr("Unavailable")
    property string daqStatus: qsTr("Ready")
    property string activeModelText: qsTr("No Active Model")
    property string activityText: qsTr("Idle")
    property string fileNameText: ""
    property string saveLocationText: qsTr("C:/OpenDSS/Images")
    property string disabledReason: qsTr("Camera unavailable")
    property string savedPath: ""
    property string bannerHeading: qsTr("Error")
    property string bannerText: ""
    property bool captureEnabled: false
    property bool showSavedPath: false
    property bool showBanner: false
    property bool drawerOpen: false
    property string selectedWorkspace: "capture"
    property string singleImagePresentation: "unavailable"
    property bool singleImageOpen: false
    property bool imageSequenceOpen: false
    property bool datasetOpen: false
    property bool otherCaptureHeadingsDisabled: false
    property bool cameraPromptVisible: true
    property string cameraPromptChoice: ""
    property string sequencePresentation: "ready"
    property string datasetPresentation: "ready"
    property int sequenceFrameCount: 0
    property int datasetFrameCount: 0
    property int datasetCropCount: 0
    property bool cameraLocked: false
    readonly property real captureBodyHeight: (captureSections.height
                                                - singleImageSection.headingButton.height
                                                - imageSequenceSection.headingButton.height
                                                - datasetCaptureSection.headingButton.height
                                                - captureSections.spacing * 2)
                                               / Math.max(1, (singleImageOpen ? 1 : 0)
                                                             + (imageSequenceOpen ? 1 : 0)
                                                             + (datasetOpen ? 1 : 0))
    property string cameraResolution: ""
    property string cameraCustomWidth: ""
    property string cameraCustomHeight: ""
    property string cameraBitDepth: ""
    property string cameraExposure: ""
    property string cameraReadoutMode: ""
    property string cameraLut: ""
    property string daqDevice: ""
    property string daqOutputChannel: ""
    property string sequenceLocationText: ""
    property string datasetLocationText: ""
    property string datasetHandoffText: ""
    property alias hardwareButton: hardwareButton
    property alias fileNameField: fileNameField
    property alias saveLocationField: saveLocationField
    property alias browseButton: browseButton
    property alias captureButton: captureButton
    property alias drawerCloseButton: drawerCloseButton
    property alias navCaptureButton: singleImageNavigationButton
    property alias navLabelButton: labelNavigationButton
    property alias navSequenceViewerButton: sequenceViewerNavigationButton
    property alias navTrainButton: trainNavigationButton
    property alias navModelTestButton: modelTestNavigationButton
    property alias navLibraryButton: libraryNavigationButton
    property alias navLiveButton: liveNavigationButton
    property alias navSequenceTestButton: sequenceTestNavigationButton
    property alias navRunsButton: runsNavigationButton
    property alias navSettingsButton: settingsNavigationButton
    property alias singleImageSection: singleImageSection
    property alias imageSequenceSection: imageSequenceSection
    property alias datasetCaptureSection: datasetCaptureSection
    property alias cameraPromptYesButton: cameraPromptYesButton
    property alias cameraPromptNoButton: cameraPromptNoButton
    property alias restoreCameraButton: restoreCameraButton
    property alias sequenceStartButton: sequenceStartButton
    property alias datasetStartButton: datasetStartButton
    property alias capturePauseButton: capturePauseButton
    property alias captureStopButton: captureStopButton
    property alias datasetPauseButton: datasetPauseButton
    property alias datasetStopButton: datasetStopButton
    property alias sequenceLocationField: sequenceLocationField
    property alias datasetLocationField: datasetLocationField
    property alias sequenceBrowseButton: sequenceBrowseButton
    property alias datasetBrowseButton: datasetBrowseButton
    property alias startCameraButton: startCameraButton
    property alias cameraDeviceSelector: cameraDeviceSelector
    property alias cameraResolutionSelector: cameraResolutionSelector
    property alias cameraCustomWidthField: cameraCustomWidthField
    property alias cameraCustomHeightField: cameraCustomHeightField
    property alias cameraExposureField: cameraExposureField
    property alias cameraLutSelector: cameraLutSelector
    property alias daqChannelSelector: daqChannelSelector
    property alias sequenceViewerButton: sequenceViewerButton
    property alias sequenceTestButton: sequenceTestButton
    property alias sequenceNewButton: sequenceNewButton
    property alias datasetLabelButton: datasetLabelButton
    property alias datasetFolderButton: datasetFolderButton
    property alias datasetNewButton: datasetNewButton

    Rectangle {
        id: header
        height: Constants.shellHeaderHeight
        color: Constants.surfaceColor
        border.color: Constants.borderColor
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        Row {
            spacing: Constants.headerItemSpacing
            anchors.left: parent.left
            anchors.leftMargin: Constants.spacing * 2
            anchors.verticalCenter: parent.verticalCenter
            Text { text: qsTr("◉  Camera  ") + root.cameraStatus + (root.cameraStatus === qsTr("Unavailable") ? qsTr(" (not ready)") : qsTr(" (ready)")); color: root.cameraStatus === qsTr("Unavailable") ? Constants.warningColor : Constants.readyColor; font: Constants.smallFont }
            Text { text: qsTr("▣  DAQ  ") + root.daqStatus + (root.daqStatus === qsTr("Unavailable") ? qsTr(" (not ready)") : root.daqStatus === qsTr("Active") ? qsTr(" (active)") : qsTr(" (ready)")); color: root.daqStatus === qsTr("Unavailable") ? Constants.warningColor : root.daqStatus === qsTr("Active") ? Constants.accentColor : Constants.readyColor; font: Constants.smallFont }
            Text { text: qsTr("◆  Active Model  ") + root.activeModelText + (root.activeModelText === qsTr("No Active Model") ? qsTr(" (none)") : qsTr(" (active)")); color: root.activeModelText === qsTr("No Active Model") ? Constants.mutedTextColor : Constants.readyColor; font: Constants.smallFont }
            Text { text: qsTr("▶  Current Activity  ") + root.activityText + (root.activityText === qsTr("Idle") ? qsTr(" (idle)") : qsTr(" (active)")); color: root.activityText === qsTr("Idle") ? Constants.textColor : Constants.accentColor; font: Constants.smallFont }
        }
    }

    Rectangle {
        width: Constants.navigationWidth
        color: Constants.surfaceColor
        border.color: Constants.borderColor
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        Column {
            spacing: 3
            anchors.fill: parent
            anchors.margins: Constants.spacing
            Text { text: qsTr("Data"); font: Constants.headingFont }
            Button { id: singleImageNavigationButton; text: qsTr("Capture"); width: parent.width; height: Constants.navigationItemHeight; checkable: true; checked: root.selectedWorkspace === "capture" }
            Button { id: labelNavigationButton; text: qsTr("Label"); width: parent.width; height: Constants.navigationItemHeight; checkable: true; checked: root.selectedWorkspace === "label" }
            Button { id: sequenceViewerNavigationButton; text: qsTr("Sequence Viewer"); width: parent.width; height: Constants.navigationItemHeight; checkable: true; checked: root.selectedWorkspace === "sequenceViewer" }
            Text { text: qsTr("Models"); font: Constants.headingFont }
            Button { id: trainNavigationButton; text: qsTr("Train"); width: parent.width; height: Constants.navigationItemHeight; checkable: true; checked: root.selectedWorkspace === "train" }
            Button { id: modelTestNavigationButton; text: qsTr("Model Test"); width: parent.width; height: Constants.navigationItemHeight; checkable: true; checked: root.selectedWorkspace === "modelTest" }
            Button { id: libraryNavigationButton; text: qsTr("Library"); width: parent.width; height: Constants.navigationItemHeight; checkable: true; checked: root.selectedWorkspace === "library" }
            Text { text: qsTr("Sort"); font: Constants.headingFont }
            Button { id: liveNavigationButton; text: qsTr("Live"); width: parent.width; height: Constants.navigationItemHeight; checkable: true; checked: root.selectedWorkspace === "live" }
            Button { id: sequenceTestNavigationButton; text: qsTr("Sequence Test"); width: parent.width; height: Constants.navigationItemHeight; checkable: true; checked: root.selectedWorkspace === "sequenceTest" }
            Text { text: qsTr("Results"); font: Constants.headingFont }
            Button { id: runsNavigationButton; text: qsTr("Runs"); width: parent.width; height: Constants.navigationItemHeight; checkable: true; checked: root.selectedWorkspace === "runs" }
            Button { id: settingsNavigationButton; text: qsTr("Settings"); width: parent.width; height: Constants.navigationItemHeight; checkable: true; checked: root.selectedWorkspace === "settings" }
        }
    }

    Item {
        id: workspace
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: Constants.navigationWidth
        anchors.right: parent.right

        Item {
            visible: root.selectedWorkspace === "capture"
            anchors.fill: parent
            anchors.margins: Constants.workspaceMargin
            Rectangle { id: cameraPreview; color: Constants.viewerColor; border.color: Constants.borderColor; anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: capturePanel.left; anchors.rightMargin: Constants.spacing; Text { text: root.cameraStatus === qsTr("Unavailable") ? qsTr("Camera unavailable") : qsTr("Camera preview"); color: Constants.surfaceColor; font: Constants.largeFont; anchors.centerIn: parent } }
            Rectangle {
                id: capturePanel
                width: Constants.operationPanelWidth
                color: Constants.surfaceColor
                border.color: Constants.borderColor
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                Column {
                    id: captureSections
                    spacing: 2
                    anchors.fill: parent
                    anchors.margins: Constants.spacing
                    CollapsibleSection {
                        id: singleImageSection
                        sectionTitle: qsTr("Single Image")
                        expanded: root.singleImageOpen
                        headingEnabled: !root.otherCaptureHeadingsDisabled || root.singleImagePresentation === "capturing"
                        width: parent.width
                        bodyHeight: root.captureBodyHeight
                        Column {
                            spacing: 6
                            width: parent.width
                            height: implicitHeight
                            Text { text: qsTr("File Name"); font: Constants.smallFont }
                            TextField { id: fileNameField; text: root.fileNameText; width: parent.width; height: Constants.controlHeight; placeholderText: qsTr("Optional — timestamp used") }
                            Text { text: qsTr("Save Location"); font: Constants.smallFont }
                            Row {
                                spacing: 6
                                width: parent.width
                                TextField { id: saveLocationField; text: root.saveLocationText; width: parent.width - browseButton.width - 6; height: Constants.controlHeight }
                                Button { id: browseButton; text: qsTr("Browse"); height: Constants.controlHeight }
                            }
                            Button { id: captureButton; text: root.singleImagePresentation === "capturing" ? qsTr("Capturing Image…") : qsTr("Capture Image"); enabled: root.captureEnabled; width: parent.width; height: Constants.controlHeight }
                            Text { visible: root.showSavedPath; text: qsTr("Saved: ") + root.savedPath; color: Constants.readyColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            Text { visible: root.disabledReason !== ""; text: root.disabledReason; color: Constants.warningColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            Rectangle { visible: root.singleImagePresentation === "error"; width: parent.width; height: 34; color: Constants.errorSurfaceColor; border.color: Constants.faultColor; Text { text: qsTr("Error"); color: Constants.faultColor; font.bold: true; anchors.centerIn: parent } }
                        }
                    }
                    CollapsibleSection {
                        id: imageSequenceSection; sectionTitle: qsTr("Image Sequence"); expanded: root.imageSequenceOpen; headingEnabled: !root.otherCaptureHeadingsDisabled || root.sequencePresentation === "running" || root.sequencePresentation === "paused"; width: parent.width; bodyHeight: root.captureBodyHeight
                        Column { spacing: 6; width: parent.width; height: implicitHeight
                            Text { text: qsTr("Name"); font: Constants.smallFont }
                            TextField { enabled: root.sequencePresentation !== "running" && root.sequencePresentation !== "paused"; text: qsTr(""); width: parent.width; height: Constants.controlHeight; placeholderText: qsTr("Sequence name") }
                            Text { text: qsTr("Experiment Type"); font: Constants.smallFont }
                            TextField { enabled: root.sequencePresentation !== "running" && root.sequencePresentation !== "paused"; text: qsTr(""); width: parent.width; height: Constants.controlHeight }
                            Text { text: qsTr("Notes"); font: Constants.smallFont }
                            TextArea { enabled: root.sequencePresentation !== "running" && root.sequencePresentation !== "paused"; width: parent.width; height: 58; placeholderText: qsTr("Optional notes") }
                            Text { text: qsTr("Duration (optional — blank continues until Stop)"); font: Constants.smallFont }
                            TextField { enabled: root.sequencePresentation !== "running" && root.sequencePresentation !== "paused"; width: parent.width; height: Constants.controlHeight; placeholderText: qsTr("Optional") }
                            Text { text: qsTr("Save Location"); font: Constants.smallFont }
                            Row { spacing: 6; width: parent.width
                                TextField { id: sequenceLocationField; enabled: root.sequencePresentation !== "running" && root.sequencePresentation !== "paused"; text: root.sequenceLocationText; width: parent.width - sequenceBrowseButton.width - 6; height: Constants.controlHeight }
                                Button { id: sequenceBrowseButton; enabled: root.sequencePresentation !== "running" && root.sequencePresentation !== "paused"; text: qsTr("Browse"); height: Constants.controlHeight }
                            }
                            Button { id: sequenceStartButton; visible: root.sequencePresentation === "ready"; text: qsTr("Start Recording"); enabled: root.sequencePresentation === "ready" && root.cameraStatus === qsTr("Streaming") && !root.otherCaptureHeadingsDisabled; width: parent.width; height: Constants.controlHeight }
                            Row { visible: root.sequencePresentation === "running" || root.sequencePresentation === "paused"; spacing: 6
                                Button { id: capturePauseButton; text: root.sequencePresentation === "paused" ? qsTr("Resume") : qsTr("Pause"); height: Constants.controlHeight }
                                Button { id: captureStopButton; text: qsTr("Stop"); height: Constants.controlHeight }
                            }
                            Text { visible: root.sequencePresentation !== "ready"; text: root.sequencePresentation === "completed" ? qsTr("Completed — 24 frames captured.") : qsTr("Frames captured: ") + root.sequenceFrameCount + (root.sequencePresentation === "paused" ? qsTr(" — Paused") : qsTr(" — Recording")); color: Constants.mutedTextColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            Row { visible: root.sequencePresentation === "completed"; spacing: 4; Button { id: sequenceViewerButton; text: qsTr("Open in Sequence Viewer") } Button { id: sequenceTestButton; text: qsTr("Open in Sequence Test") } }
                            Button { id: sequenceNewButton; visible: root.sequencePresentation === "completed"; text: qsTr("Start New Recording"); width: parent.width; height: Constants.controlHeight }
                        }
                    }
                    CollapsibleSection {
                        id: datasetCaptureSection; sectionTitle: qsTr("Droplet Dataset Capture"); expanded: root.datasetOpen; headingEnabled: !root.otherCaptureHeadingsDisabled || root.datasetPresentation === "running" || root.datasetPresentation === "paused"; width: parent.width; bodyHeight: root.captureBodyHeight
                        Column { spacing: 6; width: parent.width; height: implicitHeight
                            Text { text: qsTr("Dataset Name"); font: Constants.smallFont }
                            TextField { enabled: root.datasetPresentation !== "running" && root.datasetPresentation !== "paused"; width: parent.width; height: Constants.controlHeight; placeholderText: qsTr("Dataset name") }
                            Text { text: qsTr("Experiment Type"); font: Constants.smallFont }
                            TextField { enabled: root.datasetPresentation !== "running" && root.datasetPresentation !== "paused"; width: parent.width; height: Constants.controlHeight }
                            Text { text: qsTr("Notes"); font: Constants.smallFont }
                            TextArea { enabled: root.datasetPresentation !== "running" && root.datasetPresentation !== "paused"; width: parent.width; height: 58; placeholderText: qsTr("Optional notes") }
                            Text { text: qsTr("Duration (optional — blank continues until Stop)"); font: Constants.smallFont }
                            TextField { enabled: root.datasetPresentation !== "running" && root.datasetPresentation !== "paused"; width: parent.width; height: Constants.controlHeight; placeholderText: qsTr("Optional") }
                            Text { text: qsTr("Save Location"); font: Constants.smallFont }
                            Row { spacing: 6; width: parent.width
                                TextField { id: datasetLocationField; enabled: root.datasetPresentation !== "running" && root.datasetPresentation !== "paused"; text: root.datasetLocationText; width: parent.width - datasetBrowseButton.width - 6; height: Constants.controlHeight }
                                Button { id: datasetBrowseButton; enabled: root.datasetPresentation !== "running" && root.datasetPresentation !== "paused"; text: qsTr("Browse"); height: Constants.controlHeight }
                            }
                            Text { text: qsTr("Fixed qualified processing is used; detector, crop, and timing settings are not editable."); color: Constants.mutedTextColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            Button { id: datasetStartButton; visible: root.datasetPresentation === "ready"; text: qsTr("Start Droplet Dataset Capture"); enabled: root.datasetPresentation === "ready" && root.cameraStatus === qsTr("Streaming") && !root.otherCaptureHeadingsDisabled; width: parent.width; height: Constants.controlHeight }
                            Row { visible: root.datasetPresentation === "running" || root.datasetPresentation === "paused"; spacing: 6
                                Button { id: datasetPauseButton; text: root.datasetPresentation === "paused" ? qsTr("Resume") : qsTr("Pause"); height: Constants.controlHeight }
                                Button { id: datasetStopButton; text: qsTr("Stop"); height: Constants.controlHeight }
                            }
                            Text { visible: root.datasetPresentation !== "ready"; text: root.datasetPresentation === "completed" ? qsTr("Completed — 18 frames, 42 crops captured.") : qsTr("Frames: ") + root.datasetFrameCount + qsTr("   Crops: ") + root.datasetCropCount + (root.datasetPresentation === "paused" ? qsTr(" — Paused") : qsTr(" — Capturing")); color: Constants.mutedTextColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            Row { visible: root.datasetPresentation === "completed"; spacing: 4; Button { id: datasetLabelButton; text: qsTr("Open in Label") } Button { id: datasetFolderButton; text: qsTr("Open Folder") } }
                            Button { id: datasetNewButton; visible: root.datasetPresentation === "completed"; text: qsTr("Start New Droplet Dataset Capture"); width: parent.width; height: Constants.controlHeight }
                            Text { visible: root.datasetHandoffText !== ""; text: root.datasetHandoffText; color: Constants.mutedTextColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                        }
                    }
                }
            }
        }

        Item { visible: root.selectedWorkspace === "label"; anchors.fill: parent; anchors.margins: Constants.workspaceMargin
            Rectangle { color: Constants.viewerColor; border.color: Constants.borderColor; anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.left: parent.left; width: parent.width * 0.66; Text { text: qsTr("Droplet Crop collection"); color: Constants.surfaceColor; font: Constants.largeFont; anchors.centerIn: parent } }
            Column { spacing: Constants.spacing; anchors.left: parent.left; anchors.leftMargin: parent.width * 0.68; anchors.right: parent.right; anchors.top: parent.top
                Rectangle { width: parent.width; height: 250; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Selected Crop"); anchors.centerIn: parent } }
                Rectangle { width: parent.width; height: 220; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Classes & Filter"); anchors.centerIn: parent } }
            }
        }
        Item { visible: root.selectedWorkspace === "sequenceViewer"; anchors.fill: parent; anchors.margins: Constants.workspaceMargin
            Column { spacing: Constants.spacing; anchors.fill: parent
                Rectangle { width: parent.width; height: parent.height - 80; color: Constants.viewerColor; border.color: Constants.borderColor; Text { text: qsTr("Still-frame viewer"); color: Constants.surfaceColor; font: Constants.largeFont; anchors.centerIn: parent } }
                Rectangle { width: parent.width; height: 70; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Frame navigation"); anchors.centerIn: parent } }
            }
        }
        Item { visible: root.selectedWorkspace === "train"; anchors.fill: parent; anchors.margins: Constants.workspaceMargin
            Column { spacing: Constants.spacing; anchors.fill: parent
                Rectangle { width: parent.width; height: 150; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Dataset / pre-training"); anchors.centerIn: parent } }
                Row { spacing: Constants.spacing; width: parent.width
                    Rectangle { width: (parent.width - Constants.spacing) / 2; height: 300; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Training progress / plots"); anchors.centerIn: parent } }
                    Rectangle { width: (parent.width - Constants.spacing) / 2; height: 300; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Results"); anchors.centerIn: parent } }
                }
            }
        }
        Item { visible: root.selectedWorkspace === "modelTest"; anchors.fill: parent; anchors.margins: Constants.workspaceMargin
            Column { spacing: Constants.spacing; anchors.fill: parent
                Rectangle { width: parent.width; height: 100; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Active Model / Dataset context (read-only)"); anchors.centerIn: parent } }
                Rectangle { width: parent.width; height: 360; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Results"); anchors.centerIn: parent } }
            }
        }
        Item { visible: root.selectedWorkspace === "library"; anchors.fill: parent; anchors.margins: Constants.workspaceMargin
            Row { spacing: Constants.spacing; anchors.fill: parent
                Rectangle { width: parent.width * 0.42; height: parent.height; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Model list"); anchors.centerIn: parent } }
                Rectangle { width: parent.width * 0.58 - Constants.spacing; height: parent.height; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Selected-model detail"); anchors.centerIn: parent } }
            }
        }
        Item { visible: root.selectedWorkspace === "live"; anchors.fill: parent; anchors.margins: Constants.workspaceMargin
            Rectangle { color: Constants.viewerColor; border.color: Constants.borderColor; anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.left: parent.left; width: parent.width * 0.63; Text { text: qsTr("Camera preview"); color: Constants.surfaceColor; font: Constants.largeFont; anchors.centerIn: parent } }
            Column { spacing: 3; anchors.left: parent.left; anchors.leftMargin: parent.width * 0.65; anchors.right: parent.right; anchors.top: parent.top
                Rectangle { width: parent.width; height: 46; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Setup Profile"); anchors.centerIn: parent } }
                Rectangle { width: parent.width; height: 46; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Run Information"); anchors.centerIn: parent } }
                Rectangle { width: parent.width; height: 46; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Trigger & Timing"); anchors.centerIn: parent } }
                Rectangle { width: parent.width; height: 46; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Output & Recording"); anchors.centerIn: parent } }
                Rectangle { width: parent.width; height: 46; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Running"); anchors.centerIn: parent } }
            }
        }
        Item { visible: root.selectedWorkspace === "sequenceTest"; anchors.fill: parent; anchors.margins: Constants.workspaceMargin
            Column { spacing: Constants.spacing; anchors.fill: parent
                Rectangle { width: parent.width; height: 150; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Sequence / configuration"); anchors.centerIn: parent } }
                Rectangle { width: parent.width; height: 130; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Load / progress"); anchors.centerIn: parent } }
                Rectangle { width: parent.width; height: 200; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Results"); anchors.centerIn: parent } }
            }
        }
        Item { visible: root.selectedWorkspace === "runs"; anchors.fill: parent; anchors.margins: Constants.workspaceMargin
            Rectangle { color: Constants.surfaceColor; border.color: Constants.borderColor; anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: runsPanel.left; anchors.rightMargin: Constants.spacing; Text { text: qsTr("Loaded Run"); font: Constants.largeFont; anchors.centerIn: parent } }
            Rectangle { id: runsPanel; width: Constants.operationPanelWidth; color: Constants.surfaceColor; border.color: Constants.accentColor; anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.right: parent.right; Text { text: qsTr("Runs"); font: Constants.headingFont; anchors.centerIn: parent } }
        }
        Item { visible: root.selectedWorkspace === "settings"; anchors.fill: parent; anchors.margins: Constants.workspaceMargin
            Column { spacing: Constants.spacing; width: parent.width * 0.65; anchors.horizontalCenter: parent.horizontalCenter
                Rectangle { width: parent.width; height: 140; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Storage"); anchors.centerIn: parent } }
                Rectangle { width: parent.width; height: 140; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Application Information"); anchors.centerIn: parent } }
                Rectangle { width: parent.width; height: 140; color: Constants.surfaceColor; border.color: Constants.borderColor; Text { text: qsTr("Diagnostics"); anchors.centerIn: parent } }
            }
        }

        Button { id: hardwareButton; text: root.drawerOpen ? "⌄" : "⌃"; width: 46; height: 34; anchors.left: parent.left; anchors.leftMargin: Constants.workspaceMargin; anchors.bottom: parent.bottom; anchors.bottomMargin: Constants.workspaceMargin; Accessible.name: qsTr("Open or close Hardware panel") }
        Rectangle {
            visible: root.drawerOpen; width: Constants.hardwarePanelWidth; height: Constants.hardwarePanelHeight; color: Constants.surfaceColor; border.color: Constants.borderColor
            anchors.left: parent.left; anchors.leftMargin: Constants.workspaceMargin; anchors.bottom: parent.bottom; anchors.bottomMargin: Constants.workspaceMargin + 36
            ScrollView { anchors.fill: parent; anchors.margins: Constants.spacing; clip: true; contentWidth: availableWidth
                Column { width: parent.width; spacing: Constants.spacing
                    Row { width: parent.width; Text { text: qsTr("Hardware — illustrative mock"); font: Constants.headingFont; width: parent.width - drawerCloseButton.width } Button { id: drawerCloseButton; text: qsTr("Close"); width: 58; height: 30 } }
                    Rectangle { width: parent.width; height: 1; color: Constants.borderColor }
                    Text { text: qsTr("Camera"); font: Constants.headingFont }
                    Text { text: qsTr("Status: ") + root.cameraStatus + (root.cameraLocked ? qsTr(" — locked by current capture") : qsTr(" — mock only")); color: root.cameraStatus === qsTr("Unavailable") ? Constants.warningColor : Constants.readyColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                    Button { id: restoreCameraButton; visible: root.cameraStatus === qsTr("Unavailable"); enabled: !root.cameraLocked; text: qsTr("Restore Camera (mock)"); width: parent.width; height: Constants.controlHeight }
                    Button { id: startCameraButton; enabled: !root.cameraLocked && root.cameraStatus !== qsTr("Unavailable"); text: root.cameraStatus === qsTr("Streaming") ? qsTr("Stop Camera") : qsTr("Start Camera"); width: parent.width; height: Constants.controlHeight }
                    Text { text: qsTr("Device"); font: Constants.smallFont }
                    ComboBox { id: cameraDeviceSelector; enabled: !root.cameraLocked && root.cameraStatus !== qsTr("Unavailable"); model: ["Unavailable", "Illustrative Camera A"]; currentIndex: root.cameraStatus === qsTr("Unavailable") ? 0 : 1; width: parent.width }
                    Text { text: qsTr("Resolution preset"); font: Constants.smallFont }
                    ComboBox { id: cameraResolutionSelector; enabled: !root.cameraLocked && root.cameraStatus !== qsTr("Unavailable"); model: ["1024 × 1024", "2048 × 2048", "Custom"]; currentIndex: root.cameraResolution === "2048 × 2048" ? 1 : root.cameraResolution === "Custom" ? 2 : 0; width: parent.width }
                    Row { visible: root.cameraResolution === "Custom"; spacing: 6
                        TextField { id: cameraCustomWidthField; enabled: !root.cameraLocked && root.cameraStatus !== qsTr("Unavailable"); text: root.cameraCustomWidth; width: (parent.width - 6) / 2; placeholderText: qsTr("Custom Width") }
                        TextField { id: cameraCustomHeightField; enabled: !root.cameraLocked && root.cameraStatus !== qsTr("Unavailable"); text: root.cameraCustomHeight; width: (parent.width - 6) / 2; placeholderText: qsTr("Custom Height") }
                    }
                    Text { text: qsTr("Bit Depth: ") + root.cameraBitDepth; font: Constants.smallFont }
                    Text { text: qsTr("Exposure"); font: Constants.smallFont }
                    TextField { id: cameraExposureField; enabled: !root.cameraLocked && root.cameraStatus !== qsTr("Unavailable"); text: root.cameraExposure; width: parent.width }
                    Text { text: qsTr("Readout Mode: ") + root.cameraReadoutMode; font: Constants.smallFont }
                    Text { text: qsTr("Preview LUT"); font: Constants.smallFont }
                    ComboBox { id: cameraLutSelector; enabled: !root.cameraLocked && root.cameraStatus !== qsTr("Unavailable"); model: ["Linear", "High contrast"]; currentIndex: root.cameraLut === "High contrast" ? 1 : 0; width: parent.width }
                    Rectangle { width: parent.width; height: 1; color: Constants.borderColor }
                    Text { text: qsTr("DAQ"); font: Constants.headingFont }
                    Text { text: qsTr("Status: ") + root.daqStatus + qsTr(" — mock only"); color: Constants.readyColor; font: Constants.smallFont }
                    Text { text: qsTr("Device"); font: Constants.smallFont }
                    Text { text: root.daqDevice; font: Constants.smallFont }
                    Text { text: qsTr("Output Channel"); font: Constants.smallFont }
                    ComboBox { id: daqChannelSelector; model: ["ao0", "ao1"]; currentIndex: root.daqOutputChannel === "ao1" ? 1 : 0; width: parent.width }
                    Text { text: qsTr("Capabilities: analog output; maximum supported voltage range 0–5 V; maximum supported output frequency 10 kHz (illustrative read-only facts)"); color: Constants.mutedTextColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                }
            }
        }
        Rectangle { visible: root.cameraPromptVisible && root.selectedWorkspace === "capture"; width: 430; height: 180; color: Constants.surfaceColor; border.color: Constants.warningColor; anchors.centerIn: parent; z: 2; Column { spacing: Constants.spacing; anchors.fill: parent; anchors.margins: Constants.spacing * 2; Text { text: qsTr("Camera unavailable. Continue?"); font: Constants.headingFont } Text { text: qsTr("Camera unavailable (not ready)"); color: Constants.warningColor; font: Constants.smallFont } Row { spacing: Constants.spacing; Button { id: cameraPromptYesButton; text: qsTr("Yes"); width: 92; height: Constants.controlHeight; checkable: true; checked: root.cameraPromptChoice === "yes" } Button { id: cameraPromptNoButton; text: qsTr("No"); width: 92; height: Constants.controlHeight; checkable: true; checked: root.cameraPromptChoice === "no" } } } }
    }

    states: [
        State { name: "captureAllCollapsed"; PropertyChanges { root.singleImageOpen: false; root.imageSequenceOpen: false; root.datasetOpen: false; root.cameraPromptVisible: false } },
        State { name: "captureSingleImageExpanded"; PropertyChanges { root.singleImageOpen: true; root.imageSequenceOpen: false; root.datasetOpen: false; root.cameraPromptVisible: false } },
        State { name: "captureImageSequenceExpanded"; PropertyChanges { root.singleImageOpen: false; root.imageSequenceOpen: true; root.datasetOpen: false; root.cameraPromptVisible: false } },
        State { name: "captureDatasetExpanded"; PropertyChanges { root.singleImageOpen: false; root.imageSequenceOpen: false; root.datasetOpen: true; root.cameraPromptVisible: false } },
        State { name: "captureMultipleIdleExpanded"; PropertyChanges { root.singleImageOpen: true; root.imageSequenceOpen: true; root.cameraPromptVisible: false } },
        State { name: "captureActiveSectionExpanded"; PropertyChanges { root.singleImageOpen: true; root.otherCaptureHeadingsDisabled: true; root.cameraPromptVisible: false } },
        State { name: "singleImageUnavailable"; PropertyChanges { root.singleImagePresentation: "unavailable"; root.cameraStatus: qsTr("Unavailable"); root.activityText: qsTr("Idle"); root.captureEnabled: false; root.disabledReason: qsTr("Camera unavailable"); root.showSavedPath: false; root.savedPath: ""; root.cameraPromptVisible: false } },
        State { name: "singleImageReady"; PropertyChanges { root.singleImagePresentation: "ready"; root.cameraStatus: qsTr("Streaming"); root.activityText: qsTr("Idle"); root.captureEnabled: true; root.disabledReason: ""; root.showSavedPath: false; root.savedPath: ""; root.cameraPromptVisible: false } },
        State { name: "singleImageCapturing"; PropertyChanges { root.singleImagePresentation: "capturing"; root.cameraStatus: qsTr("Streaming"); root.captureEnabled: false; root.activityText: qsTr("Capturing Image"); root.disabledReason: ""; root.showSavedPath: false; root.savedPath: ""; root.cameraPromptVisible: false } },
        State { name: "singleImageCompleted"; PropertyChanges { root.singleImagePresentation: "completed"; root.cameraStatus: qsTr("Streaming"); root.activityText: qsTr("Idle"); root.captureEnabled: true; root.disabledReason: ""; root.showSavedPath: true; root.savedPath: qsTr("C:/OpenDSS/Images/sample_042.tiff"); root.cameraPromptVisible: false } },
        State { name: "singleImageError"; PropertyChanges { root.singleImagePresentation: "error"; root.cameraStatus: qsTr("Streaming"); root.activityText: qsTr("Idle"); root.captureEnabled: false; root.disabledReason: ""; root.showSavedPath: false; root.savedPath: ""; root.cameraPromptVisible: false } },
        State { name: "hardwareOpen"; PropertyChanges { root.drawerOpen: true; root.cameraPromptVisible: false } },
        State { name: "cameraUnavailablePrompt"; PropertyChanges { root.cameraPromptVisible: true; root.cameraPromptChoice: "" } },
        State { name: "cameraUnavailableYes"; PropertyChanges { root.cameraPromptVisible: true; root.cameraPromptChoice: "yes" } },
        State { name: "cameraUnavailableNo"; PropertyChanges { root.cameraPromptVisible: true; root.cameraPromptChoice: "no" } },
        State { name: "headerUnavailableIdle"; PropertyChanges { root.cameraStatus: qsTr("Unavailable"); root.daqStatus: qsTr("Unavailable"); root.activeModelText: qsTr("No Active Model"); root.activityText: qsTr("Idle"); root.cameraPromptVisible: false } },
        State { name: "headerReadyIdle"; PropertyChanges { root.cameraStatus: qsTr("Streaming"); root.daqStatus: qsTr("Ready"); root.activeModelText: qsTr("No Active Model"); root.activityText: qsTr("Idle"); root.cameraPromptVisible: false } },
        State { name: "headerActiveOperation"; PropertyChanges { root.cameraStatus: qsTr("Streaming"); root.daqStatus: qsTr("Active"); root.activeModelText: qsTr("DropletNet-04"); root.activityText: qsTr("Capturing Image"); root.cameraPromptVisible: false } },
        State { name: "workspaceLabel"; PropertyChanges { root.selectedWorkspace: "label"; root.cameraPromptVisible: false } },
        State { name: "workspaceSequenceViewer"; PropertyChanges { root.selectedWorkspace: "sequenceViewer"; root.cameraPromptVisible: false } },
        State { name: "workspaceTrain"; PropertyChanges { root.selectedWorkspace: "train"; root.cameraPromptVisible: false } },
        State { name: "workspaceModelTest"; PropertyChanges { root.selectedWorkspace: "modelTest"; root.cameraPromptVisible: false } },
        State { name: "workspaceLibrary"; PropertyChanges { root.selectedWorkspace: "library"; root.cameraPromptVisible: false } },
        State { name: "workspaceLive"; PropertyChanges { root.selectedWorkspace: "live"; root.cameraPromptVisible: false } },
        State { name: "workspaceSequenceTest"; PropertyChanges { root.selectedWorkspace: "sequenceTest"; root.cameraPromptVisible: false } },
        State { name: "workspaceRuns"; PropertyChanges { root.selectedWorkspace: "runs"; root.cameraPromptVisible: false } },
        State { name: "workspaceSettings"; PropertyChanges { root.selectedWorkspace: "settings"; root.cameraPromptVisible: false } }
    ]
}
