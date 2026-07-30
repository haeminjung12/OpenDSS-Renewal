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
    property string cameraError: ""
    property string cameraPreviewSource: ""
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
    property bool capturePanelExpanded: true
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
    property string sequenceElapsedTimeText: ""
    property string sequenceCompletionText: ""
    property string sequenceErrorText: ""
    property string datasetElapsedTimeText: ""
    property string datasetCompletionText: ""
    property string datasetErrorText: ""
    property bool cameraLocked: false
    property bool cameraConfigurationAvailable: true
    property string cameraDeviceText: qsTr("Unavailable")
    property string cameraResolution: ""
    property string cameraCustomWidth: ""
    property string cameraCustomHeight: ""
    property string cameraBitDepth: "8-bit"
    property string cameraExposure: ""
    property string cameraReadoutMode: ""
    property string cameraLut: ""
    property string daqDevice: ""
    property string daqOutputChannel: ""
    property bool continuousWaveformActive: false
    property string sequenceLocationText: ""
    property string datasetLocationText: ""
    property string datasetHandoffText: ""
    property bool hardwareActionEnabled: true
    property bool captureStartsAvailable: true
    property int smallDropletRejectionArea: 100
    property bool smallDropletSetEnabled: false
    property string smallDropletSetDisabledReason: qsTr("No frame is available")
    property bool smallDropletSelectionArmed: false
    property bool smallDropletSelectionVisible: false
    property real smallDropletSelectionStartXRatio: 0.0
    property real smallDropletSelectionStartYRatio: 0.0
    property real smallDropletSelectionEndXRatio: 0.0
    property real smallDropletSelectionEndYRatio: 0.0
    property alias hardwareButton: hardwareButton
    property alias capturePanelToggleButton: capturePanelToggleButton
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
    property alias cameraSectionHeadingButton: cameraSection.headingButton
    property alias cameraSectionExpanded: cameraSection.expanded
    property alias daqSectionHeadingButton: daqSection.headingButton
    property alias daqSectionExpanded: daqSection.expanded
    property alias detectorSectionHeadingButton: detectorSection.headingButton
    property alias detectorSectionExpanded: detectorSection.expanded
    property alias smallDropletRejectionValueText: smallDropletRejectionValueText
    property alias smallDropletSetButton: smallDropletSetButton
    property alias smallDropletSelectionInputArea: smallDropletSelectionInputArea
    property alias cameraPromptYesButton: cameraPromptYesButton
    property alias cameraPromptNoButton: cameraPromptNoButton
    property alias restoreCameraButton: restoreCameraButton
    property alias cameraStatusText: cameraStatusText
    property alias cameraErrorText: cameraErrorText
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
    property alias sequenceNameField: sequenceNameField
    property alias sequenceExperimentTypeField: sequenceExperimentTypeField
    property alias sequenceNotesField: sequenceNotesField
    property alias sequenceDurationField: sequenceDurationField
    property alias datasetNameField: datasetNameField
    property alias datasetExperimentTypeField: datasetExperimentTypeField
    property alias datasetNotesField: datasetNotesField
    property alias datasetDurationField: datasetDurationField
    property alias startCameraButton: startCameraButton
    property alias cameraDeviceSelector: cameraDeviceSelector
    property alias cameraResolutionSelector: cameraResolutionSelector
    property alias cameraCustomWidthField: cameraCustomWidthField
    property alias cameraCustomHeightField: cameraCustomHeightField
    property alias cameraPreviewImage: cameraPreviewViewer.image
    property alias cameraPreviewPlaceholder: cameraPreviewViewer.placeholder
    property alias cameraExposureField: cameraExposureField
    property alias cameraBitDepthSelector: cameraBitDepthSelector
    property alias cameraReadoutSelector: cameraReadoutSelector
    property alias cameraLutSelector: cameraLutSelector
    property alias previewLutRangeSlider: previewLutRangeSlider
    property alias daqStatusText: daqStatusText
    property alias daqDeviceText: daqDeviceText
    property alias daqRefreshDevicesButton: daqRefreshDevicesButton
    property alias daqChannelSelector: daqChannelSelector
    property alias daqVppSpinBox: daqVppSpinBox
    property alias daqFrequencySpinBox: daqFrequencySpinBox
    property alias daqEventDurationSpinBox: daqEventDurationSpinBox
    property alias daqDecisionDelaySpinBox: daqDecisionDelaySpinBox
    property alias continuousSineWaveButton: continuousSineWaveButton
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
        ScrollView {
            anchors.fill: parent
            contentWidth: statusRow.implicitWidth + Constants.spacing * 4
            contentHeight: availableHeight
            clip: true

        Row {
            id: statusRow
            spacing: Constants.headerItemSpacing
            anchors.left: parent.left
            anchors.leftMargin: Constants.spacing * 2
            anchors.verticalCenter: parent.verticalCenter
            Text { text: qsTr("●  Camera  ") + root.cameraStatus; color: root.cameraStatus === qsTr("Unavailable") ? Constants.warningColor : Constants.readyColor; font: Constants.smallFont }
            Text { text: qsTr("●  DAQ  ") + root.daqStatus; color: root.daqStatus === qsTr("Unavailable") ? Constants.warningColor : root.daqStatus === qsTr("Active") ? Constants.accentColor : Constants.readyColor; font: Constants.smallFont }
            Text { text: qsTr("●  Model  ") + root.activeModelText; color: root.activeModelText === qsTr("No Active Model") ? Constants.mutedTextColor : Constants.readyColor; font: Constants.smallFont }
            Text { text: qsTr("●  Activity  ") + root.activityText; color: root.activityText === qsTr("Idle") ? Constants.textColor : Constants.accentColor; font: Constants.smallFont }
        }
        }
    }

    SplitView {
        id: shellSplit
        font: Constants.font
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right

    Rectangle {
        id: navigationPanel
        SplitView.preferredWidth: Constants.navigationWidth
        SplitView.minimumWidth: Constants.navigationWidth * 0.65
        SplitView.maximumWidth: Math.max(Constants.navigationWidth * 0.65, parent.width * 0.4)
        color: Constants.backgroundColor
        border.color: Constants.borderColor
        SplitView {
            id: navigationHardwareSplit
            anchors.fill: parent
            orientation: Qt.Vertical
            handle: Rectangle {
                implicitHeight: 6
                color: Constants.borderColor
            }
        ScrollView {
            id: navigationScroll
            SplitView.fillHeight: true
            SplitView.minimumHeight: Math.round(180 * Constants.textScale)
            topPadding: Constants.spacing
            bottomPadding: Constants.spacing
            leftPadding: Constants.spacing
            rightPadding: Constants.spacing
            contentWidth: availableWidth
            clip: true

        Column {
            width: navigationScroll.availableWidth
            height: implicitHeight
            spacing: 3
            Text { text: qsTr("Data"); font: Constants.headingFont }
            AppNavigationItem { id: singleImageNavigationButton; text: qsTr("Capture"); width: parent.width; height: Constants.appStandardControlHeight; selected: root.selectedWorkspace === "capture"; checkable: true; autoExclusive: true; checked: root.selectedWorkspace === "capture" }
            AppNavigationItem { id: labelNavigationButton; text: qsTr("Label"); width: parent.width; height: Constants.appStandardControlHeight; selected: root.selectedWorkspace === "label"; checkable: true; autoExclusive: true; checked: root.selectedWorkspace === "label" }
            AppNavigationItem { id: sequenceViewerNavigationButton; text: qsTr("Sequence Viewer"); width: parent.width; height: Constants.appStandardControlHeight; selected: root.selectedWorkspace === "sequenceViewer"; checkable: true; autoExclusive: true; checked: root.selectedWorkspace === "sequenceViewer" }
            Text { text: qsTr("Models"); font: Constants.headingFont }
            AppNavigationItem { id: trainNavigationButton; text: qsTr("Train"); width: parent.width; height: Constants.appStandardControlHeight; selected: root.selectedWorkspace === "train"; checkable: true; autoExclusive: true; checked: root.selectedWorkspace === "train" }
            AppNavigationItem { id: modelTestNavigationButton; text: qsTr("Model Test"); width: parent.width; height: Constants.appStandardControlHeight; selected: root.selectedWorkspace === "modelTest"; checkable: true; autoExclusive: true; checked: root.selectedWorkspace === "modelTest" }
            AppNavigationItem { id: libraryNavigationButton; text: qsTr("Library"); width: parent.width; height: Constants.appStandardControlHeight; selected: root.selectedWorkspace === "library"; checkable: true; autoExclusive: true; checked: root.selectedWorkspace === "library" }
            Text { text: qsTr("Sort"); font: Constants.headingFont }
            AppNavigationItem { id: liveNavigationButton; text: qsTr("Live"); width: parent.width; height: Constants.appStandardControlHeight; selected: root.selectedWorkspace === "live"; checkable: true; autoExclusive: true; checked: root.selectedWorkspace === "live" }
            AppNavigationItem { id: sequenceTestNavigationButton; text: qsTr("Sequence Test"); width: parent.width; height: Constants.appStandardControlHeight; selected: root.selectedWorkspace === "sequenceTest"; checkable: true; autoExclusive: true; checked: root.selectedWorkspace === "sequenceTest" }
            Text { text: qsTr("Results"); font: Constants.headingFont }
            AppNavigationItem { id: runsNavigationButton; text: qsTr("Runs"); width: parent.width; height: Constants.appStandardControlHeight; selected: root.selectedWorkspace === "runs"; checkable: true; autoExclusive: true; checked: root.selectedWorkspace === "runs" }
            AppNavigationItem { id: settingsNavigationButton; text: qsTr("Settings"); width: parent.width; height: Constants.appStandardControlHeight; selected: root.selectedWorkspace === "settings"; checkable: true; autoExclusive: true; checked: root.selectedWorkspace === "settings" }
        }
        }
        Item {
            id: hardwareOverlay
            SplitView.minimumHeight: root.drawerOpen ? Math.min(hardwareButton.height + Math.round(180 * Constants.textScale), navigationPanel.height * 0.75) : hardwareButton.height
            SplitView.preferredHeight: Math.min(hardwareButton.height + Math.round(320 * Constants.textScale), navigationPanel.height * 0.75)
            SplitView.maximumHeight: root.drawerOpen ? navigationPanel.height * 0.75 : hardwareButton.height
            AppButton {
                id: hardwareButton
                text: qsTr("Configuration")
                enabled: root.hardwareActionEnabled
                width: parent.width
                height: 42 * Constants.textScale
                anchors.bottom: parent.bottom
                focusPolicy: Qt.StrongFocus
                Accessible.name: qsTr("Open or close Configuration panel")
                background: Rectangle {
                    color: root.hardwareActionEnabled ? Constants.backgroundColor : "#e6e8eb"
                    border.color: hardwareButton.activeFocus ? Constants.accentColor : Constants.borderColor
                    border.width: hardwareButton.activeFocus ? 2 : 1
                }
                contentItem: Item {
                    Text {
                        id: hardwareIndicator
                        text: root.drawerOpen ? "⌄" : "›"
                        color: root.hardwareActionEnabled ? Constants.textColor : Constants.mutedTextColor
                        font: Constants.headingFont
                        anchors.right: parent.right
                        anchors.rightMargin: Constants.spacing
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        id: hardwareStatus
                        visible: !root.hardwareActionEnabled
                        text: qsTr("Disabled")
                        color: Constants.mutedTextColor
                        font: Constants.smallFont
                        anchors.right: hardwareIndicator.left
                        anchors.rightMargin: Constants.spacing
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: hardwareButton.text
                        color: root.hardwareActionEnabled ? Constants.textColor : Constants.mutedTextColor
                        font: Constants.headingFont
                        elide: Text.ElideRight
                        anchors.left: parent.left
                        anchors.leftMargin: Constants.spacing
                        anchors.right: hardwareStatus.visible ? hardwareStatus.left : hardwareIndicator.left
                        anchors.rightMargin: Constants.spacing
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
            Rectangle {
                id: hardwareBody
                visible: root.drawerOpen
                width: parent.width
                color: Constants.surfaceColor
                border.color: Constants.borderColor
                anchors.top: parent.top
                anchors.bottom: hardwareButton.top
                ScrollView { anchors.fill: parent; anchors.margins: Constants.spacing; clip: true; contentWidth: availableWidth
                Column { width: parent.width; spacing: Constants.spacing
                    Text { text: qsTr("Configuration"); font: Constants.headingFont; width: parent.width; wrapMode: Text.WordWrap }
                    AppButton { id: drawerCloseButton; visible: false; enabled: false; text: qsTr("Close"); width: 0; height: 0 }
                    AppAccordion {
                        id: cameraSection
                        sectionTitle: qsTr("Camera")
                        expanded: true
                        useIntrinsicBodyHeight: true
                        width: parent.width
                        Column {
                            width: parent.width
                            height: implicitHeight
                            spacing: Constants.spacing
                            Text { id: cameraStatusText; text: qsTr("Status: ") + root.cameraStatus + (root.cameraLocked ? qsTr(" — locked by current capture") : ""); color: root.cameraStatus === qsTr("Unavailable") ? Constants.warningColor : Constants.readyColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            Text { id: cameraErrorText; visible: root.cameraError !== ""; text: root.cameraError; color: Constants.warningColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            AppButton { id: restoreCameraButton; visible: root.cameraStatus === qsTr("Unavailable"); enabled: !root.cameraLocked; text: qsTr("Recover Camera"); width: parent.width; height: Constants.appStandardControlHeight }
                            Text { text: qsTr("Device"); font: Constants.font }
                            AppComboBox { id: cameraDeviceSelector; enabled: root.cameraConfigurationAvailable && !root.cameraLocked && root.cameraStatus !== qsTr("Unavailable"); model: root.cameraConfigurationAvailable ? [qsTr("Unavailable"), root.cameraDeviceText] : [root.cameraDeviceText]; currentIndex: root.cameraConfigurationAvailable ? root.cameraStatus === qsTr("Unavailable") ? 0 : 1 : 0; width: parent.width; height: Constants.appStandardControlHeight }
                            Text { text: qsTr("Resolution preset"); font: Constants.font }
                            AppComboBox { id: cameraResolutionSelector; enabled: root.cameraConfigurationAvailable && !root.cameraLocked && root.cameraStatus !== qsTr("Unavailable"); model: root.cameraConfigurationAvailable ? ["1024 × 1024", "2048 × 2048", "Custom"] : []; currentIndex: !root.cameraConfigurationAvailable ? -1 : root.cameraResolution === "2048 × 2048" ? 1 : root.cameraResolution === "Custom" ? 2 : 0; width: parent.width; height: Constants.appStandardControlHeight }
                            Row { visible: root.cameraConfigurationAvailable && root.cameraResolution === "Custom"; spacing: 6
                                AppTextField { id: cameraCustomWidthField; enabled: root.cameraConfigurationAvailable && !root.cameraLocked && root.cameraStatus !== qsTr("Unavailable"); text: root.cameraCustomWidth; width: (parent.width - 6) / 2; height: Constants.appStandardControlHeight; placeholderText: qsTr("Custom Width") }
                                AppTextField { id: cameraCustomHeightField; enabled: root.cameraConfigurationAvailable && !root.cameraLocked && root.cameraStatus !== qsTr("Unavailable"); text: root.cameraCustomHeight; width: (parent.width - 6) / 2; height: Constants.appStandardControlHeight; placeholderText: qsTr("Custom Height") }
                            }
                            Text { text: qsTr("Bit Depth"); font: Constants.font }
                            AppComboBox { id: cameraBitDepthSelector; enabled: root.cameraConfigurationAvailable && !root.cameraLocked && root.cameraStatus !== qsTr("Unavailable"); model: root.cameraConfigurationAvailable ? ["8-bit", "12-bit", "16-bit"] : []; currentIndex: !root.cameraConfigurationAvailable ? -1 : root.cameraBitDepth === "16-bit" ? 2 : root.cameraBitDepth === "12-bit" ? 1 : 0; width: parent.width; height: Constants.appStandardControlHeight }
                            Text { text: qsTr("Exposure"); font: Constants.font }
                            AppTextField { id: cameraExposureField; enabled: root.cameraConfigurationAvailable && !root.cameraLocked && root.cameraStatus !== qsTr("Unavailable"); text: root.cameraExposure; width: parent.width; height: Constants.appStandardControlHeight }
                            Text { text: qsTr("Readout"); font: Constants.font }
                            AppComboBox { id: cameraReadoutSelector; enabled: root.cameraConfigurationAvailable && !root.cameraLocked && root.cameraStatus !== qsTr("Unavailable"); model: root.cameraConfigurationAvailable ? ["Fast", "Slow"] : []; currentIndex: !root.cameraConfigurationAvailable ? -1 : root.cameraReadoutMode === "Slow" ? 1 : 0; width: parent.width; height: Constants.appStandardControlHeight }
                            Text { text: qsTr("LUT"); font: Constants.font }
                            AppComboBox { id: cameraLutSelector; visible: false; enabled: false; model: ["Linear", "High contrast"]; currentIndex: root.cameraLut === "High contrast" ? 1 : 0; width: 0; height: 0 }
                            RangeSlider {
                                id: previewLutRangeSlider
                                enabled: root.cameraConfigurationAvailable
                                width: parent.width
                                from: 0
                                to: 255
                                first.value: 0
                                second.value: 255
                                Accessible.name: qsTr("LUT")
                            }
                            Text { text: qsTr("Presentation LUT only"); color: Constants.mutedTextColor; font: Constants.smallFont; width: parent.width; wrapMode: Text.WordWrap }
                        }
                    }
                    AppAccordion {
                        id: daqSection
                        sectionTitle: qsTr("DAQ")
                        expanded: true
                        useIntrinsicBodyHeight: true
                        width: parent.width
                        Column {
                            width: parent.width
                            height: implicitHeight
                            spacing: Constants.spacing
                            Text { id: daqStatusText; text: qsTr("Status: ") + root.daqStatus; color: Constants.readyColor; font: Constants.smallFont }
                            Text { text: qsTr("Device"); font: Constants.font }
                            Text { id: daqDeviceText; text: root.daqDevice; font: Constants.smallFont }
                            AppButton { id: daqRefreshDevicesButton; text: qsTr("Refresh Devices"); width: parent.width; height: Constants.appStandardControlHeight }
                            Rectangle {
                                width: parent.width
                                height: outputConfigurationContent.implicitHeight + Constants.spacing * 2
                                color: Constants.surfaceColor
                                border.color: Constants.borderColor
                                Column {
                                    id: outputConfigurationContent
                                    anchors.fill: parent
                                    anchors.margins: Constants.spacing
                                    spacing: Constants.spacing
                                    Text { text: qsTr("Output Configuration"); font.pixelSize: Constants.font.pixelSize; font.bold: true; width: parent.width; wrapMode: Text.WordWrap }
                                    Text { text: qsTr("Output Channel"); font: Constants.font }
                                    AppComboBox { id: daqChannelSelector; model: ["ao0", "ao1"]; currentIndex: root.daqOutputChannel === "ao1" ? 1 : 0; width: parent.width; height: Constants.appStandardControlHeight }
                                    Text { text: qsTr("Amplitude (Vpp)"); font: Constants.font }
                                    AppSpinBox { id: daqVppSpinBox; from: 0; to: 10; value: 5; stepSize: 1; editable: true; enabled: root.daqStatus !== qsTr("Unavailable"); width: parent.width; height: Constants.appStandardControlHeight }
                                    Text { text: qsTr("Frequency (kHz)"); font: Constants.font }
                                    AppSpinBox { id: daqFrequencySpinBox; from: 1; to: 1000; value: 10; stepSize: 1; editable: true; enabled: root.daqStatus !== qsTr("Unavailable"); width: parent.width; height: Constants.appStandardControlHeight }
                                    Text { text: qsTr("Event Duration (ms)"); font: Constants.font }
                                    AppSpinBox { id: daqEventDurationSpinBox; from: 1; to: 500; value: 5; stepSize: 1; editable: true; enabled: root.daqStatus !== qsTr("Unavailable"); width: parent.width; height: Constants.appStandardControlHeight }
                                    Text { text: qsTr("Decision-to-trigger Delay (ms)"); font: Constants.font }
                                    AppSpinBox { id: daqDecisionDelaySpinBox; from: 0; to: 500; value: 0; stepSize: 1; editable: true; enabled: root.daqStatus !== qsTr("Unavailable"); width: parent.width; height: Constants.appStandardControlHeight }
                                    AppButton {
                                        id: continuousSineWaveButton
                                        text: root.continuousWaveformActive
                                              ? qsTr("Stop Sine Wave")
                                              : qsTr("Start Sine Wave")
                                        enabled: root.daqStatus !== qsTr("Unavailable")
                                        width: parent.width
                                        height: Constants.appPrimaryButtonHeight
                                        visualRole: "primary"
                                    }
                                }
                            }
                        }
                    }
                    AppAccordion {
                        id: detectorSection
                        sectionTitle: qsTr("Detector Configuration")
                        expanded: true
                        useIntrinsicBodyHeight: true
                        width: parent.width
                        Column {
                            width: parent.width
                            height: implicitHeight
                            spacing: Constants.spacing
                            Row {
                                width: parent.width
                                spacing: Constants.spacing
                                Text {
                                    text: qsTr("Minimum Size")
                                    font: Constants.font
                                    width: parent.width
                                           - smallDropletRejectionValueText.implicitWidth
                                           - smallDropletUnitText.implicitWidth
                                           - smallDropletSetButton.width
                                           - parent.spacing * 3
                                    height: smallDropletSetButton.height
                                    verticalAlignment: Text.AlignVCenter
                                }
                                Text {
                                    id: smallDropletRejectionValueText
                                    text: root.smallDropletRejectionArea
                                    font: Constants.font
                                    width: implicitWidth
                                    height: smallDropletSetButton.height
                                    verticalAlignment: Text.AlignVCenter
                                }
                                Text {
                                    id: smallDropletUnitText
                                    text: qsTr("px²")
                                    font: Constants.font
                                    width: implicitWidth
                                    height: smallDropletSetButton.height
                                    verticalAlignment: Text.AlignVCenter
                                }
                                AppButton {
                                    id: smallDropletSetButton
                                    text: qsTr("Set")
                                    enabled: root.smallDropletSetEnabled
                                    width: Math.max(72, implicitWidth)
                                    height: Constants.appStandardControlHeight
                                }
                            }
                            Text {
                                visible: !root.smallDropletSetEnabled
                                text: root.smallDropletSetDisabledReason
                                color: Constants.mutedTextColor
                                font: Constants.smallFont
                                width: parent.width
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
        }
        }
    }

    Item {
        id: workspace
        SplitView.fillWidth: true

        Item {
            visible: root.selectedWorkspace === "capture"
            anchors.fill: parent
            anchors.margins: Constants.workspaceMargin
            Text { id: captureWorkspaceTitle; text: qsTr("Capture"); font: Constants.largeFont; color: Constants.textColor; height: Constants.controlHeight; verticalAlignment: Text.AlignVCenter; anchors.left: parent.left; anchors.top: parent.top }
            SplitView {
                anchors.top: captureWorkspaceTitle.bottom
                anchors.topMargin: Constants.spacing
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right

            FullSizeImageViewer {
                id: cameraPreviewViewer
                SplitView.fillWidth: true
                source: root.cameraPreviewSource
                placeholderText: root.cameraStatus === qsTr("Unavailable")
                                 ? qsTr("Camera unavailable")
                                 : qsTr("Camera preview")

                Item {
                    id: smallDropletSelectionOverlay
                    anchors.fill: parent
                    visible: cameraPreviewViewer.image.visible

                    Rectangle {
                        visible: root.smallDropletSelectionVisible
                        x: Math.min(root.smallDropletSelectionStartXRatio,
                                    root.smallDropletSelectionEndXRatio) * parent.width
                        y: Math.min(root.smallDropletSelectionStartYRatio,
                                    root.smallDropletSelectionEndYRatio) * parent.height
                        width: Math.abs(root.smallDropletSelectionEndXRatio
                                        - root.smallDropletSelectionStartXRatio) * parent.width
                        height: Math.abs(root.smallDropletSelectionEndYRatio
                                         - root.smallDropletSelectionStartYRatio) * parent.height
                        color: "transparent"
                        border.color: Constants.accentColor
                        border.width: 2
                        Accessible.ignored: true
                    }

                    MouseArea {
                        id: smallDropletSelectionInputArea
                        anchors.fill: parent
                        enabled: root.smallDropletSelectionArmed
                                 && cameraPreviewViewer.image.visible
                        cursorShape: Qt.CrossCursor
                    }
                }
            }
            Rectangle {
                id: capturePanel
                SplitView.preferredWidth: Constants.operationPanelWidth
                SplitView.minimumWidth: Constants.collapsedOperationPanelWidth
                SplitView.maximumWidth: root.capturePanelExpanded ? Math.max(Constants.collapsedOperationPanelWidth, parent.width * 0.75) : Constants.collapsedOperationPanelWidth
                color: Constants.surfaceColor
                border.color: Constants.borderColor
                Rectangle {
                    id: capturePanelTopStrip
                    height: Constants.controlHeight
                    color: Constants.backgroundColor
                    border.color: Constants.borderColor
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    Text {
                        text: qsTr("Capture")
                        visible: root.capturePanelExpanded
                        font: Constants.headingFont
                        color: Constants.textColor
                        anchors.left: parent.left
                        anchors.leftMargin: Constants.spacing
                        anchors.right: parent.right
                        anchors.rightMargin: capturePanelToggleButton.width + Constants.spacing
                        anchors.verticalCenter: parent.verticalCenter
                        elide: Text.ElideRight
                    }
                }
                AppInspectorRail {
                    id: capturePanelToggleButton
                    text: root.capturePanelExpanded ? "›" : "‹"
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    z: 1
                }
                Rectangle {
                    id: cameraActionFooter
                    visible: root.capturePanelExpanded
                    height: Constants.controlHeight + Constants.spacing * 2
                    color: Constants.surfaceColor
                    border.color: Constants.borderColor
                    anchors.left: parent.left
                    anchors.leftMargin: Constants.collapsedOperationPanelWidth
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    AppButton {
                        id: startCameraButton
                        enabled: !root.cameraLocked && root.cameraStatus !== qsTr("Unavailable")
                        text: root.cameraStatus === qsTr("Streaming") ? qsTr("Stop Camera") : qsTr("Start Camera")
                        visualRole: root.cameraStatus === qsTr("Streaming") ? "destructive" : "primary"
                        anchors.fill: parent
                        anchors.margins: Constants.spacing
                    }
                }
                ScrollView {
                    visible: root.capturePanelExpanded
                    anchors.top: capturePanelTopStrip.bottom
                    anchors.bottom: cameraActionFooter.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: Constants.spacing
                    anchors.leftMargin: Constants.spacing
                    clip: true
                    contentWidth: availableWidth
                    contentHeight: captureSections.height

                    Column {
                        id: captureSections
                        width: parent.width
                        height: implicitHeight
                        spacing: 2
                        AppAccordion {
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
                            Text { text: qsTr("File Name"); font: Constants.font }
                            AppTextField { id: fileNameField; text: root.fileNameText; width: parent.width; height: Constants.appStandardControlHeight; placeholderText: qsTr("Optional — timestamp used") }
                            Text { text: qsTr("Save Location"); font: Constants.font }
                            Row {
                                spacing: 6
                                width: parent.width
                                AppTextField { id: saveLocationField; text: root.saveLocationText; width: parent.width - browseButton.width - 6; height: Constants.appStandardControlHeight }
                                AppButton { id: browseButton; text: qsTr("Browse"); height: Constants.appStandardControlHeight }
                            }
                            AppButton { id: captureButton; text: root.singleImagePresentation === "capturing" ? qsTr("Capturing Image…") : qsTr("Capture Image"); visualRole: "primary"; enabled: root.captureEnabled; width: parent.width; height: Constants.appPrimaryButtonHeight }
                            Text { visible: root.showSavedPath; text: qsTr("Saved: ") + root.savedPath; color: Constants.readyColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            Text { visible: root.disabledReason !== ""; text: root.disabledReason; color: Constants.warningColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            Rectangle { visible: root.singleImagePresentation === "error"; width: parent.width; height: 34; color: Constants.errorSurfaceColor; border.color: Constants.faultColor; Text { text: qsTr("Error"); color: Constants.faultColor; font.family: Constants.appBodyFont.family; font.pointSize: Constants.appBodyFont.pointSize; font.bold: true; anchors.centerIn: parent } }
                        }
                    }
                        AppAccordion {
                        id: imageSequenceSection; sectionTitle: qsTr("Image Sequence"); expanded: root.imageSequenceOpen; headingEnabled: !root.otherCaptureHeadingsDisabled || root.sequencePresentation === "running" || root.sequencePresentation === "paused"; width: parent.width; useIntrinsicBodyHeight: true
                        Column { spacing: 6; width: parent.width; height: implicitHeight
                            Text { text: qsTr("Name"); font: Constants.font }
                            AppTextField { id: sequenceNameField; enabled: root.sequencePresentation !== "running" && root.sequencePresentation !== "paused"; text: qsTr(""); width: parent.width; height: Constants.appStandardControlHeight; placeholderText: qsTr("Sequence name") }
                            Text { text: qsTr("Experiment Type"); font: Constants.font }
                            AppTextField { id: sequenceExperimentTypeField; enabled: root.sequencePresentation !== "running" && root.sequencePresentation !== "paused"; text: qsTr(""); width: parent.width; height: Constants.appStandardControlHeight }
                            Text { text: qsTr("Notes"); font: Constants.font }
                            AppTextArea { id: sequenceNotesField; enabled: root.sequencePresentation !== "running" && root.sequencePresentation !== "paused"; width: parent.width; height: Math.round(72 * Constants.textScale); placeholderText: qsTr("Optional notes"); wrapMode: TextEdit.Wrap }
                            Text { text: qsTr("Duration (optional — blank continues until Stop)"); font: Constants.font }
                            AppTextField { id: sequenceDurationField; enabled: root.sequencePresentation !== "running" && root.sequencePresentation !== "paused"; width: parent.width; height: Constants.appStandardControlHeight; placeholderText: qsTr("Optional") }
                            Text { text: qsTr("Save Location"); font: Constants.font }
                            Row { spacing: 6; width: parent.width
                                AppTextField { id: sequenceLocationField; enabled: root.sequencePresentation !== "running" && root.sequencePresentation !== "paused"; text: root.sequenceLocationText; width: parent.width - sequenceBrowseButton.width - 6; height: Constants.appStandardControlHeight }
                                AppButton { id: sequenceBrowseButton; enabled: root.sequencePresentation !== "running" && root.sequencePresentation !== "paused"; text: qsTr("Browse"); height: Constants.appStandardControlHeight }
                            }
                            AppButton { id: sequenceStartButton; visible: root.sequencePresentation === "ready"; text: qsTr("Start Recording"); visualRole: "primary"; enabled: root.sequencePresentation === "ready" && root.cameraStatus === qsTr("Streaming") && root.captureStartsAvailable && !root.otherCaptureHeadingsDisabled; width: parent.width; height: Constants.appPrimaryButtonHeight }
                            Row { visible: root.sequencePresentation === "running" || root.sequencePresentation === "paused"; width: parent.width; spacing: 6
                                AppButton { id: captureStopButton; text: qsTr("Stop"); visualRole: "destructive"; width: (parent.width - parent.spacing) * 0.8; height: Constants.appPrimaryButtonHeight }
                                AppButton { id: capturePauseButton; text: root.sequencePresentation === "paused" ? qsTr("Resume") : qsTr("Pause"); width: (parent.width - parent.spacing) * 0.2; height: Constants.appStandardControlHeight }
                            }
                            Text { visible: root.sequencePresentation !== "ready"; text: root.sequencePresentation === "completed" ? (root.sequenceCompletionText !== "" ? root.sequenceCompletionText : qsTr("Completed — %1 frames captured.").arg(root.sequenceFrameCount)) : qsTr("Frames captured: %1%2%3").arg(root.sequenceFrameCount).arg(root.sequenceElapsedTimeText === "" ? "" : qsTr("   Elapsed: %1").arg(root.sequenceElapsedTimeText)).arg(root.sequencePresentation === "paused" ? qsTr(" — Paused") : qsTr(" — Recording")); color: Constants.mutedTextColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            Text { visible: root.sequenceErrorText !== ""; text: root.sequenceErrorText; color: Constants.warningColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            Row { visible: root.sequencePresentation === "completed"; spacing: 4; AppButton { id: sequenceViewerButton; text: qsTr("Open in Sequence Viewer"); height: Constants.appStandardControlHeight } AppButton { id: sequenceTestButton; text: qsTr("Open in Sequence Test"); height: Constants.appStandardControlHeight } }
                            AppButton { id: sequenceNewButton; visible: root.sequencePresentation === "completed" || root.sequencePresentation === "error"; text: qsTr("Start New Recording"); width: parent.width; height: Constants.appStandardControlHeight }
                        }
                    }
                        AppAccordion {
                        id: datasetCaptureSection; sectionTitle: qsTr("Droplet Dataset Capture"); expanded: root.datasetOpen; headingEnabled: !root.otherCaptureHeadingsDisabled || root.datasetPresentation === "running" || root.datasetPresentation === "paused"; width: parent.width; useIntrinsicBodyHeight: true
                        Column { spacing: Constants.spacing; width: parent.width; height: implicitHeight
                            Text { text: qsTr("Dataset Name"); font: Constants.font }
                            AppTextField { id: datasetNameField; enabled: root.datasetPresentation !== "running" && root.datasetPresentation !== "paused"; width: parent.width; height: Constants.appStandardControlHeight; placeholderText: qsTr("Dataset name") }
                            Text { text: qsTr("Experiment Type"); font: Constants.font }
                            AppTextField { id: datasetExperimentTypeField; enabled: root.datasetPresentation !== "running" && root.datasetPresentation !== "paused"; width: parent.width; height: Constants.appStandardControlHeight }
                            Text { text: qsTr("Notes"); font: Constants.font }
                            AppTextArea { id: datasetNotesField; enabled: root.datasetPresentation !== "running" && root.datasetPresentation !== "paused"; width: parent.width; height: Math.round(72 * Constants.textScale); placeholderText: qsTr("Optional notes"); wrapMode: TextEdit.Wrap }
                            Text { text: qsTr("Duration (optional — blank continues until Stop)"); font: Constants.font }
                            AppTextField { id: datasetDurationField; enabled: root.datasetPresentation !== "running" && root.datasetPresentation !== "paused"; width: parent.width; height: Constants.appStandardControlHeight; placeholderText: qsTr("Optional") }
                            Text { text: qsTr("Save Location"); font: Constants.font }
                            Row { spacing: 6; width: parent.width
                                AppTextField { id: datasetLocationField; enabled: root.datasetPresentation !== "running" && root.datasetPresentation !== "paused"; text: root.datasetLocationText; width: parent.width - datasetBrowseButton.width - 6; height: Constants.appStandardControlHeight }
                                AppButton { id: datasetBrowseButton; enabled: root.datasetPresentation !== "running" && root.datasetPresentation !== "paused"; text: qsTr("Browse"); height: Constants.appStandardControlHeight }
                            }
                            Text { text: qsTr("Fixed qualified processing is used; detector, crop, and timing settings are not editable."); color: Constants.mutedTextColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            AppButton { id: datasetStartButton; visible: root.datasetPresentation === "ready"; text: qsTr("Start Droplet Dataset Capture"); visualRole: "primary"; enabled: root.datasetPresentation === "ready" && root.cameraStatus === qsTr("Streaming") && root.captureStartsAvailable && !root.otherCaptureHeadingsDisabled; width: parent.width; height: Constants.appPrimaryButtonHeight }
                            Row { visible: root.datasetPresentation === "running" || root.datasetPresentation === "paused"; width: parent.width; spacing: 6
                                AppButton { id: datasetStopButton; text: qsTr("Stop"); visualRole: "destructive"; width: (parent.width - parent.spacing) * 0.8; height: Constants.appPrimaryButtonHeight }
                                AppButton { id: datasetPauseButton; text: root.datasetPresentation === "paused" ? qsTr("Resume") : qsTr("Pause"); width: (parent.width - parent.spacing) * 0.2; height: Constants.appStandardControlHeight }
                            }
                            Text { visible: root.datasetPresentation !== "ready"; text: root.datasetPresentation === "completed" ? (root.datasetCompletionText !== "" ? root.datasetCompletionText : qsTr("Completed — %1 frames, %2 crops captured.").arg(root.datasetFrameCount).arg(root.datasetCropCount)) : qsTr("Frames: %1   Crops: %2%3%4").arg(root.datasetFrameCount).arg(root.datasetCropCount).arg(root.datasetElapsedTimeText === "" ? "" : qsTr("   Elapsed: %1").arg(root.datasetElapsedTimeText)).arg(root.datasetPresentation === "paused" ? qsTr(" — Paused") : qsTr(" — Capturing")); color: Constants.mutedTextColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            Text { visible: root.datasetErrorText !== ""; text: root.datasetErrorText; color: Constants.warningColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            Row { visible: root.datasetPresentation === "completed"; spacing: 4; AppButton { id: datasetLabelButton; text: qsTr("Open in Label"); height: Constants.appStandardControlHeight } AppButton { id: datasetFolderButton; text: qsTr("Open Folder"); height: Constants.appStandardControlHeight } }
                            AppButton { id: datasetNewButton; visible: root.datasetPresentation === "completed" || root.datasetPresentation === "error"; text: qsTr("Start New Droplet Dataset Capture"); width: parent.width; height: Constants.appStandardControlHeight }
                            Text { visible: root.datasetHandoffText !== ""; text: root.datasetHandoffText; color: Constants.mutedTextColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                        }
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

    }
    }

    Rectangle { visible: root.cameraPromptVisible; width: 430; height: 180; color: Constants.surfaceColor; border.color: Constants.warningColor; anchors.centerIn: parent; z: 2; Column { spacing: Constants.spacing; anchors.fill: parent; anchors.margins: Constants.spacing * 2; Text { text: qsTr("Camera unavailable. Continue?"); font: Constants.headingFont } Text { text: qsTr("Camera unavailable (not ready)"); color: Constants.warningColor; font: Constants.smallFont } Row { spacing: Constants.spacing; Button { id: cameraPromptYesButton; text: qsTr("Yes"); width: 92; height: Constants.controlHeight; checkable: true; checked: root.cameraPromptChoice === "yes" } Button { id: cameraPromptNoButton; text: qsTr("No"); width: 92; height: Constants.controlHeight; checkable: true; checked: root.cameraPromptChoice === "no" } } } }

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
