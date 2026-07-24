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
        daqStatus: state.projectedDaqStatus
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
        capturePanelExpanded: state.capturePanelExpanded
        selectedWorkspace: state.selectedWorkspace
        singleImagePresentation: state.singleImagePresentation
        singleImageOpen: state.singleImageOpen
        imageSequenceOpen: state.imageSequenceOpen
        datasetOpen: state.datasetOpen
        otherCaptureHeadingsDisabled: state.otherCaptureHeadingsDisabled
        cameraPromptVisible: state.cameraPromptVisible
        cameraPromptChoice: state.cameraPromptChoice
        sequencePresentation: state.capturePresentation === "sequence" ? state.capturePhase : "ready"
        datasetPresentation: state.capturePresentation === "dataset" ? state.capturePhase : "ready"
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
        hardwareActionEnabled: !state.liveOwnsOperation
        captureStartsAvailable: state.activeOperation === ""
    }

    Binding { target: screen.labelWorkspace; property: "presentation"; value: state.labelPresentation }
    Binding { target: screen.labelWorkspace; property: "classCount"; value: state.labelClassCount }
    Binding { target: screen.labelWorkspace; property: "datasetName"; value: state.labelDatasetName }
    Binding { target: screen.labelWorkspace; property: "totalCount"; value: state.labelTotalCount }
    Binding { target: screen.labelWorkspace; property: "labeledCount"; value: state.labelLabeledCount }
    Binding { target: screen.labelWorkspace; property: "rightPanelExpanded"; value: state.labelRightPanelExpanded }
    Binding { target: screen.labelWorkspace; property: "datasetSummaryExpanded"; value: state.labelDatasetSummaryExpanded }
    Binding { target: screen.labelWorkspace; property: "labelExpanded"; value: state.labelExpanded }
    Binding { target: screen.labelWorkspace; property: "filterExpanded"; value: state.labelFilterExpanded }

    Binding { target: screen.sequenceViewerWorkspace; property: "presentation"; value: state.sequenceViewerPresentation === "empty" || state.sequenceViewerPresentation === "error" ? state.sequenceViewerPresentation : "ready" }
    Binding { target: screen.sequenceViewerWorkspace; property: "currentFrame"; value: state.sequenceViewerPresentation === "firstFrame" ? 1 : state.sequenceViewerPresentation === "middleFrame" ? 60 : state.sequenceViewerPresentation === "finalFrame" ? 120 : 0 }
    Binding { target: screen.sequenceViewerWorkspace; property: "totalFrames"; value: state.sequenceViewerPresentation === "empty" || state.sequenceViewerPresentation === "error" ? 0 : 120 }

    Binding { target: screen.trainWorkspace; property: "presentation"; value: state.trainPresentation }
    Binding { target: screen.trainWorkspace; property: "datasetText"; value: state.trainPresentation === "empty" ? qsTr("No Dataset selected") : qsTr("Dataset-042") }
    Binding { target: screen.trainWorkspace; property: "deviceText"; value: state.trainPresentation === "readyCpu" ? qsTr("CPU (automatic)") : qsTr("GPU (automatic)") }
    Binding { target: screen.trainWorkspace; property: "disabledReason"; value: state.activeOperation !== "" ? qsTr("Another operation is active") : state.trainPresentation === "empty" ? qsTr("No dataset selected") : state.trainPresentation === "unavailable" ? qsTr("No Labeled Droplet Crops") : state.trainModelNameDraft.trim() === "" ? qsTr("Model name required") : "" }
    Binding { target: screen.trainWorkspace; property: "modelNameText"; value: state.trainModelNameDraft }
    Binding { target: screen.trainWorkspace; property: "saveLocationText"; value: state.trainSaveLocationDraft }
    Binding { target: screen.trainWorkspace; property: "startEnabled"; value: (state.trainPresentation === "readyCpu" || state.trainPresentation === "readyGpu") && state.trainModelNameDraft.trim() !== "" && state.activeOperation === "" }
    Binding { target: screen.trainWorkspace; property: "showRunning"; value: state.trainPresentation === "running" }
    Binding { target: screen.trainWorkspace; property: "showCompleted"; value: state.trainPresentation === "completed" }
    Binding { target: screen.trainWorkspace; property: "showError"; value: state.trainPresentation === "error" }
    Binding { target: screen.trainWorkspace; property: "showInterrupted"; value: state.trainPresentation === "interrupted" }
    Binding { target: screen.trainWorkspace; property: "trainingSetupExpanded"; value: state.trainingSetupExpanded }
    Binding { target: screen.trainWorkspace; property: "trainingStatusExpanded"; value: state.trainingStatusExpanded }
    Binding { target: screen.trainWorkspace; property: "operationPanelExpanded"; value: state.trainOperationPanelExpanded }

    Binding { target: screen.modelLibraryWorkspace; property: "presentation"; value: state.modelLibraryPresentation }
    Binding { target: screen.modelLibraryWorkspace; property: "hasSelection"; value: state.modelLibraryPresentation !== "empty" && state.modelLibraryPresentation !== "error" }
    Binding { target: screen.modelLibraryWorkspace; property: "selectedActive"; value: state.modelLibraryPresentation === "readyActive" || state.modelLibraryPresentation === "locked" }
    Binding { target: screen.modelLibraryWorkspace; property: "modelLocked"; value: state.modelLibraryPresentation === "locked" }
    Binding { target: screen.modelLibraryWorkspace; property: "showError"; value: state.modelLibraryPresentation === "error" }
    Binding { target: screen.modelLibraryWorkspace; property: "selectedModelExpanded"; value: state.selectedModelExpanded }
    Binding { target: screen.modelLibraryWorkspace; property: "rightPanelExpanded"; value: state.modelLibraryRightPanelExpanded }

    Binding { target: screen.modelTestWorkspace; property: "presentation"; value: state.modelTestPresentation }
    Binding { target: screen.modelTestWorkspace; property: "activeModelText"; value: state.activeModelText }
    Binding { target: screen.modelTestWorkspace; property: "datasetText"; value: state.modelTestDatasetSelected ? qsTr("Dataset-042") : qsTr("No Dataset selected") }
    Binding { target: screen.modelTestWorkspace; property: "deviceText"; value: state.modelTestPresentation === "readyCpu" ? qsTr("CPU (automatic)") : qsTr("GPU (automatic)") }
    Binding { target: screen.modelTestWorkspace; property: "outputLocationText"; value: state.modelTestOutputLocationDraft }
    Binding { target: screen.modelTestWorkspace; property: "blockerText"; value: state.activeOperation !== "" ? qsTr("Another operation is active") : state.activeModelId === "" ? qsTr("No Active Model") : !state.modelTestDatasetSelected ? qsTr("No dataset selected") : "" }
    Binding { target: screen.modelTestWorkspace; property: "startEnabled"; value: (state.modelTestPresentation === "readyCpu" || state.modelTestPresentation === "readyGpu") && state.activeOperation === "" }
    Binding { target: screen.modelTestWorkspace; property: "showRunning"; value: state.modelTestPresentation === "running" }
    Binding { target: screen.modelTestWorkspace; property: "showCompleted"; value: state.modelTestPresentation === "completedTwoClass" || state.modelTestPresentation === "completedThreeClass" }
    Binding { target: screen.modelTestWorkspace; property: "showError"; value: state.modelTestPresentation === "interrupted" || state.modelTestPresentation === "error" }
    Binding { target: screen.modelTestWorkspace; property: "threeClassResult"; value: state.modelTestPresentation === "completedThreeClass" }
    Binding { target: screen.modelTestWorkspace; property: "modelTestSetupExpanded"; value: state.modelTestSetupExpanded }
    Binding { target: screen.modelTestWorkspace; property: "modelTestStatusExpanded"; value: state.modelTestStatusExpanded }
    Binding { target: screen.modelTestWorkspace; property: "operationPanelExpanded"; value: state.modelTestOperationPanelExpanded }

    Binding { target: screen.liveWorkspace; property: "presentation"; value: state.livePresentation }
    Binding { target: screen.liveWorkspace; property: "cameraStreaming"; value: state.cameraStreaming }
    Binding { target: screen.liveWorkspace; property: "startSortingEnabled"; value: state.liveStartSortingEnabled }
    Binding { target: screen.liveWorkspace; property: "setupProfileExpanded"; value: !state.liveActive && state.livePresentation !== "completed" && state.liveSetupProfileExpanded }
    Binding { target: screen.liveWorkspace; property: "runInformationExpanded"; value: !state.liveActive && state.livePresentation !== "completed" && state.liveRunInformationExpanded }
    Binding { target: screen.liveWorkspace; property: "triggerTimingExpanded"; value: !state.liveActive && state.livePresentation !== "completed" && state.liveTriggerTimingExpanded }
    Binding { target: screen.liveWorkspace; property: "outputRecordingExpanded"; value: !state.liveActive && state.livePresentation !== "completed" && state.liveOutputRecordingExpanded }
    Binding { target: screen.liveWorkspace; property: "runningExpanded"; value: (state.liveActive || state.livePresentation === "completed") && state.liveRunningExpanded }
    Binding { target: screen.liveWorkspace; property: "runningHeadingEnabled"; value: state.liveActive || state.livePresentation === "completed" }
    Binding { target: screen.liveWorkspace; property: "rightPanelExpanded"; value: state.liveRightPanelExpanded }

    Binding { target: screen.sequenceTestWorkspace; property: "presentation"; value: state.sequenceTestPresentation }
    Binding { target: screen.sequenceTestWorkspace; property: "activeModelText"; value: state.activeModelText }
    Binding { target: screen.sequenceTestWorkspace; property: "sequenceTestExpanded"; value: state.sequenceTestExpanded }
    Binding { target: screen.sequenceTestWorkspace; property: "rightPanelExpanded"; value: state.sequenceTestRightPanelExpanded }
    Binding { target: screen.sequenceTestWorkspace.physicalDaqOutputControl; property: "checked"; value: state.physicalDaqOutputChecked }
    Binding { target: screen.sequenceTestWorkspace.startStopButton; property: "enabled"; value: state.sequenceTestPresentation === "running" || state.sequenceTestStartEnabled }

    Binding { target: screen.runsWorkspace; property: "selectedRunId"; value: state.runsPresentation === "runsEmpty" || state.runsPresentation === "runsError" ? "" : "Run-042" }
    Binding { target: screen.runsWorkspace; property: "loadedRunId"; value: state.runsPresentation === "runsLoaded" || state.runsPresentation === "runsNotesEditing" ? "Run-042" : "" }
    Binding { target: screen.runsWorkspace; property: "runsError"; value: state.runsPresentation === "runsError" }
    Binding { target: screen.runsWorkspace; property: "runsPanelExpanded"; value: state.runsPanelExpanded }
    Binding { target: screen.runsWorkspace; property: "rightPanelExpanded"; value: state.runsRightPanelExpanded }
    Binding { target: screen.runsWorkspace; property: "notesEditing"; value: state.runsPresentation === "runsNotesEditing" }
    Binding { target: screen.runsWorkspace; property: "loadedRunStatusText"; value: state.loadedRunStatusText }
    Binding { target: screen.runsWorkspace; property: "loadedRunStopReasonText"; value: state.loadedRunStopReasonText }
    Binding { target: screen.runsWorkspace; property: "run042RowStatusText"; value: state.run042RowStatusText }

    Binding { target: screen.settingsWorkspace; property: "settingsPresentation"; value: state.settingsPresentation === "settingsError" ? "error" : "ready" }
    Binding { target: screen.settingsWorkspace; property: "textSizePercent"; value: state.textSizePercent }
    Binding { target: Constants; property: "textSizePercent"; value: state.textSizePercent }

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
    Connections { target: screen.capturePanelToggleButton; function onClicked() { state.toggleCapturePanel() } }

    Connections { target: screen.labelWorkspace.openDatasetButton; function onClicked() { state.openLabelDataset() } }
    Connections { target: screen.labelWorkspace.twoClassChoice; function onClicked() { state.defineLabelClasses(2) } }
    Connections { target: screen.labelWorkspace.threeClassChoice; function onClicked() { state.defineLabelClasses(3) } }
    Connections { target: screen.labelWorkspace.rightPanelToggleButton; function onClicked() { state.toggleLabelPanel() } }
    Connections { target: screen.labelWorkspace.datasetSummaryHeadingButton; function onClicked() { state.toggleLabelDatasetSummary() } }
    Connections { target: screen.labelWorkspace.labelHeadingButton; function onClicked() { state.toggleLabelSection() } }
    Connections { target: screen.labelWorkspace.filterHeadingButton; function onClicked() { state.toggleLabelFilter() } }
    Connections { target: screen.labelWorkspace.allFilterButton; function onClicked() { state.selectLabelFilter("all") } }
    Connections { target: screen.labelWorkspace.class0FilterButton; function onClicked() { state.selectLabelFilter("class0") } }
    Connections { target: screen.labelWorkspace.class1FilterButton; function onClicked() { state.selectLabelFilter("class1") } }
    Connections { target: screen.labelWorkspace.class2FilterButton; function onClicked() { state.selectLabelFilter("class2") } }
    Connections { target: screen.labelWorkspace.excludedFilterButton; function onClicked() { state.selectLabelFilter("excluded") } }
    Connections { target: screen.labelWorkspace.unreviewedFilterButton; function onClicked() { state.selectLabelFilter("unreviewed") } }
    Connections { target: screen.labelWorkspace.class0Button; function onClicked() { state.recordLabel() } }
    Connections { target: screen.labelWorkspace.class1Button; function onClicked() { state.recordLabel() } }
    Connections { target: screen.labelWorkspace.class2Button; function onClicked() { state.recordLabel() } }
    Connections { target: screen.labelWorkspace.excludeButton; function onClicked() { state.recordLabel() } }
    Connections { target: screen.labelWorkspace.undoButton; function onClicked() { state.undoLabel() } }
    Connections { target: screen.labelWorkspace.previousButton; function onClicked() { state.moveLabelSelection(-1) } }
    Connections { target: screen.labelWorkspace.nextButton; function onClicked() { state.moveLabelSelection(1) } }
    Connections { target: screen.labelWorkspace.saveAsButton; function onClicked() { state.saveLabelDatasetAs() } }

    Connections { target: screen.sequenceViewerWorkspace.openSequenceButton; function onClicked() { state.openViewerSequence() } }
    Connections { target: screen.sequenceViewerWorkspace.previousButton; function onClicked() { state.previousViewerFrame() } }
    Connections { target: screen.sequenceViewerWorkspace.nextButton; function onClicked() { state.nextViewerFrame() } }
    Connections { target: screen.sequenceViewerWorkspace.directSeekField; function onAccepted() { state.seekViewerFrame(screen.sequenceViewerWorkspace.directSeekField.text) } }

    Connections { target: screen.trainWorkspace.selectDatasetButton; function onClicked() { state.selectTrainDataset() } }
    Connections { target: screen.trainWorkspace.modelNameField; function onTextEdited() { state.trainModelNameDraft = screen.trainWorkspace.modelNameField.text } }
    Connections { target: screen.trainWorkspace.saveLocationField; function onTextEdited() { state.trainSaveLocationDraft = screen.trainWorkspace.saveLocationField.text } }
    Connections { target: screen.trainWorkspace.browseButton; function onClicked() { state.browseTrainSaveLocation() } }
    Connections { target: screen.trainWorkspace.startButton; function onClicked() { state.startTraining() } }
    Connections { target: screen.trainWorkspace.stopButton; function onClicked() { state.stopTraining() } }
    Connections { target: screen.trainWorkspace.retrySaveButton; function onClicked() { state.retryTrainingSave() } }
    Connections { target: screen.trainWorkspace.openInModelTestButton; function onClicked() { state.openTrainingInModelTest() } }
    Connections { target: screen.trainWorkspace.trainingSetupHeadingButton; function onClicked() { state.toggleTrainingSetup() } }
    Connections { target: screen.trainWorkspace.trainingStatusHeadingButton; function onClicked() { state.toggleTrainingStatus() } }
    Connections { target: screen.trainWorkspace.operationPanelToggleButton; function onClicked() { state.toggleTrainOperationPanel() } }

    Connections { target: screen.modelLibraryWorkspace.activeModelRowButton; function onClicked() { state.selectActiveLibraryModel() } }
    Connections { target: screen.modelLibraryWorkspace.candidateModelRowButton; function onClicked() { state.selectCandidateLibraryModel() } }
    Connections { target: screen.modelLibraryWorkspace.setActiveButton; function onClicked() { state.setCandidateModelActive() } }
    Connections { target: screen.modelLibraryWorkspace.openInModelTestButton; function onClicked() { state.openLibraryModelTest() } }
    Connections { target: screen.modelLibraryWorkspace.selectedModelHeadingButton; function onClicked() { state.toggleSelectedModel() } }
    Connections { target: screen.modelLibraryWorkspace.rightPanelToggleButton; function onClicked() { state.toggleModelLibraryRightPanel() } }

    Connections { target: screen.modelTestWorkspace.selectDatasetButton; function onClicked() { state.selectModelTestDataset() } }
    Connections { target: screen.modelTestWorkspace.outputLocationField; function onTextEdited() { state.modelTestOutputLocationDraft = screen.modelTestWorkspace.outputLocationField.text } }
    Connections { target: screen.modelTestWorkspace.browseButton; function onClicked() { state.browseModelTestOutput() } }
    Connections { target: screen.modelTestWorkspace.startButton; function onClicked() { state.startModelTest() } }
    Connections { target: screen.modelTestWorkspace.stopButton; function onClicked() { state.stopModelTest() } }
    Connections { target: screen.modelTestWorkspace.openPredictionsButton; function onClicked() { state.openModelTestArtifact() } }
    Connections { target: screen.modelTestWorkspace.openSummaryButton; function onClicked() { state.openModelTestArtifact() } }
    Connections { target: screen.modelTestWorkspace.startAnotherButton; function onClicked() { state.startAnotherModelTest() } }
    Connections { target: screen.modelTestWorkspace.modelTestSetupHeadingButton; function onClicked() { state.toggleModelTestSetup() } }
    Connections { target: screen.modelTestWorkspace.modelTestStatusHeadingButton; function onClicked() { state.toggleModelTestStatus() } }
    Connections { target: screen.modelTestWorkspace.operationPanelToggleButton; function onClicked() { state.toggleModelTestOperationPanel() } }

    Connections { target: screen.liveWorkspace.primaryActionButton; function onClicked() { state.livePrimaryAction() } }
    Connections { target: screen.liveWorkspace.secondaryActionButton; function onClicked() { state.liveSecondaryAction() } }
    Connections { target: screen.liveWorkspace.setupProfileHeadingButton; function onClicked() { state.toggleLiveSetupProfile() } }
    Connections { target: screen.liveWorkspace.runInformationHeadingButton; function onClicked() { state.toggleLiveRunInformation() } }
    Connections { target: screen.liveWorkspace.triggerTimingHeadingButton; function onClicked() { state.toggleLiveTriggerTiming() } }
    Connections { target: screen.liveWorkspace.outputRecordingHeadingButton; function onClicked() { state.toggleLiveOutputRecording() } }
    Connections { target: screen.liveWorkspace.runningHeadingButton; function onClicked() { state.toggleLiveRunning() } }
    Connections { target: screen.liveWorkspace.rightPanelToggleButton; function onClicked() { state.toggleLiveRightPanel() } }

    Connections { target: screen.sequenceTestWorkspace.loadSequenceButton; function onClicked() { state.loadSequenceTest() } }
    Connections { target: screen.sequenceTestWorkspace.loadToMemoryButton; function onClicked() { state.loadSequenceTestToMemory() } }
    Connections { target: screen.sequenceTestWorkspace.startStopButton; function onClicked() { state.startOrStopSequenceTest() } }
    Connections { target: screen.sequenceTestWorkspace.physicalDaqOutputControl; function onToggled() { state.physicalDaqOutputChecked = screen.sequenceTestWorkspace.physicalDaqOutputControl.checked } }
    Connections { target: screen.sequenceTestWorkspace.sequenceTestHeadingButton; function onClicked() { state.toggleSequenceTest() } }
    Connections { target: screen.sequenceTestWorkspace.rightPanelToggleButton; function onClicked() { state.toggleSequenceTestRightPanel() } }

    Connections { target: screen.runsWorkspace.runsPanelToggleButton; function onClicked() { state.toggleRunsPanel() } }
    Connections { target: screen.runsWorkspace.rightPanelToggleButton; function onClicked() { state.toggleRunsRightPanel() } }
    Connections { target: screen.runsWorkspace.loadSelectedRunButton; function onClicked() { state.loadSelectedRun() } }
    Connections { target: screen.runsWorkspace.editNotesButton; function onClicked() { state.editRunNotes() } }
    Connections { target: screen.runsWorkspace.saveNotesButton; function onClicked() { state.finishRunNotesEditing() } }
    Connections {
        target: screen.settingsWorkspace.textSizeSelector
        function onActivated() {
            state.setTextSizePercent([80, 100, 125, 150, 175, 200][screen.settingsWorkspace.textSizeSelector.currentIndex])
        }
    }
    Connections { target: screen.runsWorkspace.cancelNotesButton; function onClicked() { state.finishRunNotesEditing() } }

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
    Connections { target: screen.cameraSectionHeadingButton; function onClicked() { screen.cameraSectionExpanded = !screen.cameraSectionExpanded } }
    Connections { target: screen.daqSectionHeadingButton; function onClicked() { screen.daqSectionExpanded = !screen.daqSectionExpanded } }

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
