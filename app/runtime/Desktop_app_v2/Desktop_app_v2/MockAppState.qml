import QtQml

QtObject {
    id: root

    property string selectedWorkspace: "capture"
    property bool cameraAvailable: false
    property bool cameraStreaming: false
    property bool daqAvailable: true
    readonly property string daqStatus: daqAvailable ? qsTr("Ready") : qsTr("Unavailable")
    property string activeModelId: ""
    readonly property string activeModelText: activeModelId === "" ? qsTr("No Active Model") : activeModelId
    property bool hardwareDrawerOpen: false
    property bool capturePanelExpanded: true
    property string activeOperation: ""
    property string labelPresentation: "empty"
    property int labelClassCount: 3
    property string labelDatasetName: qsTr("Droplet Dataset")
    property int labelTotalCount: 18072
    property int labelLabeledCount: 18069
    property bool labelRightPanelExpanded: true
    property bool labelDatasetSummaryExpanded: true
    property bool labelExpanded: true
    property bool labelFilterExpanded: true
    property string selectedLabelFilter: "all"
    property int labelSelectionIndex: 0
    property string sequenceViewerPresentation: "empty"
    property string trainPresentation: "empty"
    property string trainModelNameDraft: ""
    property string trainSaveLocationDraft: qsTr("C:/OpenDSS/Models")
    property bool trainingSetupExpanded: true
    property bool trainingStatusExpanded: true
    property bool trainOperationPanelExpanded: true
    property string modelLibraryPresentation: "readySelected"
    property bool selectedModelExpanded: true
    property bool modelLibraryRightPanelExpanded: true
    property string modelTestPresentation: "empty"
    property bool modelTestDatasetSelected: false
    property string modelTestOutputLocationDraft: qsTr("C:/OpenDSS/ModelTests")
    property bool modelTestSetupExpanded: true
    property bool modelTestStatusExpanded: true
    property bool modelTestOperationPanelExpanded: true
    property string livePresentation: "unavailable"
    property bool liveRightPanelExpanded: true
    property bool liveSetupProfileExpanded: true
    property bool liveRunInformationExpanded: true
    property bool liveTriggerTimingExpanded: true
    property bool liveOutputRecordingExpanded: true
    property bool liveRunningExpanded: true
    property string sequenceTestPresentation: "empty"
    property bool sequenceTestExpanded: true
    property bool sequenceTestRightPanelExpanded: true
    property bool physicalDaqOutputChecked: false
    property string runsPresentation: "runsEmpty"
    property bool runsPanelExpanded: true
    property bool runsRightPanelExpanded: true
    property string loadedRunOutcome: "completed"
    readonly property string loadedRunStatusText: loadedRunOutcome === "stopped"
                                                   ? qsTr("Live Sorting  •  Stopped  •  2026-07-23 10:41")
                                                   : qsTr("Live Sorting  •  Completed  •  2026-07-23 10:41")
    readonly property string loadedRunStopReasonText: loadedRunOutcome === "stopped"
                                                       ? qsTr("Stop Reason: Stopped by user")
                                                       : qsTr("Stop Reason: Completed duration")
    readonly property string run042RowStatusText: loadedRunOutcome === "stopped"
                                                   ? qsTr("Live Sorting  |  Stopped")
                                                   : qsTr("Live Sorting  |  Completed")
    property string settingsPresentation: "settingsReady"
    property bool cameraPromptHandled: false
    property string cameraPromptChoice: ""
    property bool singleImageOpen: false
    property bool imageSequenceOpen: false
    property bool datasetOpen: false
    property bool nextCaptureFails: false
    property string fileNameDraft: ""
    property string saveLocationDraft: qsTr("C:/OpenDSS/Images")
    property bool capturing: false
    property bool captureFailed: false
    property bool captureWillFail: false
    property string savedPath: ""

    property string sequenceLocationDraft: qsTr("C:/OpenDSS/Sequences")
    property string datasetLocationDraft: qsTr("C:/OpenDSS/Datasets")
    property string capturePresentation: ""
    property string capturePhase: "ready"
    property int sequenceFrameCount: 0
    property int datasetFrameCount: 0
    property int datasetCropCount: 0
    property string datasetHandoffText: ""

    property string cameraResolution: qsTr("1024 × 1024")
    property string cameraCustomWidth: "1024"
    property string cameraCustomHeight: "1024"
    property string cameraBitDepth: qsTr("8-bit")
    property string cameraExposure: "8.0 ms"
    property string cameraReadoutMode: qsTr("Fastest")
    property string cameraLut: qsTr("Linear")
    property string daqDevice: qsTr("Illustrative DAQ 1")
    property string daqOutputChannel: qsTr("ao0")

    readonly property bool captureRunning: (activeOperation === "imageSequence" || activeOperation === "dataset")
                                           && (capturePhase === "running" || capturePhase === "paused")
    readonly property bool cameraOperationActive: activeOperation === "imageSequence"
                                                   || activeOperation === "dataset"
                                                   || activeOperation === "live"
    readonly property bool cameraLocked: capturing || cameraOperationActive
    readonly property string cameraStatus: !cameraAvailable ? qsTr("Unavailable") : cameraStreaming ? qsTr("Streaming") : qsTr("Connected")
    readonly property string activityText: activeOperation === "imageSequence" && capturePhase === "paused" ? qsTr("Paused")
                                                   : activeOperation === "dataset" && capturePhase === "paused" ? qsTr("Paused")
                                                   : activeOperation === "live" && livePresentation === "paused" ? qsTr("Paused")
                                                   : activeOperation === "imageSequence" ? qsTr("Recording Sequence")
                                                   : activeOperation === "dataset" ? qsTr("Droplet Dataset Capture")
                                                   : activeOperation === "training" ? qsTr("Training")
                                                   : activeOperation === "modelTest" ? qsTr("Testing Model")
                                                   : activeOperation === "sequenceTest" ? qsTr("Testing Sequence")
                                                   : activeOperation === "live" ? qsTr("Sorting")
                                                   : capturing ? qsTr("Capturing Image")
                                                   : selectedWorkspace === "label" && labelPresentation !== "empty" ? qsTr("Labeling")
                                                   : qsTr("Idle")
    readonly property string projectedDaqStatus: activeOperation === "live"
                                                   || (activeOperation === "sequenceTest" && physicalDaqOutputChecked)
                                                   ? qsTr("Active") : daqStatus
    readonly property bool captureEnabled: !capturing && !captureFailed && cameraStatus === qsTr("Streaming") && !cameraOperationActive
    readonly property string disabledReason: capturing ? qsTr("Capture is already in progress")
                                                   : cameraOperationActive ? qsTr("Another operation is active")
                                                   : cameraStatus === qsTr("Unavailable") ? qsTr("Camera unavailable")
                                                   : cameraStatus !== qsTr("Streaming") ? qsTr("Start Camera") : ""
    readonly property bool showBanner: captureFailed
    readonly property string bannerHeading: qsTr("Error")
    readonly property string bannerText: ""
    readonly property bool showSavedPath: savedPath !== ""
    readonly property string singleImagePresentation: capturing ? "capturing" : captureFailed ? "error" : cameraStatus === qsTr("Streaming") ? (savedPath !== "" ? "completed" : "ready") : "unavailable"
    readonly property bool otherCaptureHeadingsDisabled: capturing || captureRunning
    readonly property bool cameraPromptVisible: cameraStatus === qsTr("Unavailable") && !cameraPromptHandled
    readonly property bool liveActive: livePresentation === "running" || livePresentation === "paused"
    readonly property bool liveOwnsOperation: activeOperation === "live" && liveActive
    readonly property bool liveStartSortingEnabled: livePresentation === "ready"
        && cameraStreaming
        && daqAvailable
        && !capturing
        && activeOperation === ""
    readonly property bool sequenceTestStartEnabled: sequenceTestPresentation === "ready"
        && activeOperation === ""
        && (!physicalDaqOutputChecked || daqAvailable)
    function browse() { saveLocationDraft = qsTr("C:/OpenDSS/MockImages") }
    function browseSequence() { sequenceLocationDraft = qsTr("C:/OpenDSS/MockSequences") }
    function browseDataset() { datasetLocationDraft = qsTr("C:/OpenDSS/MockDatasets") }
    function selectCameraDevice(available) {
        cameraAvailable = available
        if (!available)
            cameraStreaming = false
        if (!liveActive && livePresentation !== "completed")
            livePresentation = available ? "ready" : "unavailable"
    }
    function toggleCameraStreaming() { if (cameraAvailable && !cameraLocked) cameraStreaming = !cameraStreaming }
    function capture() {
        if (!captureEnabled) return
        captureWillFail = nextCaptureFails; nextCaptureFails = false; captureFailed = false; savedPath = ""; singleImageOpen = true; capturing = true; captureTimer.restart()
    }
    function startSequence() { if (cameraStatus === qsTr("Streaming") && activeOperation === "") { activeOperation = "imageSequence"; capturePresentation = "sequence"; capturePhase = "running"; imageSequenceOpen = true; sequenceFrameCount = 24 } }
    function startDataset() { if (cameraStatus === qsTr("Streaming") && activeOperation === "") { activeOperation = "dataset"; capturePresentation = "dataset"; capturePhase = "running"; datasetOpen = true; datasetFrameCount = 18; datasetCropCount = 42 } }
    function pauseOrResumeCapture() { if (captureRunning) capturePhase = capturePhase === "running" ? "paused" : "running" }
    function stopCapture() { if (captureRunning) { capturePhase = "completed"; activeOperation = "" } }
    function openSequenceViewer() { selectedWorkspace = "sequenceViewer" }
    function openSequenceTest() { selectedWorkspace = "sequenceTest" }
    function startNewSequence() { capturePresentation = ""; capturePhase = "ready"; imageSequenceOpen = true }
    function openLabel() { selectedWorkspace = "label" }
    function showMockFolder() { datasetHandoffText = qsTr("Illustrative mock — no folder opened") }
    function startNewDataset() { capturePresentation = ""; capturePhase = "ready"; datasetOpen = true }
    function selectWorkspace(workspace) {
        selectedWorkspace = workspace
        if (workspace === "live" && !liveActive && livePresentation !== "completed")
            livePresentation = cameraAvailable ? "ready" : "unavailable"
    }
    function toggleSingleImage() { if (!capturing && !captureRunning) singleImageOpen = !singleImageOpen }
    function toggleImageSequence() { if (!capturing && !captureRunning) imageSequenceOpen = !imageSequenceOpen }
    function toggleDataset() { if (!capturing && !captureRunning) datasetOpen = !datasetOpen }
    function toggleCapturePanel() { capturePanelExpanded = !capturePanelExpanded }
    function continueWithoutCamera() { cameraPromptHandled = true; cameraPromptChoice = "yes" }
    function declineCamera() { cameraPromptHandled = true; cameraPromptChoice = "no" }

    function openLabelDataset() {
        labelPresentation = "ready"
        labelDatasetName = qsTr("Droplet Dataset")
        labelTotalCount = 18072
        labelLabeledCount = 18069
    }
    function defineLabelClasses(count) {
        if (count === 2 || count === 3)
            labelClassCount = count
        labelPresentation = "ready"
    }
    function toggleLabelPanel() { labelRightPanelExpanded = !labelRightPanelExpanded }
    function toggleLabelDatasetSummary() { labelDatasetSummaryExpanded = !labelDatasetSummaryExpanded }
    function toggleLabelSection() { labelExpanded = !labelExpanded }
    function toggleLabelFilter() { labelFilterExpanded = !labelFilterExpanded }
    function recordLabel() { labelLabeledCount = Math.min(labelTotalCount, labelLabeledCount + 1) }
    function undoLabel() { labelLabeledCount = Math.max(0, labelLabeledCount - 1) }
    function moveLabelSelection(direction) { labelSelectionIndex = Math.max(0, labelSelectionIndex + direction) }
    function selectLabelFilter(filter) { selectedLabelFilter = filter }
    function saveLabelDatasetAs() { labelDatasetName = qsTr("Droplet Dataset Copy") }

    function openViewerSequence() {
        sequenceViewerPresentation = "firstFrame"
    }
    function previousViewerFrame() {
        if (sequenceViewerPresentation === "empty" || sequenceViewerPresentation === "error")
            return
        sequenceViewerPresentation = sequenceViewerPresentation === "finalFrame" ? "middleFrame" : "firstFrame"
    }
    function nextViewerFrame() {
        if (sequenceViewerPresentation === "empty" || sequenceViewerPresentation === "error")
            return
        sequenceViewerPresentation = sequenceViewerPresentation === "firstFrame" ? "middleFrame" : "finalFrame"
    }
    function seekViewerFrame(text) {
        var requestedFrame = Number(text)
        if (requestedFrame > 0 && Math.floor(requestedFrame) === requestedFrame)
            sequenceViewerPresentation = "middleFrame"
        else
            sequenceViewerPresentation = "error"
    }

    function selectTrainDataset() { trainPresentation = "readyGpu" }
    function browseTrainSaveLocation() { trainSaveLocationDraft = qsTr("C:/OpenDSS/MockModels") }
    function toggleTrainingSetup() {
        if (trainPresentation !== "running" && trainPresentation !== "completed" && trainPresentation !== "error")
            trainingSetupExpanded = !trainingSetupExpanded
    }
    function toggleTrainingStatus() {
        if (trainPresentation === "running")
            trainingStatusExpanded = !trainingStatusExpanded
    }
    function toggleTrainOperationPanel() { trainOperationPanelExpanded = !trainOperationPanelExpanded }
    function startTraining() {
        if ((trainPresentation === "readyCpu" || trainPresentation === "readyGpu")
                && trainModelNameDraft.trim() !== "" && activeOperation === "") {
            activeOperation = "training"
            trainPresentation = "running"
        }
    }
    function stopTraining() {
        if (trainPresentation === "running" && activeOperation === "training") {
            trainPresentation = "interrupted"
            activeOperation = ""
        }
    }
    function retryTrainingSave() {
        if (trainPresentation === "error") {
            trainPresentation = "completed"
            activeModelId = "DropletNet-04"
        }
    }
    function openTrainingInModelTest() {
        if (activeOperation !== "modelTest") {
            activeModelId = "DropletNet-04"
            modelTestDatasetSelected = false
            modelTestPresentation = "modelOnly"
        }
        selectedWorkspace = "modelTest"
    }

    function selectActiveLibraryModel() {
        modelLibraryPresentation = "readyActive"
        activeModelId = "DropletNet-04"
    }
    function selectCandidateLibraryModel() { modelLibraryPresentation = "readySelected" }
    function toggleSelectedModel() { selectedModelExpanded = !selectedModelExpanded }
    function toggleModelLibraryRightPanel() { modelLibraryRightPanelExpanded = !modelLibraryRightPanelExpanded }
    function setCandidateModelActive() {
        modelLibraryPresentation = "readyActive"
        activeModelId = "DropletNet-03"
    }
    function openLibraryModelTest() {
        if (activeOperation !== "modelTest") {
            modelTestDatasetSelected = false
            modelTestPresentation = activeModelId === "" ? "empty" : "modelOnly"
        }
        selectedWorkspace = "modelTest"
    }

    function selectModelTestDataset() {
        modelTestDatasetSelected = true
        modelTestPresentation = activeModelId === "" ? "datasetOnly" : "readyGpu"
    }
    function browseModelTestOutput() { modelTestOutputLocationDraft = qsTr("C:/OpenDSS/MockModelTests") }
    function toggleModelTestSetup() {
        if (modelTestPresentation !== "running"
                && modelTestPresentation !== "completedTwoClass"
                && modelTestPresentation !== "completedThreeClass"
                && modelTestPresentation !== "interrupted"
                && modelTestPresentation !== "error")
            modelTestSetupExpanded = !modelTestSetupExpanded
    }
    function toggleModelTestStatus() {
        if (modelTestPresentation === "running")
            modelTestStatusExpanded = !modelTestStatusExpanded
    }
    function toggleModelTestOperationPanel() { modelTestOperationPanelExpanded = !modelTestOperationPanelExpanded }
    function openModelTestArtifact() { modelTestOutputLocationDraft = qsTr("Illustrative mock — no file opened") }
    function startModelTest() {
        if ((modelTestPresentation === "readyCpu" || modelTestPresentation === "readyGpu")
                && activeOperation === "") {
            activeOperation = "modelTest"
            modelTestPresentation = "running"
        }
    }
    function stopModelTest() {
        if (modelTestPresentation === "running" && activeOperation === "modelTest") {
            modelTestPresentation = "interrupted"
            activeOperation = ""
        }
    }
    function startAnotherModelTest() {
        if (activeModelId !== "")
            modelTestPresentation = modelTestDatasetSelected ? "readyGpu" : "modelOnly"
        else
            modelTestPresentation = modelTestDatasetSelected ? "datasetOnly" : "empty"
    }

    function livePrimaryAction() {
        if (livePresentation === "ready")
            toggleCameraStreaming()
        else if (livePresentation === "running" && activeOperation === "live") {
            hardwareDrawerOpen = false
            livePresentation = "paused"
        } else if (livePresentation === "paused" && activeOperation === "live") {
            hardwareDrawerOpen = false
            livePresentation = "running"
        } else if (livePresentation === "completed")
            livePresentation = "ready"
    }
    function toggleLiveSetupProfile() {
        if (!liveActive && livePresentation !== "completed")
            liveSetupProfileExpanded = !liveSetupProfileExpanded
    }
    function toggleLiveRunInformation() {
        if (!liveActive && livePresentation !== "completed")
            liveRunInformationExpanded = !liveRunInformationExpanded
    }
    function toggleLiveTriggerTiming() {
        if (livePresentation !== "completed")
            liveTriggerTimingExpanded = !liveTriggerTimingExpanded
    }
    function toggleLiveOutputRecording() {
        if (!liveActive && livePresentation !== "completed")
            liveOutputRecordingExpanded = !liveOutputRecordingExpanded
    }
    function toggleLiveRunning() {
        if (liveActive || livePresentation === "completed")
            liveRunningExpanded = !liveRunningExpanded
    }
    function toggleLiveRightPanel() { liveRightPanelExpanded = !liveRightPanelExpanded }
    function liveSecondaryAction() {
        if (liveStartSortingEnabled) {
            activeOperation = "live"
            hardwareDrawerOpen = false
            liveRunningExpanded = true
            livePresentation = "running"
        } else if (liveOwnsOperation) {
            hardwareDrawerOpen = false
            liveRunningExpanded = true
            livePresentation = "completed"
            activeOperation = ""
            loadedRunOutcome = "stopped"
        } else if (livePresentation === "completed") {
            runsPresentation = "runsLoaded"
            selectedWorkspace = "runs"
        }
    }

    function loadSequenceTest() {
        sequenceTestPresentation = "selected"
        physicalDaqOutputChecked = false
    }
    function toggleSequenceTest() { sequenceTestExpanded = !sequenceTestExpanded }
    function toggleSequenceTestRightPanel() { sequenceTestRightPanelExpanded = !sequenceTestRightPanelExpanded }
    function loadSequenceTestToMemory() {
        if (sequenceTestPresentation === "selected")
            sequenceTestPresentation = "ready"
    }
    function startOrStopSequenceTest() {
        if (sequenceTestStartEnabled) {
            activeOperation = "sequenceTest"
            sequenceTestPresentation = "running"
        } else if (sequenceTestPresentation === "running" && activeOperation === "sequenceTest") {
            sequenceTestPresentation = "completed"
            activeOperation = ""
        }
    }

    function loadSelectedRun() {
        if (runsPresentation === "runsSelected")
            runsPresentation = "runsLoaded"
    }
    function toggleRunsPanel() { runsPanelExpanded = !runsPanelExpanded }
    function toggleRunsRightPanel() { runsRightPanelExpanded = !runsRightPanelExpanded }
    function editRunNotes() {
        if (runsPresentation === "runsLoaded")
            runsPresentation = "runsNotesEditing"
    }
    function finishRunNotesEditing() {
        if (runsPresentation === "runsNotesEditing")
            runsPresentation = "runsLoaded"
    }

    property Timer captureTimer: Timer {
        interval: 100; repeat: false
        onTriggered: { root.capturing = false; root.captureFailed = root.captureWillFail; root.captureWillFail = false; if (!root.captureFailed) root.savedPath = qsTr("Illustrative mock path — no file written: C:/OpenDSS/MockImages/%1.tiff").arg(root.fileNameDraft === "" ? qsTr("single_image") : root.fileNameDraft) }
    }
}
