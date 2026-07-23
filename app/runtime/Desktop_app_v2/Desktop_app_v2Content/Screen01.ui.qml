/* This is a UI file (.ui.qml) intended for Qt Design Studio editing. */
import QtQuick
import QtQuick.Controls.Basic
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
    property bool hardwareActionEnabled: true
    property bool captureStartsAvailable: true
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
    property alias labelWorkspace: labelWorkspace
    property alias sequenceViewerWorkspace: sequenceViewerWorkspace
    property alias trainWorkspace: trainWorkspace
    property alias modelTestWorkspace: modelTestWorkspace
    property alias modelLibraryWorkspace: modelLibraryWorkspace
    property alias liveWorkspace: liveWorkspace
    property alias sequenceTestWorkspace: sequenceTestWorkspace
    property alias runsWorkspace: runsWorkspace
    property alias settingsWorkspace: settingsWorkspace

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
            Button { id: singleImageNavigationButton; text: qsTr("Capture"); width: parent.width; height: Constants.navigationItemHeight; checkable: true; autoExclusive: true; checked: root.selectedWorkspace === "capture" }
            Button { id: labelNavigationButton; text: qsTr("Label"); width: parent.width; height: Constants.navigationItemHeight; checkable: true; autoExclusive: true; checked: root.selectedWorkspace === "label" }
            Button { id: sequenceViewerNavigationButton; text: qsTr("Sequence Viewer"); width: parent.width; height: Constants.navigationItemHeight; checkable: true; autoExclusive: true; checked: root.selectedWorkspace === "sequenceViewer" }
            Text { text: qsTr("Models"); font: Constants.headingFont }
            Button { id: trainNavigationButton; text: qsTr("Train"); width: parent.width; height: Constants.navigationItemHeight; checkable: true; autoExclusive: true; checked: root.selectedWorkspace === "train" }
            Button { id: modelTestNavigationButton; text: qsTr("Model Test"); width: parent.width; height: Constants.navigationItemHeight; checkable: true; autoExclusive: true; checked: root.selectedWorkspace === "modelTest" }
            Button { id: libraryNavigationButton; text: qsTr("Library"); width: parent.width; height: Constants.navigationItemHeight; checkable: true; autoExclusive: true; checked: root.selectedWorkspace === "library" }
            Text { text: qsTr("Sort"); font: Constants.headingFont }
            Button { id: liveNavigationButton; text: qsTr("Live"); width: parent.width; height: Constants.navigationItemHeight; checkable: true; autoExclusive: true; checked: root.selectedWorkspace === "live" }
            Button { id: sequenceTestNavigationButton; text: qsTr("Sequence Test"); width: parent.width; height: Constants.navigationItemHeight; checkable: true; autoExclusive: true; checked: root.selectedWorkspace === "sequenceTest" }
            Text { text: qsTr("Results"); font: Constants.headingFont }
            Button { id: runsNavigationButton; text: qsTr("Runs"); width: parent.width; height: Constants.navigationItemHeight; checkable: true; autoExclusive: true; checked: root.selectedWorkspace === "runs" }
            Button { id: settingsNavigationButton; text: qsTr("Settings"); width: parent.width; height: Constants.navigationItemHeight; checkable: true; autoExclusive: true; checked: root.selectedWorkspace === "settings" }
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
            Text { id: captureWorkspaceTitle; text: qsTr("Capture"); font: Constants.largeFont; color: Constants.textColor; height: Constants.controlHeight; verticalAlignment: Text.AlignVCenter; anchors.left: parent.left; anchors.top: parent.top }
            Rectangle { id: cameraPreview; color: Constants.viewerColor; border.color: Constants.borderColor; anchors.top: captureWorkspaceTitle.bottom; anchors.topMargin: Constants.spacing; anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: capturePanel.left; anchors.rightMargin: Constants.spacing; Text { text: root.cameraStatus === qsTr("Unavailable") ? qsTr("Camera unavailable") : qsTr("Camera preview"); color: Constants.surfaceColor; font: Constants.largeFont; anchors.centerIn: parent } }
            Rectangle {
                id: capturePanel
                width: Constants.operationPanelWidth
                color: Constants.surfaceColor
                border.color: Constants.borderColor
                anchors.top: captureWorkspaceTitle.bottom
                anchors.topMargin: Constants.spacing
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                ScrollView {
                    anchors.fill: parent
                    anchors.margins: Constants.spacing
                    clip: true
                    contentWidth: availableWidth
                    contentHeight: captureSections.height

                    Column {
                        id: captureSections
                        width: parent.width
                        height: implicitHeight
                        spacing: 2
                        CollapsibleSection {
                        id: singleImageSection
                        sectionTitle: qsTr("Single Image")
                        expanded: root.singleImageOpen
                        headingEnabled: !root.otherCaptureHeadingsDisabled || root.singleImagePresentation === "capturing"
                        width: parent.width
                        useIntrinsicBodyHeight: true
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
                        id: imageSequenceSection; sectionTitle: qsTr("Image Sequence"); expanded: root.imageSequenceOpen; headingEnabled: !root.otherCaptureHeadingsDisabled || root.sequencePresentation === "running" || root.sequencePresentation === "paused"; width: parent.width; useIntrinsicBodyHeight: true
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
                            Button { id: sequenceStartButton; visible: root.sequencePresentation === "ready"; text: qsTr("Start Recording"); enabled: root.sequencePresentation === "ready" && root.cameraStatus === qsTr("Streaming") && root.captureStartsAvailable && !root.otherCaptureHeadingsDisabled; width: parent.width; height: Constants.controlHeight }
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
                        id: datasetCaptureSection; sectionTitle: qsTr("Droplet Dataset Capture"); expanded: root.datasetOpen; headingEnabled: !root.otherCaptureHeadingsDisabled || root.datasetPresentation === "running" || root.datasetPresentation === "paused"; width: parent.width; useIntrinsicBodyHeight: true
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
                            Button { id: datasetStartButton; visible: root.datasetPresentation === "ready"; text: qsTr("Start Droplet Dataset Capture"); enabled: root.datasetPresentation === "ready" && root.cameraStatus === qsTr("Streaming") && root.captureStartsAvailable && !root.otherCaptureHeadingsDisabled; width: parent.width; height: Constants.controlHeight }
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
        }

        LabelWorkspace {
            id: labelWorkspace
            anchors.fill: parent
            visible: root.selectedWorkspace === "label"
        }
        SequenceViewerWorkspace {
            id: sequenceViewerWorkspace
            anchors.fill: parent
            visible: root.selectedWorkspace === "sequenceViewer"
        }
        TrainWorkspace {
            id: trainWorkspace
            anchors.fill: parent
            visible: root.selectedWorkspace === "train"
        }
        ModelTestWorkspace {
            id: modelTestWorkspace
            anchors.fill: parent
            visible: root.selectedWorkspace === "modelTest"
        }
        ModelLibraryWorkspace {
            id: modelLibraryWorkspace
            anchors.fill: parent
            visible: root.selectedWorkspace === "library"
        }
        LiveWorkspace {
            id: liveWorkspace
            anchors.fill: parent
            visible: root.selectedWorkspace === "live"
        }
        SequenceTestWorkspace {
            id: sequenceTestWorkspace
            anchors.fill: parent
            visible: root.selectedWorkspace === "sequenceTest"
        }
        RunsWorkspace {
            id: runsWorkspace
            anchors.fill: parent
            visible: root.selectedWorkspace === "runs"
        }
        SettingsWorkspace {
            id: settingsWorkspace
            anchors.fill: parent
            visible: root.selectedWorkspace === "settings"
        }

        Item {
            id: hardwareOverlay
            width: Constants.hardwarePanelWidth
            height: hardwareButton.height + (root.drawerOpen ? hardwareBody.height : 0)
            anchors.left: parent.left
            anchors.leftMargin: Constants.workspaceMargin
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Constants.workspaceMargin
            Button {
                id: hardwareButton
                text: qsTr("Hardware")
                enabled: root.hardwareActionEnabled
                width: parent.width
                height: 42
                focusPolicy: Qt.StrongFocus
                Accessible.name: qsTr("Open or close Hardware panel")
                background: Rectangle {
                    color: root.hardwareActionEnabled ? Constants.backgroundColor : "#e6e8eb"
                    border.color: hardwareButton.activeFocus ? Constants.accentColor : Constants.borderColor
                    border.width: hardwareButton.activeFocus ? 2 : 1
                }
                contentItem: Item {
                    Text { text: (root.drawerOpen ? "⌄  " : "›  ") + hardwareButton.text; color: root.hardwareActionEnabled ? Constants.textColor : Constants.mutedTextColor; font: Constants.headingFont; anchors.left: parent.left; anchors.leftMargin: Constants.spacing; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: root.drawerOpen ? qsTr("Expanded") : qsTr("Collapsed"); color: Constants.mutedTextColor; font: Constants.smallFont; anchors.right: parent.right; anchors.rightMargin: Constants.spacing; anchors.verticalCenter: parent.verticalCenter }
                }
            }
            Rectangle {
                id: hardwareBody
                visible: root.drawerOpen
                width: parent.width
                height: Constants.hardwarePanelHeight
                color: Constants.surfaceColor
                border.color: Constants.borderColor
                anchors.top: hardwareButton.bottom
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
