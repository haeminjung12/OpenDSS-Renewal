import QtQuick
import QtTest
import Desktop_app_v2Content

TestCase {
    name: "ShellSingleImage"

    ShellSingleImage {
        id: shell
        width: 1600
        height: 900
    }

    SignalSpy {
        id: closeSpy
        target: shell
        signalName: "closeRequested"
    }

    function init() {
        closeSpy.clear()
        shell.mockState.cameraStatus = "Streaming"
        shell.mockState.selectedWorkspace = "capture"
        shell.mockState.daqStatus = "Ready"
        shell.mockState.activeModelText = "No Active Model"
        shell.mockState.conflictingOperation = false
        shell.mockState.hardwareDrawerOpen = false
        shell.mockState.cameraPromptHandled = true
        shell.mockState.cameraPromptChoice = ""
        shell.mockState.singleImageOpen = false
        shell.mockState.imageSequenceOpen = false
        shell.mockState.datasetOpen = false
        shell.mockState.nextCaptureFails = false
        shell.mockState.fileNameDraft = "sample_042"
        shell.mockState.saveLocationDraft = "C:/OpenDSS/Images"
        shell.mockState.capturing = false
        shell.mockState.captureFailed = false
        shell.mockState.captureWillFail = false
        shell.mockState.savedPath = ""
    }

    function test_startupPromptAndNavigation() {
        shell.mockState.cameraStatus = "Unavailable"
        shell.mockState.cameraPromptHandled = false
        verify(shell.form.cameraPromptVisible)
        tryVerify(function() { return shell.form.cameraPromptYesButton.activeFocus })
        shell.form.cameraPromptYesButton.clicked()
        verify(!shell.form.cameraPromptVisible)
        compare(shell.mockState.cameraPromptChoice, "yes")
        verify(shell.form.singleImageSection.headingButton.activeFocus)

        shell.mockState.cameraPromptHandled = false
        shell.form.cameraPromptNoButton.clicked()
        compare(shell.mockState.cameraPromptChoice, "no")
        compare(closeSpy.count, 1)

        shell.form.navLabelButton.clicked()
        compare(shell.form.selectedWorkspace, "label")
        shell.form.navSequenceViewerButton.clicked()
        compare(shell.form.selectedWorkspace, "sequenceViewer")
        shell.form.navTrainButton.clicked()
        compare(shell.form.selectedWorkspace, "train")
        shell.form.navModelTestButton.clicked()
        compare(shell.form.selectedWorkspace, "modelTest")
        shell.form.navLibraryButton.clicked()
        compare(shell.form.selectedWorkspace, "library")
        shell.form.navLiveButton.clicked()
        compare(shell.form.selectedWorkspace, "live")
        shell.form.navSequenceTestButton.clicked()
        compare(shell.form.selectedWorkspace, "sequenceTest")
        shell.form.navRunsButton.clicked()
        compare(shell.form.selectedWorkspace, "runs")
        shell.form.navSettingsButton.clicked()
        compare(shell.form.selectedWorkspace, "settings")
        shell.form.navCaptureButton.clicked()
        compare(shell.form.selectedWorkspace, "capture")
    }

    function test_captureDisclosures() {
        verify(!shell.form.singleImageOpen)
        verify(!shell.form.imageSequenceOpen)
        verify(!shell.form.datasetOpen)
        shell.form.singleImageSection.headingButton.clicked()
        shell.form.imageSequenceSection.headingButton.clicked()
        verify(shell.form.singleImageOpen)
        verify(shell.form.imageSequenceOpen)

        shell.form.captureButton.clicked()
        verify(shell.form.singleImageOpen)
        verify(shell.form.singleImageSection.headingButton.enabled)
        verify(!shell.form.imageSequenceSection.headingButton.enabled)
        verify(!shell.form.datasetCaptureSection.headingButton.enabled)
        shell.form.singleImageSection.headingButton.clicked()
        verify(shell.form.singleImageOpen)
        tryVerify(function() { return !shell.mockState.capturing }, 1000)
        verify(shell.form.singleImageSection.headingButton.enabled)
        verify(shell.form.imageSequenceSection.headingButton.enabled)
        verify(shell.form.datasetCaptureSection.headingButton.enabled)
    }

    function test_cameraStatesAndConflictReason() {
        shell.mockState.cameraStatus = "Unavailable"
        compare(shell.form.cameraStatus, "Unavailable")
        verify(!shell.form.captureEnabled)
        compare(shell.form.disabledReason, "Camera unavailable")

        shell.mockState.cameraStatus = "Connected"
        compare(shell.form.cameraStatus, "Connected")
        verify(!shell.form.captureEnabled)

        shell.mockState.cameraStatus = "Streaming"
        compare(shell.form.cameraStatus, "Streaming")
        verify(shell.form.captureEnabled)

        shell.mockState.conflictingOperation = true
        verify(!shell.form.captureEnabled)
        compare(shell.form.disabledReason, "Another operation is active")
    }

    function test_captureSuccessAndFailure() {
        shell.form.hardwareButton.clicked()
        verify(shell.mockState.hardwareDrawerOpen)
        shell.form.hardwareButton.clicked()
        verify(!shell.mockState.hardwareDrawerOpen)
        shell.form.hardwareButton.clicked()
        shell.form.drawerCloseButton.clicked()
        verify(!shell.mockState.hardwareDrawerOpen)
        verify(shell.form.hardwareButton.activeFocus)

        shell.form.browseButton.clicked()
        compare(shell.mockState.saveLocationDraft, "C:/OpenDSS/MockImages")

        shell.form.captureButton.clicked()
        compare(shell.form.activityText, "Capturing Image")
        verify(!shell.form.captureEnabled)
        tryVerify(function() { return !shell.mockState.capturing }, 1000)
        verify(shell.form.showSavedPath)
        verify(shell.form.savedPath.indexOf("Illustrative mock path") === 0)

        shell.mockState.fileNameDraft = ""
        shell.form.captureButton.clicked()
        tryVerify(function() { return !shell.mockState.capturing }, 1000)
        compare(shell.form.savedPath, "Illustrative mock path — no file written: C:/OpenDSS/MockImages/single_image.tiff")

        init()
        shell.mockState.nextCaptureFails = true
        shell.form.captureButton.clicked()
        tryVerify(function() { return !shell.mockState.capturing }, 1000)
        verify(shell.form.showBanner)
        compare(shell.form.singleImagePresentation, "error")
        compare(shell.form.bannerHeading, "Error")
        compare(shell.form.bannerText, "")
        compare(shell.form.savedPath, "")
        compare(shell.form.disabledReason, "")
    }
}
