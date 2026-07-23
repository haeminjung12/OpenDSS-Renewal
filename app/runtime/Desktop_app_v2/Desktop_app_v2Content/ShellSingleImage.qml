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
    }

    Connections {
        target: state
        function onCameraPromptVisibleChanged() {
            root.focusCameraPrompt()
        }
    }

    Connections {
        target: screen.hardwareButton
        function onClicked() {
            state.hardwareDrawerOpen = !state.hardwareDrawerOpen
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
