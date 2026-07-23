import QtQuick
import Desktop_app_v2

Item {
    id: root

    anchors.fill: parent
    property alias mockState: state
    property alias form: screen
    signal closeRequested()

    function focusCameraPrompt() {
        if (state.cameraPromptVisible)
            screen.cameraPromptYesButton.forceActiveFocus()
    }

    Component.onCompleted: focusCameraPrompt()

    MockAppState {
        id: state
    }

    Screen01 {
        id: screen
        anchors.fill: parent
        cameraStatus: state.cameraStatus
        daqStatus: state.daqStatus
        activeModelText: state.activeModelText
        activityText: state.activityText
        fileNameText: state.fileNameDraft
        saveLocationText: state.saveLocationDraft
        disabledReason: state.disabledReason
        savedPath: state.savedPath
        bannerHeading: state.bannerHeading
        bannerText: state.bannerText
        captureEnabled: state.captureEnabled
        showSavedPath: state.showSavedPath
        showBanner: state.showBanner
        drawerOpen: state.hardwareDrawerOpen
        selectedWorkspace: state.selectedWorkspace
        singleImagePresentation: state.singleImagePresentation
        singleImageOpen: state.singleImageOpen
        imageSequenceOpen: state.imageSequenceOpen
        datasetOpen: state.datasetOpen
        otherCaptureHeadingsDisabled: state.otherCaptureHeadingsDisabled
        cameraPromptVisible: state.cameraPromptVisible
        cameraPromptChoice: state.cameraPromptChoice
        sequencePresentation: state.activeCapture === "sequence" ? state.capturePhase : "ready"
        datasetPresentation: state.activeCapture === "dataset" ? state.capturePhase : "ready"
        sequenceFrameCount: state.sequenceFrameCount
        datasetFrameCount: state.datasetFrameCount
        datasetCropCount: state.datasetCropCount
        cameraLocked: state.cameraLocked
        cameraResolution: state.cameraResolution
        cameraCustomWidth: state.cameraCustomWidth
        cameraCustomHeight: state.cameraCustomHeight
        cameraBitDepth: state.cameraBitDepth
        cameraExposure: state.cameraExposure
        cameraReadoutMode: state.cameraReadoutMode
        cameraLut: state.cameraLut
        daqDevice: state.daqDevice
        daqOutputChannel: state.daqOutputChannel
        sequenceLocationText: state.sequenceLocationDraft
        datasetLocationText: state.datasetLocationDraft
        datasetHandoffText: state.datasetHandoffText
    }

    Connections {
        target: state
        function onCameraPromptVisibleChanged() {
            root.focusCameraPrompt()
        }
    }

    Connections { target: screen.sequenceStartButton; function onClicked() { state.startSequence() } }
    Connections { target: screen.datasetStartButton; function onClicked() { state.startDataset() } }
    Connections { target: screen.capturePauseButton; function onClicked() { state.pauseOrResumeCapture() } }
    Connections { target: screen.captureStopButton; function onClicked() { state.stopCapture() } }
    Connections { target: screen.datasetPauseButton; function onClicked() { state.pauseOrResumeCapture() } }
    Connections { target: screen.datasetStopButton; function onClicked() { state.stopCapture() } }
    Connections { target: screen.sequenceBrowseButton; function onClicked() { state.browseSequence() } }
    Connections { target: screen.datasetBrowseButton; function onClicked() { state.browseDataset() } }
    Connections { target: screen.sequenceLocationField; function onTextEdited() { state.sequenceLocationDraft = screen.sequenceLocationField.text } }
    Connections { target: screen.datasetLocationField; function onTextEdited() { state.datasetLocationDraft = screen.datasetLocationField.text } }
    Connections { target: screen.startCameraButton; function onClicked() { state.toggleCameraStreaming() } }
    Connections { target: screen.restoreCameraButton; function onClicked() { state.selectCameraDevice(true) } }
    Connections { target: screen.cameraDeviceSelector; function onActivated(index) { state.selectCameraDevice(index === 1) } }
    Connections { target: screen.cameraResolutionSelector; function onActivated(index) { state.cameraResolution = index === 1 ? qsTr("2048 × 2048") : index === 2 ? qsTr("Custom") : qsTr("1024 × 1024") } }
    Connections { target: screen.cameraCustomWidthField; function onTextEdited() { state.cameraCustomWidth = screen.cameraCustomWidthField.text } }
    Connections { target: screen.cameraCustomHeightField; function onTextEdited() { state.cameraCustomHeight = screen.cameraCustomHeightField.text } }
    Connections { target: screen.cameraExposureField; function onTextEdited() { state.cameraExposure = screen.cameraExposureField.text } }
    Connections { target: screen.cameraLutSelector; function onActivated(index) { state.cameraLut = index === 1 ? qsTr("High contrast") : qsTr("Linear") } }
    Connections { target: screen.daqChannelSelector; function onActivated(index) { state.daqOutputChannel = index === 1 ? qsTr("ao1") : qsTr("ao0") } }
    Connections { target: screen.sequenceViewerButton; function onClicked() { state.openSequenceViewer() } }
    Connections { target: screen.sequenceTestButton; function onClicked() { state.openSequenceTest() } }
    Connections { target: screen.sequenceNewButton; function onClicked() { state.startNewSequence() } }
    Connections { target: screen.datasetLabelButton; function onClicked() { state.openLabel() } }
    Connections { target: screen.datasetFolderButton; function onClicked() { state.showMockFolder() } }
    Connections { target: screen.datasetNewButton; function onClicked() { state.startNewDataset() } }

    Connections {
        target: screen.hardwareButton
        function onClicked() {
            state.hardwareDrawerOpen = !state.hardwareDrawerOpen
            if (state.hardwareDrawerOpen)
                screen.drawerCloseButton.forceActiveFocus()
        }
    }

    Connections { target: screen.navCaptureButton; function onClicked() { state.selectWorkspace("capture") } }
    Connections { target: screen.navLabelButton; function onClicked() { state.selectWorkspace("label") } }
    Connections { target: screen.navSequenceViewerButton; function onClicked() { state.selectWorkspace("sequenceViewer") } }
    Connections { target: screen.navTrainButton; function onClicked() { state.selectWorkspace("train") } }
    Connections { target: screen.navModelTestButton; function onClicked() { state.selectWorkspace("modelTest") } }
    Connections { target: screen.navLibraryButton; function onClicked() { state.selectWorkspace("library") } }
    Connections { target: screen.navLiveButton; function onClicked() { state.selectWorkspace("live") } }
    Connections { target: screen.navSequenceTestButton; function onClicked() { state.selectWorkspace("sequenceTest") } }
    Connections { target: screen.navRunsButton; function onClicked() { state.selectWorkspace("runs") } }
    Connections { target: screen.navSettingsButton; function onClicked() { state.selectWorkspace("settings") } }

    Connections { target: screen.singleImageSection.headingButton; function onClicked() { state.toggleSingleImage() } }
    Connections { target: screen.imageSequenceSection.headingButton; function onClicked() { state.toggleImageSequence() } }
    Connections { target: screen.datasetCaptureSection.headingButton; function onClicked() { state.toggleDataset() } }

    Connections {
        target: screen.cameraPromptYesButton
        function onClicked() {
            state.continueWithoutCamera()
            screen.singleImageSection.headingButton.forceActiveFocus()
        }
    }

    Connections {
        target: screen.cameraPromptNoButton
        function onClicked() {
            state.declineCamera()
            root.closeRequested()
        }
    }

    Connections {
        target: screen.drawerCloseButton
        function onClicked() {
            state.hardwareDrawerOpen = false
            screen.hardwareButton.forceActiveFocus()
        }
    }

    Connections {
        target: screen.fileNameField
        function onTextEdited() {
            state.fileNameDraft = screen.fileNameField.text
        }
    }

    Connections {
        target: screen.saveLocationField
        function onTextEdited() {
            state.saveLocationDraft = screen.saveLocationField.text
        }
    }

    Connections {
        target: screen.browseButton
        function onClicked() {
            state.browse()
        }
    }

    Connections {
        target: screen.captureButton
        function onClicked() {
            state.capture()
        }
    }
}
