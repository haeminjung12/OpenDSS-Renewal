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
        shell.mockState.cameraAvailable = true
        shell.mockState.cameraStreaming = true
        shell.mockState.selectedWorkspace = "capture"
        shell.mockState.daqAvailable = true
        shell.mockState.activeModelId = ""
        shell.mockState.hardwareDrawerOpen = false
        shell.mockState.activeOperation = ""
        shell.mockState.labelPresentation = "empty"
        shell.mockState.labelClassCount = 0
        shell.mockState.sequenceViewerPresentation = "empty"
        shell.mockState.trainPresentation = "empty"
        shell.mockState.trainModelNameDraft = ""
        shell.mockState.trainSaveLocationDraft = "C:/OpenDSS/Models"
        shell.mockState.modelLibraryPresentation = "readySelected"
        shell.mockState.modelTestPresentation = "empty"
        shell.mockState.modelTestDatasetSelected = false
        shell.mockState.modelTestOutputLocationDraft = "C:/OpenDSS/ModelTests"
        shell.mockState.livePresentation = "ready"
        shell.mockState.sequenceTestPresentation = "empty"
        shell.mockState.physicalDaqOutputChecked = false
        shell.mockState.runsPresentation = "runsEmpty"
        shell.mockState.runsPanelExpanded = true
        shell.mockState.loadedRunOutcome = "completed"
        shell.mockState.settingsPresentation = "settingsReady"
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
        shell.mockState.capturePresentation = ""
        shell.mockState.capturePhase = "ready"
        shell.mockState.sequenceFrameCount = 0
        shell.mockState.datasetFrameCount = 0
        shell.mockState.datasetCropCount = 0
    }

    function test_startupPromptAndNavigation() {
        shell.mockState.cameraAvailable = false
        shell.mockState.cameraStreaming = false
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
        verify(shell.form.labelWorkspace !== null)
        verify(shell.form.labelWorkspace.visible)
        shell.form.navSequenceViewerButton.clicked()
        compare(shell.form.selectedWorkspace, "sequenceViewer")
        verify(shell.form.sequenceViewerWorkspace !== null)
        verify(shell.form.sequenceViewerWorkspace.visible)
        shell.form.navTrainButton.clicked()
        compare(shell.form.selectedWorkspace, "train")
        verify(shell.form.trainWorkspace !== null)
        verify(shell.form.trainWorkspace.visible)
        shell.form.navModelTestButton.clicked()
        compare(shell.form.selectedWorkspace, "modelTest")
        verify(shell.form.modelTestWorkspace !== null)
        verify(shell.form.modelTestWorkspace.visible)
        shell.form.navLibraryButton.clicked()
        compare(shell.form.selectedWorkspace, "library")
        verify(shell.form.modelLibraryWorkspace !== null)
        verify(shell.form.modelLibraryWorkspace.visible)
        shell.form.navLiveButton.clicked()
        compare(shell.form.selectedWorkspace, "live")
        verify(shell.form.liveWorkspace !== null)
        verify(shell.form.liveWorkspace.visible)
        shell.form.navSequenceTestButton.clicked()
        compare(shell.form.selectedWorkspace, "sequenceTest")
        verify(shell.form.sequenceTestWorkspace !== null)
        verify(shell.form.sequenceTestWorkspace.visible)
        shell.form.navRunsButton.clicked()
        compare(shell.form.selectedWorkspace, "runs")
        verify(shell.form.runsWorkspace !== null)
        verify(shell.form.runsWorkspace.visible)
        compare(shell.mockState.runsPresentation, "runsEmpty")
        shell.form.navSettingsButton.clicked()
        compare(shell.form.selectedWorkspace, "settings")
        verify(shell.form.settingsWorkspace !== null)
        verify(shell.form.settingsWorkspace.visible)
        compare(shell.form.settingsWorkspace.settingsPresentation, "ready")
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
        shell.mockState.cameraAvailable = false
        shell.mockState.cameraStreaming = false
        compare(shell.form.cameraStatus, "Unavailable")
        verify(!shell.form.captureEnabled)
        compare(shell.form.disabledReason, "Camera unavailable")

        shell.mockState.cameraAvailable = true
        compare(shell.form.cameraStatus, "Connected")
        verify(!shell.form.captureEnabled)

        shell.mockState.cameraStreaming = true
        compare(shell.form.cameraStatus, "Streaming")
        verify(shell.form.captureEnabled)

        shell.mockState.activeOperation = "training"
        verify(shell.form.captureEnabled)
        compare(shell.form.disabledReason, "")
        verify(!shell.form.sequenceStartButton.enabled)
        verify(!shell.form.datasetStartButton.enabled)
        shell.mockState.activeOperation = "modelTest"
        verify(shell.form.captureEnabled)
        shell.mockState.activeOperation = "sequenceTest"
        verify(shell.form.captureEnabled)
        shell.mockState.activeOperation = "live"
        shell.mockState.livePresentation = "running"
        verify(!shell.form.captureEnabled)
        compare(shell.form.disabledReason, "Another operation is active")
    }

    function test_workspaceHandoffs() {
        shell.form.navLabelButton.clicked()
        shell.form.labelWorkspace.openDatasetButton.clicked()
        compare(shell.mockState.labelPresentation, "classDefinition")
        shell.form.labelWorkspace.twoClassChoice.clicked()
        compare(shell.mockState.labelPresentation, "rightSectionsExpanded")
        compare(shell.mockState.labelClassCount, 2)
        shell.form.labelWorkspace.useInTrainButton.clicked()
        compare(shell.form.selectedWorkspace, "train")
        compare(shell.mockState.trainPresentation, "readyGpu")

        shell.form.navLibraryButton.clicked()
        shell.form.modelLibraryWorkspace.candidateModelRowButton.clicked()
        shell.form.modelLibraryWorkspace.setActiveButton.clicked()
        compare(shell.mockState.activeModelId, "DropletNet-03")
        compare(shell.mockState.activeModelText, shell.mockState.activeModelId)
        shell.form.modelLibraryWorkspace.openInModelTestButton.clicked()
        compare(shell.form.selectedWorkspace, "modelTest")
        compare(shell.mockState.activeModelId, "DropletNet-03")
        compare(shell.mockState.modelTestPresentation, "modelOnly")

        shell.form.navLiveButton.clicked()
        verify(shell.form.liveWorkspace.cameraStreaming)
        shell.form.liveWorkspace.primaryActionButton.clicked()
        compare(shell.mockState.livePresentation, "ready")
        verify(!shell.mockState.cameraStreaming)
        shell.form.liveWorkspace.primaryActionButton.clicked()
        verify(shell.mockState.cameraStreaming)
        shell.form.liveWorkspace.secondaryActionButton.clicked()
        compare(shell.mockState.livePresentation, "running")
        compare(shell.mockState.activeOperation, "live")
        verify(!shell.form.hardwareButton.enabled)
        compare(shell.form.daqStatus, "Active")
        shell.form.liveWorkspace.secondaryActionButton.clicked()
        compare(shell.mockState.livePresentation, "completed")
        compare(shell.mockState.activeOperation, "")
        shell.form.liveWorkspace.secondaryActionButton.clicked()
        compare(shell.form.selectedWorkspace, "runs")
        compare(shell.mockState.runsPresentation, "runsLoaded")
        compare(shell.form.runsWorkspace.run042RowStatusText, "Live Sorting  |  Stopped")
        compare(shell.form.runsWorkspace.loadedRunStatusText, "Live Sorting  •  Stopped  •  2026-07-23 10:41")
        compare(shell.form.runsWorkspace.loadedRunStopReasonText, "Stop Reason: Stopped by user")
    }

    function test_liveStartSortingRequiresSingleImageCameraRelease() {
        shell.mockState.cameraStreaming = true
        shell.mockState.daqAvailable = false
        verify(!shell.form.liveWorkspace.startSortingEnabled)

        shell.mockState.daqAvailable = true
        verify(shell.form.liveWorkspace.startSortingEnabled)

        shell.mockState.capturing = true

        verify(!shell.form.liveWorkspace.startSortingEnabled)
        shell.form.liveWorkspace.secondaryActionButton.clicked()
        compare(shell.mockState.livePresentation, "ready")
        compare(shell.mockState.activeOperation, "")

        shell.mockState.capturing = false
        verify(shell.form.liveWorkspace.startSortingEnabled)
    }

    function test_sequenceViewerTransitions() {
        shell.form.navSequenceViewerButton.clicked()
        shell.form.sequenceViewerWorkspace.openSequenceButton.clicked()
        compare(shell.mockState.sequenceViewerPresentation, "firstFrame")
        compare(shell.form.sequenceViewerWorkspace.currentFrame, 1)
        shell.form.sequenceViewerWorkspace.nextButton.clicked()
        compare(shell.mockState.sequenceViewerPresentation, "middleFrame")
        compare(shell.form.sequenceViewerWorkspace.currentFrame, 60)
        shell.form.sequenceViewerWorkspace.directSeekField.text = "42"
        shell.form.sequenceViewerWorkspace.directSeekField.accepted()
        compare(shell.mockState.sequenceViewerPresentation, "middleFrame")
    }

    function test_trainStartRequirementsAndInterrupt() {
        shell.form.navTrainButton.clicked()
        shell.form.trainWorkspace.selectDatasetButton.clicked()
        compare(shell.mockState.trainPresentation, "readyGpu")
        verify(!shell.form.trainWorkspace.startEnabled)
        compare(shell.form.trainWorkspace.disabledReason, "Model name required")

        shell.mockState.trainModelNameDraft = "DropletNet-Test"
        verify(shell.form.trainWorkspace.startEnabled)
        shell.mockState.activeOperation = "sequenceTest"
        verify(!shell.form.trainWorkspace.startEnabled)
        compare(shell.form.trainWorkspace.disabledReason, "Another operation is active")
        shell.mockState.activeOperation = ""

        shell.form.trainWorkspace.startButton.clicked()
        compare(shell.mockState.activeOperation, "training")
        compare(shell.mockState.trainPresentation, "running")
        compare(shell.form.activityText, "Training")
        shell.form.trainWorkspace.stopButton.clicked()
        compare(shell.mockState.activeOperation, "")
        compare(shell.mockState.trainPresentation, "interrupted")
        verify(shell.form.trainWorkspace.showInterrupted)
        verify(!shell.form.trainWorkspace.showError)
        shell.form.trainWorkspace.retrySaveButton.clicked()
        compare(shell.mockState.trainPresentation, "interrupted")

        shell.mockState.trainPresentation = "error"
        shell.mockState.activeModelId = ""
        shell.form.trainWorkspace.retrySaveButton.clicked()
        compare(shell.mockState.trainPresentation, "completed")
        compare(shell.mockState.activeModelId, "DropletNet-04")
        compare(shell.mockState.activeModelText, shell.mockState.activeModelId)
    }

    function test_modelTestStartAndInterrupt() {
        shell.mockState.activeModelId = "DropletNet-04"
        shell.form.navModelTestButton.clicked()
        shell.form.modelTestWorkspace.selectDatasetButton.clicked()
        compare(shell.mockState.modelTestPresentation, "readyGpu")
        verify(shell.form.modelTestWorkspace.startEnabled)
        shell.form.modelTestWorkspace.startButton.clicked()
        compare(shell.mockState.activeOperation, "modelTest")
        compare(shell.mockState.modelTestPresentation, "running")
        shell.form.modelTestWorkspace.stopButton.clicked()
        compare(shell.mockState.activeOperation, "")
        compare(shell.mockState.modelTestPresentation, "interrupted")
    }

    function test_sequenceTestAndRunsStates() {
        shell.form.navRunsButton.clicked()
        compare(shell.mockState.runsPresentation, "runsEmpty")

        shell.form.navSequenceTestButton.clicked()
        shell.mockState.physicalDaqOutputChecked = true
        verify(shell.form.sequenceTestWorkspace.physicalDaqOutputControl.checked)
        shell.form.sequenceTestWorkspace.loadSequenceButton.clicked()
        compare(shell.mockState.sequenceTestPresentation, "selected")
        verify(!shell.mockState.physicalDaqOutputChecked)
        shell.form.sequenceTestWorkspace.loadToMemoryButton.clicked()
        compare(shell.mockState.sequenceTestPresentation, "ready")
        shell.mockState.physicalDaqOutputChecked = true
        verify(shell.form.sequenceTestWorkspace.physicalDaqOutputControl.checked)
        shell.form.sequenceTestWorkspace.startStopButton.clicked()
        compare(shell.mockState.sequenceTestPresentation, "running")
        compare(shell.mockState.activeOperation, "sequenceTest")
        compare(shell.form.daqStatus, "Active")
        shell.form.sequenceTestWorkspace.startStopButton.clicked()
        compare(shell.mockState.sequenceTestPresentation, "completed")
        compare(shell.mockState.activeOperation, "")
        compare(shell.form.daqStatus, "Ready")

        shell.mockState.livePresentation = "completed"
        shell.mockState.liveSecondaryAction()
        compare(shell.mockState.runsPresentation, "runsLoaded")
        verify(shell.form.runsWorkspace.visible)
        shell.form.runsWorkspace.runsPanelToggleButton.clicked()
        verify(!shell.form.runsWorkspace.runsPanelExpanded)
        shell.form.runsWorkspace.editNotesButton.clicked()
        compare(shell.mockState.runsPresentation, "runsNotesEditing")
        shell.form.runsWorkspace.cancelNotesButton.clicked()
        compare(shell.mockState.runsPresentation, "runsLoaded")
        shell.form.runsWorkspace.editNotesButton.clicked()
        compare(shell.mockState.runsPresentation, "runsNotesEditing")
        shell.form.runsWorkspace.saveNotesButton.clicked()
        compare(shell.mockState.runsPresentation, "runsLoaded")
    }

    function test_sequenceTestPhysicalDaqRequirement() {
        shell.mockState.sequenceTestPresentation = "ready"
        shell.mockState.daqAvailable = false
        shell.mockState.physicalDaqOutputChecked = true

        verify(!shell.form.sequenceTestWorkspace.startStopButton.enabled)
        shell.mockState.startOrStopSequenceTest()
        compare(shell.mockState.sequenceTestPresentation, "ready")
        compare(shell.mockState.activeOperation, "")
        compare(shell.form.daqStatus, "Unavailable")

        shell.mockState.physicalDaqOutputChecked = false
        verify(shell.form.sequenceTestWorkspace.startStopButton.enabled)
        shell.mockState.startOrStopSequenceTest()
        compare(shell.mockState.sequenceTestPresentation, "running")
        compare(shell.mockState.activeOperation, "sequenceTest")
        compare(shell.form.daqStatus, "Unavailable")
    }

    function test_currentActivityProjections() {
        shell.mockState.selectedWorkspace = "label"
        shell.mockState.labelPresentation = "classDefinition"
        compare(shell.form.activityText, "Labeling")

        shell.mockState.activeOperation = "imageSequence"
        shell.mockState.capturePhase = "running"
        compare(shell.form.activityText, "Recording Sequence")
        shell.mockState.capturePhase = "paused"
        compare(shell.form.activityText, "Paused")

        shell.mockState.activeOperation = "dataset"
        shell.mockState.capturePhase = "running"
        compare(shell.form.activityText, "Droplet Dataset Capture")
        shell.mockState.activeOperation = "training"
        compare(shell.form.activityText, "Training")
        shell.mockState.activeOperation = "modelTest"
        compare(shell.form.activityText, "Testing Model")
        shell.mockState.activeOperation = "sequenceTest"
        compare(shell.form.activityText, "Testing Sequence")
        shell.mockState.activeOperation = "live"
        shell.mockState.livePresentation = "running"
        compare(shell.form.activityText, "Sorting")
        shell.mockState.livePresentation = "paused"
        compare(shell.form.activityText, "Paused")
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
        compare(shell.mockState.activeOperation, "")
        compare(shell.form.activityText, "Capturing Image")
        verify(!shell.form.captureEnabled)
        tryVerify(function() { return !shell.mockState.capturing }, 1000)
        compare(shell.mockState.activeOperation, "")
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
