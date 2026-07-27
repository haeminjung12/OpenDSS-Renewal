import QtQuick
import QtQuick.Dialogs
import QtTest
import Desktop_app_v2
import Desktop_app_v2Content

Item {
    id: testRoot
    width: 1600
    height: 900

    QtObject {
        id: textSizeController
        property int textSizePercent: 100
        property int lastRequestedTextSizePercent: -1
        property url storageRoot: "file:///C:/OpenDSS/Settings%20Root"
        property int openStorageRootCallCount: 0
        property string openStorageRootError: ""

        function setTextSizePercent(value) {
            lastRequestedTextSizePercent = value
            textSizePercent = value
        }

        function setStorageRoot(value) {
            storageRoot = value
            return ""
        }

        function openStorageRoot() {
            ++openStorageRootCallCount
            return openStorageRootError
        }
    }

    ListModel {
        id: labelController
        property string presentation: "ready"
        property url manifestUrl: "file:///C:/OpenDSS/Datasets/fixture/dataset.json"
        property string datasetId: "fixture-dataset"
        property int totalCount: 2
        property int labeledCount: 1
        property int unreviewedCount: 1
        property int excludedCount: 0
        property int classCount: 3
        property int class0Count: 1
        property int class1Count: 0
        property int class2Count: 0
        property var classNames: ["Empty", "Single cell", "Multiple cells"]
        property bool class2Enabled: true
        property bool canUndo: true
        property string selectedRecordId: "r1"
        property url selectedCropUrl: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII="
        property int selectedIndex: 0
        property string filter: "all"
        property string errorMessage: ""
        property int configuredClassCount: -1
        property string assignedClassId: ""
        property int excludeCallCount: 0
        property int undoCallCount: 0
        property int previousCallCount: 0
        property int nextCallCount: 0
        property string selectedRecordArgument: ""
        property int selectCallCount: 0
        property string filterArgument: ""
        property int renamedClassIndex: -1
        property string renamedClassName: ""
        property url openArgument
        property url saveAsArgument

        ListElement {
            recordId: "r1"
            cropUrl: ""
            state: "class0"
            selected: true
        }
        ListElement {
            recordId: "r2"
            cropUrl: ""
            state: "unreviewed"
            selected: false
        }

        function resetRecords() {
            clear()
            append({ recordId: "r1", cropUrl: "", state: "class0", selected: true })
            append({ recordId: "r2", cropUrl: "", state: "unreviewed", selected: false })
        }

        function open(url) { openArgument = url; return true }
        function configureClassCount(count) { configuredClassCount = count; classCount = count; return true }
        function renameClass(index, name) {
            renamedClassIndex = index
            renamedClassName = name
            return true
        }
        function assignClass(classId) { assignedClassId = classId; return true }
        function exclude() { ++excludeCallCount; return true }
        function undo() { ++undoCallCount; return true }
        function previous() { ++previousCallCount; return true }
        function next() { ++nextCallCount; return true }
        function select(recordId) {
            selectedRecordArgument = recordId
            ++selectCallCount
            return true
        }
        function setFilter(filterName) { filterArgument = filterName; filter = filterName; return true }
        function saveAs(url) { saveAsArgument = url; return true }
    }

    QtObject {
        id: modelLibraryController
        property var modelRows: [
            {
                id: "model-active", name: "Active Model",
                architecture: "efficientnet_b0", performanceLabel: "More Accurate",
                classSummary: "Empty, Single", active: true
            },
            {
                id: "model-candidate", name: "Candidate Model",
                architecture: "mobilenet_v3_small", performanceLabel: "Faster",
                classSummary: "Empty, Single, MoreThanOne", active: false
            }
        ]
        property int selectedIndex: -1
        readonly property string selectedId: selectedIndex >= 0
                                                     ? modelRows[selectedIndex].id : ""
        property string activeId: "model-active"
        readonly property var selectedDetail: selectedIndex >= 0 ? {
            id: modelRows[selectedIndex].id,
            name: modelRows[selectedIndex].name,
            active: modelRows[selectedIndex].active,
            architecture: modelRows[selectedIndex].architecture,
            userFacingLabel: modelRows[selectedIndex].architecture === "mobilenet_v3_small"
                             ? "MobileNetV3-Small — Faster"
                             : "EfficientNet-B0 — More Accurate",
            performanceLabel: modelRows[selectedIndex].performanceLabel,
            classCount: modelRows[selectedIndex].classSummary.split(",").length,
            classSummary: modelRows[selectedIndex].classSummary,
            createdAt: "2026-07-25",
            packageLocation: "C:/OpenDSS/Models/model-candidate",
            status: "ready",
            message: "",
            canActivate: true
        } : ({})
        property string presentation: "ready"
        property string errorMessage: ""
        property bool operationInProgress: false
        readonly property bool canImport: !operationInProgress
        readonly property bool canExport: !operationInProgress && selectedIndex >= 0
        readonly property bool canDuplicate: canExport
        readonly property bool canDelete: !operationInProgress && selectedIndex >= 0
        property int refreshCallCount: 0
        property int selectCallCount: 0
        property int setActiveCallCount: 0
        property int renameCallCount: 0
        property int importCallCount: 0
        property int exportCallCount: 0
        property int duplicateCallCount: 0
        property int deleteCallCount: 0
        property url importArgument
        property url exportArgument
        property url duplicateDestinationArgument
        property string duplicateNameArgument: ""
        property string renamedTo: ""

        function reset() {
            modelRows = [
                {
                    id: "model-active", name: "Active Model",
                    architecture: "efficientnet_b0", performanceLabel: "More Accurate",
                    classSummary: "Empty, Single", active: true
                },
                {
                    id: "model-candidate", name: "Candidate Model",
                    architecture: "mobilenet_v3_small", performanceLabel: "Faster",
                    classSummary: "Empty, Single, MoreThanOne", active: false
                }
            ]
            selectedIndex = -1
            activeId = "model-active"
            presentation = "ready"
            errorMessage = ""
            operationInProgress = false
            selectCallCount = 0
            setActiveCallCount = 0
            renameCallCount = 0
            importCallCount = 0
            exportCallCount = 0
            duplicateCallCount = 0
            deleteCallCount = 0
            importArgument = ""
            exportArgument = ""
            duplicateDestinationArgument = ""
            duplicateNameArgument = ""
            renamedTo = ""
        }

        function refresh() {
            ++refreshCallCount
            return true
        }

        function select(index) {
            ++selectCallCount
            selectedIndex = index
            return true
        }

        function setActive() {
            ++setActiveCallCount
            activeId = selectedId
            modelRows = [
                {
                    id: "model-active", name: "Active Model",
                    architecture: "efficientnet_b0", performanceLabel: "More Accurate",
                    classSummary: "Empty, Single", active: false
                },
                {
                    id: "model-candidate", name: renamedTo || "Candidate Model",
                    architecture: "mobilenet_v3_small", performanceLabel: "Faster",
                    classSummary: "Empty, Single, MoreThanOne", active: true
                }
            ]
            return true
        }

        function renameSelected(name) {
            ++renameCallCount
            renamedTo = name
            modelRows = [
                modelRows[0],
                {
                    id: "model-candidate", name: name,
                    architecture: "mobilenet_v3_small", performanceLabel: "Faster",
                    classSummary: "Empty, Single, MoreThanOne", active: false
                }
            ]
            return true
        }

        function importModel(url) {
            ++importCallCount
            importArgument = url
            return true
        }

        function exportSelected(url) {
            ++exportCallCount
            exportArgument = url
            return true
        }

        function duplicateSelected(name, url) {
            ++duplicateCallCount
            duplicateNameArgument = name
            duplicateDestinationArgument = url
            return true
        }

        function deleteSelected() {
            ++deleteCallCount
            return true
        }
    }

    QtObject {
        id: trainingController
        property url datasetManifestUrl: "file:///C:/OpenDSS/Datasets/fixture/dataset.json"
        property string architecture: "mobilenet"
        property string modelName: "DropletNet-Test"
        property url outputDirectoryUrl: "file:///C:/OpenDSS/Training%20Output"
        property string requestedDevice: "gpu"
        property string presentation: "ready"
        property string errorMessage: ""
        property string stage: "fine_tune"
        property int stageEpochs: 20
        property int epoch: 5
        property int globalEpoch: 15
        property url resultDirectoryUrl: ""
        property url modelOnnxUrl: ""
        property url metadataUrl: ""
        property url registeredPackageUrl: ""
        property bool retrySaveAvailable: false
        property var weightOptions: ["ImageNet-pretrained", "Fixture checkpoint — OpenDSS checkpoint"]
        property int selectedWeightIndex: 0
        property int startCallCount: 0
        property int stopCallCount: 0
        property int retrySaveCallCount: 0
        property int loadWeightsCallCount: 0
        property int loadedWeightIndex: -1

        function reset() {
            datasetManifestUrl = "file:///C:/OpenDSS/Datasets/fixture/dataset.json"
            architecture = "mobilenet"
            modelName = "DropletNet-Test"
            outputDirectoryUrl = "file:///C:/OpenDSS/Training%20Output"
            requestedDevice = "gpu"
            presentation = "ready"
            errorMessage = ""
            stage = "fine_tune"
            stageEpochs = 20
            epoch = 5
            globalEpoch = 15
            resultDirectoryUrl = ""
            modelOnnxUrl = ""
            metadataUrl = ""
            registeredPackageUrl = ""
            retrySaveAvailable = false
            startCallCount = 0
            stopCallCount = 0
            retrySaveCallCount = 0
            selectedWeightIndex = 0
            loadWeightsCallCount = 0
            loadedWeightIndex = -1
        }

        function start() {
            ++startCallCount
            return true
        }

        function stop() {
            ++stopCallCount
        }

        function retrySave() {
            ++retrySaveCallCount
            return true
        }

        function loadWeights(index) {
            ++loadWeightsCallCount
            loadedWeightIndex = index
            selectedWeightIndex = index
            return true
        }
    }

    QtObject {
        id: captureWorkflowController
        property string sequencePresentation: "ready"
        property string datasetPresentation: "ready"
        property int sequenceFrameCount: 0
        property int datasetFrameCount: 0
        property int datasetCropCount: 0
        property string sequenceLocation: "C:/OpenDSS/Collections"
        property string datasetLocation: "C:/OpenDSS/Datasets"
        property string sequenceFolder: "C:/OpenDSS/Collections/fixture"
        property string datasetFolder: "C:/OpenDSS/Datasets/fixture"
        property string sequenceError: ""
        property string datasetError: ""
        readonly property bool captureActive:
            sequencePresentation === "running" || sequencePresentation === "paused"
            || datasetPresentation === "running" || datasetPresentation === "paused"
        property bool captureStartAvailable: !captureActive
        property int startSequenceCallCount: 0
        property int pauseSequenceCallCount: 0
        property int stopSequenceCallCount: 0
        property int startDatasetCallCount: 0
        property int pauseDatasetCallCount: 0
        property int stopDatasetCallCount: 0

        function reset() {
            sequencePresentation = "ready"
            datasetPresentation = "ready"
            sequenceFrameCount = 0
            datasetFrameCount = 0
            datasetCropCount = 0
            startSequenceCallCount = 0
            pauseSequenceCallCount = 0
            stopSequenceCallCount = 0
            startDatasetCallCount = 0
            pauseDatasetCallCount = 0
            stopDatasetCallCount = 0
        }
        function startSequence() {
            ++startSequenceCallCount
            sequencePresentation = "running"
            return true
        }
        function pauseOrResumeSequence() {
            ++pauseSequenceCallCount
            sequencePresentation = sequencePresentation === "paused" ? "running" : "paused"
            return true
        }
        function stopSequence() {
            ++stopSequenceCallCount
            sequencePresentation = "completed"
            return true
        }
        function newSequence() { sequencePresentation = "ready" }
        function startDataset() {
            ++startDatasetCallCount
            datasetPresentation = "running"
            return true
        }
        function pauseOrResumeDataset() {
            ++pauseDatasetCallCount
            datasetPresentation = datasetPresentation === "paused" ? "running" : "paused"
            return true
        }
        function stopDataset() {
            ++stopDatasetCallCount
            datasetPresentation = "completed"
            return true
        }
        function newDataset() { datasetPresentation = "ready" }
    }

    QtObject {
        id: modelTestController
        property url datasetManifestUrl: "file:///C:/OpenDSS/Datasets/fixture/dataset.json"
        property url outputFolderUrl: "file:///C:/OpenDSS/Model%20Tests"
        property string presentation: "ready"
        property string errorMessage: ""
        property string actionError: ""
        property bool canStart: true
        property string activeModelId: "active-model"
        property string activeModelName: "Active Test Model"
        property bool activeModelReady: true
        property string plannedDeviceText: "Determined at start"
        property int processedImages: 0
        property int eligibleImages: 2
        property real progress: 0
        property var resultSummary: ({})
        property url summaryUrl: ""
        property url predictionsCsvUrl: ""
        property url artifactOutputFolderUrl: ""
        property int startCallCount: 0
        property int stopCallCount: 0
        property int openSummaryCallCount: 0
        property int openPredictionsCallCount: 0

        function reset() {
            datasetManifestUrl = "file:///C:/OpenDSS/Datasets/fixture/dataset.json"
            outputFolderUrl = "file:///C:/OpenDSS/Model%20Tests"
            presentation = "ready"
            errorMessage = ""
            actionError = ""
            canStart = true
            activeModelId = "active-model"
            activeModelName = "Active Test Model"
            activeModelReady = true
            plannedDeviceText = "Determined at start"
            processedImages = 0
            eligibleImages = 2
            progress = 0
            resultSummary = ({})
            summaryUrl = ""
            predictionsCsvUrl = ""
            artifactOutputFolderUrl = ""
            startCallCount = 0
            stopCallCount = 0
            openSummaryCallCount = 0
            openPredictionsCallCount = 0
        }

        function start() {
            ++startCallCount
            return true
        }

        function stop() {
            ++stopCallCount
            return true
        }

        function openSummary() {
            ++openSummaryCallCount
            return true
        }

        function openPredictions() {
            ++openPredictionsCallCount
            return true
        }
    }

    QtObject {
        id: singleImageCaptureController
        property url outputFolder: "file:///C:/OpenDSS/Images"
        property string fileName: "accepted-name"
        property bool canCapture: presentation !== "capturing"
        property string disabledReason: ""
        property string presentation: "ready"
        property string error: ""
        property url savedArtifactUrl: ""
        property int outputPathEditCount: 0
        property string requestedOutputPath: ""

        function reset() {
            outputFolder = "file:///C:/OpenDSS/Images"
            fileName = "accepted-name"
            presentation = "ready"
            error = ""
            savedArtifactUrl = ""
            outputPathEditCount = 0
            requestedOutputPath = ""
        }

        function setOutputFolderPath(path) {
            ++outputPathEditCount
            requestedOutputPath = path
        }

        function capture() {
            presentation = "capturing"
            return true
        }
    }

    QtObject {
        id: daqController
        property var devices: []
        property var outputChannels: []
        property string selectedOutputChannel: ""
        property real amplitudeVpp: 5
        property real frequencyHz: 10000
        property real durationMs: 5
        property real delayMs: 0
        property string daqStatus: "Ready"
        property bool canApply: true
        property bool ready: true
        property bool continuousWaveformActive: false
        property string error: ""
        property int refreshDevicesCallCount: 0
        property int applyCallCount: 0
        property int sendTestSineWaveCallCount: 0
        property int toggleContinuousWaveformCallCount: 0

        function reset() {
            devices = [{ deviceId: "Dev1" }]
            outputChannels = ["Dev1/ao0", "Dev1/ao1"]
            selectedOutputChannel = "Dev1/ao0"
            amplitudeVpp = 5
            frequencyHz = 10000
            durationMs = 5
            delayMs = 0
            daqStatus = "Ready"
            canApply = true
            ready = true
            continuousWaveformActive = false
            error = ""
            refreshDevicesCallCount = 0
            applyCallCount = 0
            sendTestSineWaveCallCount = 0
            toggleContinuousWaveformCallCount = 0
        }

        function refreshDevices() {
            ++refreshDevicesCallCount
            devices = [{ deviceId: "Dev2" }]
            outputChannels = ["Dev2/ao0", "Dev2/ao1"]
            selectedOutputChannel = "Dev2/ao0"
            return true
        }

        function apply() {
            ++applyCallCount
            return true
        }
    }

    QtObject {
        id: liveSortingController
        property string presentation: "ready"
        property string error: ""
        property string diagnostic: ""
        property bool cameraStreaming: true
        property bool startSortingEnabled: true
        property string runName: "Production Run"
        property string experimentType: "Sorting"
        property string notes: "Controller-backed"
        property string duration: ""
        property string saveLocation: "C:/OpenDSS/Runs"
        property string activeModelText: "Production Model"
        property var hitClassOptions: ["Empty", "Single", "MoreThanOne"]
        property var hitClassModel: [
            { id: "2", name: "MoreThanOne" },
            { id: "1", name: "Single" },
            { id: "0", name: "Empty" }
        ]
        property string hitClassId: "1"
        property bool triggerEveryDroplet: false
        property bool daqOutputEnabled: false
        property bool recordFullImageSequence: false
        property real elapsedSeconds: 65
        property int persistedEvents: 12
        property var integrity: ({
            sourceFrameGaps: { count: 1 },
            queueRejections: { count: 2 },
            consumerFailures: { count: 0 }
        })
        property string stopReason: ""
        property string runFolder: "C:/OpenDSS/Runs/Production-Run"
        property string profilePath: ""
        property string profileStatus: ""
        property bool canSaveProfile: profilePath !== ""
        property int primaryActionCallCount: 0
        property int secondaryActionCallCount: 0
        property int saveProfileCallCount: 0

        function reset() {
            presentation = "ready"
            error = ""
            diagnostic = ""
            cameraStreaming = true
            startSortingEnabled = true
            runName = "Production Run"
            experimentType = "Sorting"
            notes = "Controller-backed"
            duration = ""
            saveLocation = "C:/OpenDSS/Runs"
            activeModelText = "Production Model"
            hitClassId = "1"
            triggerEveryDroplet = false
            daqOutputEnabled = false
            recordFullImageSequence = false
            primaryActionCallCount = 0
            secondaryActionCallCount = 0
            profilePath = ""
            profileStatus = ""
            saveProfileCallCount = 0
        }

        function primaryAction() { ++primaryActionCallCount; return true }
        function secondaryAction() { ++secondaryActionCallCount; return true }
        function saveProfile() { ++saveProfileCallCount; return true }
    }

    QtObject {
        id: sequenceTestController
        property string presentation: "selected"
        property string errorMessage: ""
        property bool canLoadToMemory: true
        property bool canStart: false
        property string activeModelName: "Production Model"
        property bool activeModelReady: true
        property url sourceManifestUrl: "file:///C:/Sequences/sequence.json"
        property string sequenceName: "Test Sequence"
        property url sequenceFolderUrl: "file:///C:/Sequences"
        property string sequencePath: "C:/Sequences/sequence.json"
        property int frameCount: 24
        property real recordedFps: 120
        property url previewUrl: ""
        property string sequenceValidation: "Valid"
        property real availableMemoryBytes: 8589934592
        property real bufferBytes: 25165824
        property bool memoryReady: false
        property string loadStatus: "Not loaded"
        property real requestedProcessingFps: 120
        property real achievedProcessingFps: 0
        property int processedFrames: 0
        property int totalFrames: 24
        property real progress: 0
        property string outputStatus: "Idle"
        property bool triggerEveryDroplet: false
        property var hitClassModel: [
            { id: "2", name: "MoreThanOne" },
            { id: "0", name: "Empty" },
            { id: "1", name: "Single" }
        ]
        property string selectedHitClassId: "1"
        property bool physicalDaqOutputEnabled: false
        property url outputFolderUrl: "file:///C:/OpenDSS/Sequence%20Tests"
        property url runFolderUrl: ""
        property url runSummaryUrl: ""
        property int loadToMemoryCallCount: 0
        property int startCallCount: 0
        property int stopCallCount: 0
        property int startAnotherCallCount: 0
        property int openRunFolderCallCount: 0

        function reset() {
            presentation = "selected"
            errorMessage = ""
            canLoadToMemory = true
            canStart = false
            requestedProcessingFps = 120
            triggerEveryDroplet = false
            selectedHitClassId = "1"
            physicalDaqOutputEnabled = false
            loadToMemoryCallCount = 0
            startCallCount = 0
            stopCallCount = 0
            startAnotherCallCount = 0
            openRunFolderCallCount = 0
            runFolderUrl = ""
            runSummaryUrl = ""
        }

        function selectSequence(url) { sourceManifestUrl = url; return true }
        function loadToMemory() { ++loadToMemoryCallCount; return true }
        function start() { ++startCallCount; return true }
        function stop() { ++stopCallCount; return true }
        function startAnotherTest() {
            ++startAnotherCallCount
            presentation = "ready"
            runFolderUrl = ""
            runSummaryUrl = ""
        }

        function sendTestSineWave() {
            ++sendTestSineWaveCallCount
            return true
        }

        function toggleContinuousWaveform() {
            ++toggleContinuousWaveformCallCount
            continuousWaveformActive = !continuousWaveformActive
            return true
        }
        function openRunFolder() { ++openRunFolderCallCount; return true }
    }

    QtObject {
        id: runsControllerMock
        property var runs: []
        property string selectedRunId: ""
        property var loadedRun: ({ notes: "Persisted notes" })
        property string errorMessage: ""
        property int updateNotesCallCount: 0
        property string updatedNotes: ""
        property int openSummaryCallCount: 0
        signal savedSequenceRequested(string manifestPath)
        function updateLoadedNotes(notes) {
            ++updateNotesCallCount
            updatedNotes = notes
            loadedRun = ({ notes: notes })
            return true
        }
        function openRunSummary(url) {
            ++openSummaryCallCount
            return true
        }
    }

    QtObject {
        id: unavailableCameraController
        property bool busy: false
        property string cameraStatus: "Unavailable"
        property string error: "Camera hardware unavailable"
        property string deviceId: "Hamamatsu ORCA-Flash4.0"
        property string previewSource: ""
        property bool streaming: false
        property bool configurationAvailable: false
        property string resolution: ""
        property string customWidth: ""
        property string customHeight: ""
        property string bitDepth: ""
        property string exposureMs: ""
        property string readoutMode: ""
        property var resolutionPresets: [
            "2304 x 2304", "2304 x 1152", "2304 x 576", "2304 x 288",
            "2304 x 144", "2304 x 72", "2304 x 36", "2304 x 16",
            "2304 x 8", "2304 x 4", "1152 x 1152", "1152 x 576",
            "1152 x 288", "1152 x 144", "576 x 576", "576 x 288",
            "576 x 144", "288 x 288", "288 x 144", "144 x 144",
            "Custom", "512 x 128", "512 x 64", "256 x 64", "256 x 32"
        ]
        property int resolutionPresetIndex: -1
        property int previewLutMinimum: 0
        property int previewLutMaximum: 255
        property int startCallCount: 0
        property int stopCallCount: 0
        property int recoverCallCount: 0
        property int resolutionCallCount: 0
        property int bitDepthCallCount: 0
        property int exposureCallCount: 0
        property int readoutCallCount: 0
        property int lutCallCount: 0
        property int previewReadyCallCount: 0
        property string lastPreviewReadySource: ""
        property int requestedWidth: 0
        property int requestedHeight: 0
        property int requestedBitDepth: 0
        property real requestedExposureMs: 0
        property string requestedReadoutMode: ""

        function start() { ++startCallCount }
        function stop() { ++stopCallCount }
        function recover() { ++recoverCallCount; error = "" }
        function selectCustomResolution() {
            resolution = "Custom"
            resolutionPresetIndex = 20
            return true
        }
        function selectResolutionPreset(index) {
            resolutionPresetIndex = index
            if (index === 20)
                return selectCustomResolution()
            const parts = resolutionPresets[index].split(" x ")
            resolution = resolutionPresets[index]
            return applyResolution(Number(parts[0]), Number(parts[1]))
        }
        function applyResolution(width, height) {
            ++resolutionCallCount
            requestedWidth = width
            requestedHeight = height
            return true
        }
        function applyBitDepth(depth) {
            ++bitDepthCallCount
            requestedBitDepth = depth
            return true
        }
        function applyExposureMs(value) {
            ++exposureCallCount
            requestedExposureMs = value
            return true
        }
        function applyReadoutMode(mode) {
            ++readoutCallCount
            requestedReadoutMode = mode
            return true
        }
        function setPreviewLutRange(minimum, maximum) {
            ++lutCallCount
            previewLutMinimum = minimum
            previewLutMaximum = maximum
        }
        function acknowledgePreviewReady(source) {
            ++previewReadyCallCount
            lastPreviewReadySource = source
        }
    }

    QtObject {
        id: unavailableCaptureController
        property url outputFolder: "file:///C:/OpenDSS/Images"
        property string fileName: "accepted-name"
        property bool canCapture: false
        property string disabledReason: "Camera unavailable"
        property string presentation: "ready"
        property string error: ""
        property url savedArtifactUrl: ""

        function setOutputFolderPath(path) {}
        function capture() {}
    }

    ShellSingleImage {
        id: shell
        anchors.fill: parent
        settingsController: textSizeController
        modelLibraryController: modelLibraryController
    }

    ShellSingleImage {
        id: unavailableCameraShell
        visible: false
        cameraController: unavailableCameraController
        singleImageCaptureController: unavailableCaptureController
    }

    TestCase {
        name: "ShellSingleImage"
        when: windowShown

    SignalSpy {
        id: closeSpy
        target: shell
        signalName: "closeRequested"
    }

    function init() {
        closeSpy.clear()
        shell.settingsController = textSizeController
        shell.datasetLabelController = null
        shell.trainingController = null
        shell.modelLibraryController = null
        shell.modelTestController = null
        shell.liveSortingController = null
        shell.sequenceTestController = null
        shell.captureWorkflowController = null
        shell.runsResultsController = null
        shell.singleImageCaptureController = null
        shell.daqController = null
        shell.settingsActionError = ""
        shell.liveHitBoundaryDefined = false
        shell.liveHitBoundaryXRatio = 0.0
        shell.liveHitBoundaryYRatio = 0.5
        shell.liveHitBoundarySide = "top"
        shell.sequenceHitBoundaryDefined = false
        shell.sequenceHitBoundaryXRatio = 0.0
        shell.sequenceHitBoundaryYRatio = 0.5
        shell.sequenceHitBoundarySide = "top"
        modelLibraryController.reset()
        modelTestController.reset()
        singleImageCaptureController.reset()
        daqController.reset()
        liveSortingController.reset()
        sequenceTestController.reset()
        captureWorkflowController.reset()
        shell.mockState.cameraAvailable = true
        shell.mockState.cameraStreaming = true
        shell.mockState.selectedWorkspace = "capture"
        shell.mockState.daqAvailable = true
        textSizeController.textSizePercent = 100
        textSizeController.lastRequestedTextSizePercent = -1
        textSizeController.storageRoot = "file:///C:/OpenDSS/Settings%20Root"
        textSizeController.openStorageRootCallCount = 0
        textSizeController.openStorageRootError = ""
        labelController.resetRecords()
        labelController.classCount = 3
        labelController.configuredClassCount = -1
        labelController.assignedClassId = ""
        labelController.excludeCallCount = 0
        labelController.undoCallCount = 0
        labelController.previousCallCount = 0
        labelController.nextCallCount = 0
        labelController.selectedRecordArgument = ""
        labelController.selectCallCount = 0
        labelController.filter = "all"
        labelController.filterArgument = ""
        labelController.renamedClassIndex = -1
        labelController.renamedClassName = ""
        labelController.openArgument = ""
        labelController.saveAsArgument = ""
        trainingController.reset()
        shell.mockState.activeModelId = ""
        shell.mockState.hardwareDrawerOpen = false
        shell.mockState.capturePanelExpanded = true
        shell.mockState.activeOperation = ""
        shell.mockState.labelPresentation = "empty"
        shell.mockState.labelClassCount = 3
        shell.mockState.labelDatasetName = "Droplet Dataset"
        shell.mockState.labelTotalCount = 18072
        shell.mockState.labelLabeledCount = 18069
        shell.mockState.labelRightPanelExpanded = true
        shell.mockState.labelDatasetSummaryExpanded = true
        shell.mockState.labelExpanded = true
        shell.mockState.labelFilterExpanded = true
        shell.mockState.selectedLabelFilter = "all"
        shell.mockState.labelSelectionIndex = 0
        shell.mockState.sequenceViewerPresentation = "empty"
        shell.mockState.trainPresentation = "empty"
        shell.mockState.trainModelNameDraft = ""
        shell.mockState.trainSaveLocationDraft = "C:/OpenDSS/Models"
        shell.mockState.trainingSetupExpanded = true
        shell.mockState.trainingStatusExpanded = true
        shell.mockState.trainOperationPanelExpanded = true
        shell.mockState.modelLibraryPresentation = "readySelected"
        shell.mockState.selectedModelExpanded = true
        shell.mockState.modelLibraryRightPanelExpanded = true
        shell.mockState.modelTestPresentation = "empty"
        shell.mockState.modelTestDatasetSelected = false
        shell.mockState.modelTestOutputLocationDraft = "C:/OpenDSS/ModelTests"
        shell.mockState.modelTestSetupExpanded = true
        shell.mockState.modelTestStatusExpanded = true
        shell.mockState.modelTestOperationPanelExpanded = true
        shell.mockState.livePresentation = "ready"
        shell.mockState.liveRightPanelExpanded = true
        shell.mockState.liveSetupProfileExpanded = true
        shell.mockState.liveRunInformationExpanded = true
        shell.mockState.liveTriggerTimingExpanded = true
        shell.mockState.liveOutputRecordingExpanded = true
        shell.mockState.liveRunningExpanded = true
        shell.mockState.sequenceTestPresentation = "empty"
        shell.mockState.sequenceTestExpanded = true
        shell.mockState.sequenceTestRightPanelExpanded = true
        shell.mockState.physicalDaqOutputChecked = false
        shell.mockState.runsPresentation = "runsEmpty"
        shell.mockState.runsPanelExpanded = true
        shell.mockState.runsRightPanelExpanded = true
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

    function test_daqControllerProjectionAndRefresh() {
        shell.daqController = daqController

        compare(shell.form.daqStatus, "Ready")
        compare(shell.form.daqDevice, "Dev1")
        compare(shell.form.daqStatusText.text, "Status: Ready")
        verify(shell.form.daqStatusText.text.indexOf("mock") === -1)
        compare(shell.form.daqChannelSelector.model.length, 2)
        compare(shell.form.daqChannelSelector.model[0], "Dev1/ao0")
        compare(shell.form.daqChannelSelector.currentIndex, 0)
        compare(shell.form.daqVppSpinBox.value, 5)
        compare(shell.form.daqFrequencySpinBox.value, 10)
        compare(shell.form.daqEventDurationSpinBox.value, 5)
        compare(shell.form.daqDecisionDelaySpinBox.value, 0)

        shell.form.daqRefreshDevicesButton.clicked()
        compare(daqController.refreshDevicesCallCount, 1)
        compare(shell.form.daqDevice, "Dev2")
        compare(shell.form.daqChannelSelector.model[0], "Dev2/ao0")
        compare(shell.form.daqChannelSelector.currentIndex, 0)
        compare(daqController.applyCallCount, 0)
    }

    function test_realUnavailableCameraDoesNotShowStartupPrompt() {
        unavailableCameraShell.visible = true
        unavailableCameraShell.mockState.hardwareDrawerOpen = true
        wait(0)
        unavailableCameraController.cameraStatus = "Connected"
        unavailableCameraController.configurationAvailable = true
        unavailableCameraController.deviceId = "DCAM:0"
        wait(0)
        compare(unavailableCameraShell.form.cameraDeviceSelector.model[1], "DCAM:0")
        verify(unavailableCameraShell.form.cameraDeviceSelector.model[1].indexOf("Illustrative") === -1)
        unavailableCameraController.cameraStatus = "Unavailable"
        unavailableCameraController.configurationAvailable = false
        unavailableCameraController.deviceId = "Hamamatsu ORCA-Flash4.0"
        wait(0)
        compare(unavailableCameraShell.form.cameraStatus, "Unavailable")
        compare(unavailableCameraShell.form.disabledReason, "Camera unavailable")
        compare(unavailableCameraShell.form.cameraErrorText.text,
                "Camera hardware unavailable")
        verify(unavailableCameraShell.form.cameraErrorText.visible)
        verify(unavailableCameraShell.form.cameraPreviewImage.retainWhileLoading)
        verify(!unavailableCameraShell.form.cameraPromptVisible)
        verify(unavailableCameraShell.form.cameraStatusText.text.indexOf("mock") === -1)
        compare(unavailableCameraShell.form.cameraDeviceSelector.model[0],
                "Hamamatsu ORCA-Flash4.0")
        verify(!unavailableCameraShell.form.cameraDeviceSelector.enabled)
        verify(unavailableCameraShell.form.cameraDeviceSelector.model[0].indexOf("Illustrative") === -1)
        verify(!unavailableCameraShell.form.cameraResolutionSelector.enabled)
        verify(!unavailableCameraShell.form.cameraBitDepthSelector.enabled)
        verify(!unavailableCameraShell.form.cameraExposureField.enabled)
        verify(!unavailableCameraShell.form.cameraReadoutSelector.enabled)
        verify(!unavailableCameraShell.form.previewLutRangeSlider.enabled)
        compare(unavailableCameraShell.form.cameraResolutionSelector.currentIndex, -1)
        compare(unavailableCameraShell.form.cameraBitDepthSelector.currentIndex, -1)
        compare(unavailableCameraShell.form.cameraReadoutSelector.currentIndex, -1)
        compare(unavailableCameraShell.form.cameraExposureField.text, "")
        const originalCameraResolution = unavailableCameraShell.mockState.cameraResolution
        unavailableCameraShell.form.cameraResolutionSelector.activated(2)
        compare(unavailableCameraShell.mockState.cameraResolution, originalCameraResolution)
        const originalCameraLut = unavailableCameraShell.mockState.cameraLut
        unavailableCameraShell.form.cameraLutSelector.activated(1)
        compare(unavailableCameraShell.mockState.cameraLut, originalCameraLut)
        unavailableCameraShell.form.restoreCameraButton.clicked()
        compare(unavailableCameraController.recoverCallCount, 1)
        compare(unavailableCameraShell.form.cameraErrorText.text, "")
        verify(!unavailableCameraShell.form.cameraErrorText.visible)

        unavailableCameraController.cameraStatus = "Connected"
        unavailableCameraController.configurationAvailable = true
        unavailableCameraController.resolution = "2304 x 2304"
        unavailableCameraController.resolutionPresetIndex = 0
        unavailableCameraController.customWidth = "2304"
        unavailableCameraController.customHeight = "2304"
        unavailableCameraController.bitDepth = "12-bit"
        unavailableCameraController.exposureMs = "10"
        unavailableCameraController.readoutMode = "Fast"
        unavailableCameraController.resolutionCallCount = 0
        wait(0)
        verify(unavailableCameraShell.form.cameraResolutionSelector.enabled)
        verify(unavailableCameraShell.form.cameraBitDepthSelector.enabled)
        verify(unavailableCameraShell.form.cameraExposureField.enabled)
        verify(unavailableCameraShell.form.cameraReadoutSelector.enabled)
        compare(unavailableCameraShell.form.cameraResolutionSelector.currentIndex, 0)
        compare(unavailableCameraShell.form.cameraBitDepthSelector.currentIndex, 1)
        compare(unavailableCameraShell.form.cameraExposureField.text, "10")
        compare(unavailableCameraShell.form.cameraReadoutSelector.currentIndex, 0)

        compare(unavailableCameraShell.form.cameraResolutionSelector.model.length, 25)
        compare(unavailableCameraShell.form.cameraResolutionSelector.model[20], "Custom")
        compare(unavailableCameraShell.form.cameraResolutionSelector.model[21], "512 x 128")
        compare(unavailableCameraShell.form.cameraLutSelector.model.length, 1)
        compare(unavailableCameraShell.form.cameraLutSelector.model[0], "Linear")
        unavailableCameraShell.form.cameraResolutionSelector.activated(10)
        compare(unavailableCameraController.resolutionCallCount, 1)
        compare(unavailableCameraController.requestedWidth, 1152)
        compare(unavailableCameraController.requestedHeight, 1152)
        unavailableCameraShell.form.cameraResolutionSelector.activated(20)
        compare(unavailableCameraController.resolution, "Custom")
        wait(0)
        unavailableCameraShell.form.cameraCustomWidthField.text = "1536"
        unavailableCameraShell.form.cameraCustomHeightField.text = "1024"
        unavailableCameraShell.form.cameraCustomWidthField.editingFinished()
        compare(unavailableCameraController.resolutionCallCount, 2)
        compare(unavailableCameraController.requestedWidth, 1536)
        compare(unavailableCameraController.requestedHeight, 1024)
        unavailableCameraShell.form.cameraBitDepthSelector.activated(2)
        compare(unavailableCameraController.bitDepthCallCount, 1)
        compare(unavailableCameraController.requestedBitDepth, 16)
        unavailableCameraShell.form.cameraExposureField.text = "4.5"
        unavailableCameraShell.form.cameraExposureField.editingFinished()
        compare(unavailableCameraController.exposureCallCount, 1)
        compare(unavailableCameraController.requestedExposureMs, 4.5)
        unavailableCameraShell.form.cameraReadoutSelector.activated(1)
        compare(unavailableCameraController.readoutCallCount, 1)
        compare(unavailableCameraController.requestedReadoutMode, "Slow")
        unavailableCameraShell.form.previewLutRangeSlider.first.value = 24
        unavailableCameraShell.form.previewLutRangeSlider.first.moved()
        compare(unavailableCameraController.lutCallCount, 1)
        compare(unavailableCameraController.previewLutMinimum, 24)
        compare(unavailableCameraController.previewLutMaximum, 255)

        unavailableCameraShell.form.startCameraButton.clicked()
        compare(unavailableCameraController.startCallCount, 1)
        unavailableCameraController.cameraStatus = "Streaming"
        unavailableCameraController.streaming = true
        wait(0)
        verify(unavailableCameraShell.form.cameraResolutionSelector.enabled)
        verify(unavailableCameraShell.form.cameraBitDepthSelector.enabled)
        verify(unavailableCameraShell.form.cameraExposureField.enabled)
        verify(unavailableCameraShell.form.cameraReadoutSelector.enabled)
        unavailableCameraController.busy = true
        wait(0)
        verify(!unavailableCameraShell.form.cameraResolutionSelector.enabled)
        verify(!unavailableCameraShell.form.cameraBitDepthSelector.enabled)
        verify(!unavailableCameraShell.form.cameraExposureField.enabled)
        verify(!unavailableCameraShell.form.cameraReadoutSelector.enabled)
        unavailableCameraController.busy = false
        unavailableCameraShell.mockState.activeOperation = "imageSequence"
        wait(0)
        verify(!unavailableCameraShell.form.cameraResolutionSelector.enabled)
        verify(!unavailableCameraShell.form.cameraBitDepthSelector.enabled)
        verify(!unavailableCameraShell.form.cameraExposureField.enabled)
        verify(!unavailableCameraShell.form.cameraReadoutSelector.enabled)
        unavailableCameraShell.mockState.activeOperation = ""
        wait(0)
        verify(unavailableCameraShell.form.cameraResolutionSelector.enabled)
        verify(unavailableCameraShell.form.cameraBitDepthSelector.enabled)
        verify(unavailableCameraShell.form.cameraExposureField.enabled)
        verify(unavailableCameraShell.form.cameraReadoutSelector.enabled)
        unavailableCameraShell.form.startCameraButton.clicked()
        compare(unavailableCameraController.stopCallCount, 1)
        unavailableCameraController.cameraStatus = "Unavailable"
        unavailableCameraController.error = "Camera disconnected"
        verify(!unavailableCameraShell.form.cameraPromptVisible)
        compare(unavailableCameraShell.form.cameraErrorText.text, "Camera disconnected")
        verify(unavailableCameraShell.form.cameraErrorText.visible)
        unavailableCameraShell.form.restoreCameraButton.clicked()
        compare(unavailableCameraController.recoverCallCount, 2)
        verify(!unavailableCameraShell.form.cameraErrorText.visible)
        unavailableCameraController.cameraStatus = "Unavailable"
        unavailableCameraController.streaming = false
        unavailableCameraController.configurationAvailable = false
        unavailableCameraController.resolution = ""
        unavailableCameraController.customWidth = ""
        unavailableCameraController.customHeight = ""
        unavailableCameraController.bitDepth = ""
        unavailableCameraController.exposureMs = ""
        unavailableCameraController.readoutMode = ""
        unavailableCameraShell.visible = false
        unavailableCameraShell.mockState.hardwareDrawerOpen = false
    }

    function test_daqEditsApplyImmediatelyOnce() {
        shell.daqController = daqController

        shell.form.daqChannelSelector.currentIndex = 1
        shell.form.daqChannelSelector.activated(1)
        compare(daqController.selectedOutputChannel, "Dev1/ao1")
        compare(daqController.applyCallCount, 1)

        shell.form.daqVppSpinBox.value = 6
        shell.form.daqVppSpinBox.valueModified()
        compare(daqController.amplitudeVpp, 6)
        compare(daqController.applyCallCount, 2)

        shell.form.daqFrequencySpinBox.value = 11
        shell.form.daqFrequencySpinBox.valueModified()
        compare(daqController.frequencyHz, 11000)
        compare(daqController.applyCallCount, 3)

        shell.form.daqEventDurationSpinBox.value = 7
        shell.form.daqEventDurationSpinBox.valueModified()
        compare(daqController.durationMs, 7)
        compare(daqController.applyCallCount, 4)

        shell.form.daqDecisionDelaySpinBox.value = 2
        shell.form.daqDecisionDelaySpinBox.valueModified()
        compare(daqController.delayMs, 2)
        compare(daqController.applyCallCount, 5)
    }

    function test_daqControllerProjectionRefreshDoesNotApply() {
        shell.daqController = daqController
        daqController.amplitudeVpp = 8
        daqController.frequencyHz = 12000
        daqController.durationMs = 9
        daqController.delayMs = 3
        daqController.selectedOutputChannel = "Dev1/ao1"

        compare(shell.form.daqVppSpinBox.value, 8)
        compare(shell.form.daqFrequencySpinBox.value, 12)
        compare(shell.form.daqEventDurationSpinBox.value, 9)
        compare(shell.form.daqDecisionDelaySpinBox.value, 3)
        compare(shell.form.daqChannelSelector.currentIndex, 1)
        compare(daqController.applyCallCount, 0)
    }

    function test_daqInvalidUnavailableAndLockedControlsDoNotApply() {
        shell.daqController = daqController
        daqController.canApply = false
        daqController.daqStatus = "Unavailable"
        daqController.error = "No DAQ analog-output channels were found."

        verify(shell.form.daqRefreshDevicesButton.enabled)
        verify(!shell.form.daqChannelSelector.enabled)
        verify(!shell.form.daqVppSpinBox.enabled)
        verify(!shell.form.daqFrequencySpinBox.enabled)
        verify(!shell.form.daqEventDurationSpinBox.enabled)
        verify(!shell.form.daqDecisionDelaySpinBox.enabled)
        verify(shell.form.daqStatusText.text.indexOf("No DAQ analog-output channels") !== -1)

        shell.form.daqRefreshDevicesButton.clicked()
        compare(daqController.refreshDevicesCallCount, 1)
        compare(daqController.applyCallCount, 0)

        shell.form.daqVppSpinBox.value = 10
        shell.form.daqVppSpinBox.valueModified()
        compare(daqController.applyCallCount, 0)

        daqController.daqStatus = "Ready"
        daqController.error = "DAQ settings are locked by another operation."
        verify(shell.form.daqStatusText.text.indexOf("locked by another operation") !== -1)
        shell.form.daqChannelSelector.activated(1)
        compare(daqController.applyCallCount, 0)
    }

    function labelCropDelegate(recordId) {
        const grid = shell.form.labelWorkspace.cropGridHost
        grid.forceLayout()
        for (let index = 0; index < grid.count; ++index) {
            const item = grid.itemAtIndex(index)
            if (item !== null && item["recordId"] === recordId)
                return item
        }
        return null
    }

    function test_cameraPreviewReadyAcknowledgesActiveSurface() {
        const firstSource =
                "data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='1' height='1'%3E%3Crect width='1' height='1' fill='%23ff0000'/%3E%3C/svg%3E"
        const secondSource =
                "data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='1' height='1'%3E%3Crect width='1' height='1' fill='%230000ff'/%3E%3C/svg%3E"
        unavailableCameraShell.visible = true
        unavailableCameraController.cameraStatus = "Connected"
        unavailableCameraController.previewReadyCallCount = 0
        unavailableCameraController.lastPreviewReadySource = ""
        unavailableCameraController.previewSource = firstSource

        tryCompare(unavailableCameraShell.form.cameraPreviewImage,
                   "status", Image.Ready)
        tryCompare(unavailableCameraController, "previewReadyCallCount", 1)
        compare(unavailableCameraController.lastPreviewReadySource, firstSource)
        verify(unavailableCameraShell.form.cameraPreviewImage.retainWhileLoading)

        unavailableCameraShell.form.navLiveButton.clicked()
        tryCompare(unavailableCameraShell.form, "selectedWorkspace", "live")
        unavailableCameraController.previewSource = secondSource
        tryCompare(unavailableCameraShell.form.liveWorkspace.cameraPreviewImage,
                   "status", Image.Ready)
        tryCompare(unavailableCameraController, "previewReadyCallCount", 2)
        compare(unavailableCameraController.lastPreviewReadySource, secondSource)
        verify(unavailableCameraShell.form.liveWorkspace.cameraPreviewImage
                   .retainWhileLoading)

        unavailableCameraController.previewSource = ""
        unavailableCameraShell.mockState.selectedWorkspace = "capture"
        unavailableCameraShell.visible = false
    }

    function verifyLabelFilterSelection(expectedFilter, expectedButton) {
        const workspace = shell.form.labelWorkspace
        const buttons = [
            workspace.allFilterButton,
            workspace.class0FilterButton,
            workspace.class1FilterButton,
            workspace.class2FilterButton,
            workspace.excludedFilterButton,
            workspace.unreviewedFilterButton
        ]
        let checkedCount = 0
        compare(workspace.currentFilter, expectedFilter)
        for (let index = 0; index < buttons.length; ++index) {
            const selected = buttons[index] === expectedButton
            compare(buttons[index].checked, selected)
            compare(buttons[index].visualRole, selected ? "primary" : "secondary")
            if (buttons[index].checked)
                ++checkedCount
        }
        compare(checkedCount, 1)
    }

    function test_chooserDefaultsUseStorageRootAndSpecificFolder() {
        const storageRoot = textSizeController.storageRoot.toString()
        compare(shell.modelImportFolderDialog.currentFolder.toString(), storageRoot)
        compare(shell.modelExportFolderDialog.currentFolder.toString(), storageRoot)
        compare(shell.modelDuplicateFolderDialog.currentFolder.toString(), storageRoot)

        shell.sequenceTestController = sequenceTestController
        compare(shell.sequenceTestOutputFolderDialog.currentFolder.toString(),
                sequenceTestController.outputFolderUrl.toString())
        sequenceTestController.outputFolderUrl = ""
        compare(shell.sequenceTestOutputFolderDialog.currentFolder.toString(),
                storageRoot)
    }

    function test_modelLibraryControllerWiring() {
        compare(modelLibraryController.refreshCallCount, 1)
        shell.modelLibraryController = modelLibraryController
        shell.modelImportFolderDialog.options = FolderDialog.DontUseNativeDialog
        shell.modelExportFolderDialog.options = FolderDialog.DontUseNativeDialog
        shell.modelDuplicateFolderDialog.options = FolderDialog.DontUseNativeDialog
        shell.form.navLibraryButton.clicked()

        tryCompare(shell.form.modelLibraryWorkspace.modelListView, "count", 2)
        compare(shell.form.modelLibraryWorkspace.presentation, "ready")
        compare(shell.form.modelLibraryWorkspace.modelRows[1].name, "Candidate Model")
        verify(!shell.form.modelLibraryWorkspace.hasSelection)
        verify(shell.form.modelLibraryWorkspace.importButton.enabled)
        verify(!shell.form.modelLibraryWorkspace.exportButton.enabled)
        verify(!shell.form.modelLibraryWorkspace.duplicateButton.enabled)
        verify(!shell.form.modelLibraryWorkspace.deleteButton.enabled)

        shell.form.modelLibraryWorkspace.importButton.clicked()
        tryVerify(function() { return shell.modelImportFolderDialog.visible })
        shell.modelImportFolderDialog.close()
        shell.importModelPackage("file:///C:/Packages/imported-model")
        compare(modelLibraryController.importCallCount, 1)
        compare(modelLibraryController.importArgument.toString(),
                "file:///C:/Packages/imported-model")

        shell.mockState.selectedWorkspace = "library"
        shell.form.modelLibraryWorkspace.openInModelTestButton.clicked()
        compare(shell.mockState.selectedWorkspace, "library")

        tryVerify(function() {
            return shell.form.modelLibraryWorkspace.modelListView.itemAtIndex(1) !== null
        })
        const candidate = shell.form.modelLibraryWorkspace.modelListView.itemAtIndex(1)
        mouseClick(candidate)
        compare(modelLibraryController.selectCallCount, 1)
        compare(modelLibraryController.selectedIndex, 1)
        compare(shell.form.modelLibraryWorkspace.selectedModelIndex, 1)
        compare(shell.form.modelLibraryWorkspace.selectedModelName, "Candidate Model")
        compare(shell.form.modelLibraryWorkspace.selectedModelArchitecture,
                "mobilenet_v3_small")
        compare(shell.form.modelLibraryWorkspace.selectedModelPerformanceLabel, "Faster")
        compare(shell.form.modelLibraryWorkspace.selectedModelClassSummary,
                "Empty, Single, MoreThanOne")
        verify(typeof modelLibraryController.selectedDetail.sourceDataset === "undefined")
        verify(typeof modelLibraryController.selectedDetail.trainingMetrics === "undefined")
        compare(shell.form.modelLibraryWorkspace.selectedModelSourceDataset, "")
        compare(shell.form.modelLibraryWorkspace.selectedModelCreationDate, "2026-07-25")
        compare(shell.form.modelLibraryWorkspace.selectedModelPackageLocation,
                "C:/OpenDSS/Models/model-candidate")
        compare(shell.form.modelLibraryWorkspace.selectedModelTrainingMetrics, "")
        verify(shell.form.modelLibraryWorkspace.exportButton.enabled)
        verify(shell.form.modelLibraryWorkspace.duplicateButton.enabled)
        verify(shell.form.modelLibraryWorkspace.deleteButton.enabled)
        modelLibraryController.operationInProgress = true
        verify(!shell.form.modelLibraryWorkspace.importButton.enabled)
        verify(!shell.form.modelLibraryWorkspace.exportButton.enabled)
        verify(!shell.form.modelLibraryWorkspace.duplicateButton.enabled)
        verify(!shell.form.modelLibraryWorkspace.deleteButton.enabled)
        modelLibraryController.operationInProgress = false

        shell.form.modelLibraryWorkspace.exportButton.clicked()
        tryVerify(function() { return shell.modelExportFolderDialog.visible })
        shell.modelExportFolderDialog.close()
        shell.exportSelectedModel("file:///C:/Exports")
        compare(modelLibraryController.exportCallCount, 1)
        compare(modelLibraryController.exportArgument.toString(),
                "file:///C:/Exports")

        shell.form.modelLibraryWorkspace.duplicateButton.clicked()
        tryVerify(function() { return shell.modelDuplicateDialog.opened })
        compare(shell.modelDuplicateNameField.text, "Candidate Model Copy")
        shell.modelDuplicateNameField.text = "Independent Copy"
        shell.modelDuplicateDialog.accept()
        tryVerify(function() { return shell.modelDuplicateFolderDialog.visible })
        shell.modelDuplicateFolderDialog.close()
        shell.duplicateSelectedModel("Independent Copy", "file:///C:/Duplicates")
        compare(modelLibraryController.duplicateCallCount, 1)
        compare(modelLibraryController.duplicateNameArgument, "Independent Copy")
        compare(modelLibraryController.duplicateDestinationArgument.toString(),
                "file:///C:/Duplicates")

        shell.form.modelLibraryWorkspace.deleteButton.clicked()
        tryVerify(function() { return shell.modelDeleteDialog.opened })
        shell.modelDeleteDialog.accept()
        compare(modelLibraryController.deleteCallCount, 1)

        shell.form.modelLibraryWorkspace.renameButton.clicked()
        tryVerify(function() { return shell.modelRenameDialog.opened })
        compare(shell.modelRenameField.text, "Candidate Model")
        shell.modelRenameField.text = "Renamed Candidate"
        shell.modelRenameDialog.accept()
        compare(modelLibraryController.renameCallCount, 1)
        compare(modelLibraryController.renamedTo, "Renamed Candidate")
        compare(shell.form.modelLibraryWorkspace.selectedModelName, "Renamed Candidate")

        verify(!shell.form.modelLibraryWorkspace.openInModelTestButton.enabled)
        shell.mockState.selectedWorkspace = "library"
        shell.form.modelLibraryWorkspace.openInModelTestButton.clicked()
        compare(shell.mockState.selectedWorkspace, "library")
        compare(shell.form.modelTestWorkspace.activeModelText, "model-active")
        compare(modelLibraryController.activeId, "model-active")

        shell.form.modelLibraryWorkspace.setActiveButton.clicked()
        compare(modelLibraryController.setActiveCallCount, 1)
        compare(modelLibraryController.activeId, "model-candidate")
        verify(shell.form.modelLibraryWorkspace.selectedActive)
        verify(shell.form.modelLibraryWorkspace.deleteButton.enabled)
        verify(shell.form.modelLibraryWorkspace.openInModelTestButton.enabled)

        shell.form.modelLibraryWorkspace.openInModelTestButton.clicked()
        compare(shell.mockState.selectedWorkspace, "modelTest")
        compare(shell.form.modelTestWorkspace.activeModelText, "Renamed Candidate")
        compare(shell.form.modelTestWorkspace.blockerText, "No dataset selected")
        compare(modelLibraryController.activeId, "model-candidate")

        shell.form.modelTestWorkspace.selectDatasetButton.clicked()
        compare(shell.form.modelTestWorkspace.activeModelText, "Renamed Candidate")
        compare(shell.form.modelTestWorkspace.presentation, "readyGpu")
        compare(shell.form.modelTestWorkspace.blockerText, "")
        compare(modelLibraryController.activeId, "model-candidate")

        modelLibraryController.errorMessage = "Registry unavailable"
        modelLibraryController.presentation = "error"
        compare(shell.form.modelLibraryWorkspace.presentation, "error")
        verify(shell.form.modelLibraryWorkspace.showError)
    }

    function test_modelLibraryNullControllerFallback() {
        shell.modelLibraryController = null
        shell.form.navLibraryButton.clicked()
        verify(shell.form.modelLibraryWorkspace.importButton.enabled)
        verify(shell.form.modelLibraryWorkspace.exportButton.enabled)
        verify(shell.form.modelLibraryWorkspace.duplicateButton.enabled)
        verify(shell.form.modelLibraryWorkspace.deleteButton.enabled)

        shell.form.modelLibraryWorkspace.candidateModelRowButton.clicked()
        compare(shell.mockState.modelLibraryPresentation, "readySelected")
        shell.form.modelLibraryWorkspace.setActiveButton.clicked()
        compare(shell.mockState.activeModelId, "DropletNet-03")
        shell.form.modelLibraryWorkspace.openInModelTestButton.clicked()
        compare(shell.mockState.selectedWorkspace, "modelTest")
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
        verify(shell.form.navCaptureButton.checked)
        shell.form.navCaptureButton.clicked()
        compare(shell.form.selectedWorkspace, "capture")
        verify(shell.form.navCaptureButton.checked)
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

    function test_runtimeCaptureFreezesInputsAndDisclosure() {
        shell.singleImageCaptureController = singleImageCaptureController
        shell.mockState.singleImageOpen = true
        singleImageCaptureController.presentation = "capturing"

        verify(shell.form.singleImageOpen)
        verify(!shell.form.fileNameField.enabled)
        verify(!shell.form.saveLocationField.enabled)
        verify(!shell.form.browseButton.enabled)

        shell.form.fileNameField.text = "edited-during-capture"
        shell.form.fileNameField.textEdited()
        shell.form.saveLocationField.text = "C:/OpenDSS/Edited"
        shell.form.saveLocationField.textEdited()
        shell.form.browseButton.clicked()
        shell.form.singleImageSection.headingButton.clicked()

        compare(singleImageCaptureController.fileName, "accepted-name")
        compare(singleImageCaptureController.outputPathEditCount, 0)
        verify(shell.form.singleImageOpen)

        singleImageCaptureController.presentation = "completed"
        verify(shell.form.fileNameField.enabled)
        verify(shell.form.saveLocationField.enabled)
        verify(shell.form.browseButton.enabled)

        shell.form.fileNameField.text = "edited-after-completion"
        shell.form.fileNameField.textEdited()
        shell.form.saveLocationField.text = "C:/OpenDSS/Edited"
        shell.form.saveLocationField.textEdited()
        compare(singleImageCaptureController.fileName, "edited-after-completion")
        compare(singleImageCaptureController.outputPathEditCount, 1)
        compare(singleImageCaptureController.requestedOutputPath, "C:/OpenDSS/Edited")

        shell.form.singleImageSection.headingButton.clicked()
        verify(!shell.form.singleImageOpen)
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

    function test_cameraPreviewSourcePort() {
        const selectedWorkspace = shell.form.selectedWorkspace
        const capturePanelExpanded = shell.form.capturePanelExpanded
        const singleImageOpen = shell.form.singleImageOpen
        const cameraStatus = shell.form.cameraStatus
        const captureEnabled = shell.form.captureEnabled

        compare(shell.form.cameraPreviewSource, "")
        verify(!shell.form.cameraPreviewImage.visible)
        verify(shell.form.cameraPreviewPlaceholder.visible)
        compare(shell.form.cameraPreviewPlaceholder.text, "Camera preview")

        shell.form.cameraPreviewSource = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII="
        verify(shell.form.cameraPreviewImage.visible)
        verify(!shell.form.cameraPreviewPlaceholder.visible)
        compare(shell.form.selectedWorkspace, selectedWorkspace)
        compare(shell.form.capturePanelExpanded, capturePanelExpanded)
        compare(shell.form.singleImageOpen, singleImageOpen)
        compare(shell.form.cameraStatus, cameraStatus)
        compare(shell.form.captureEnabled, captureEnabled)

        shell.form.cameraPreviewSource = ""
        verify(!shell.form.cameraPreviewImage.visible)
        verify(shell.form.cameraPreviewPlaceholder.visible)
    }

    function test_workspaceHandoffs() {
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

    function test_liveDisclosureRules() {
        shell.form.navLiveButton.clicked()
        compare(shell.mockState.livePresentation, "ready")
        verify(shell.form.liveWorkspace.setupProfileExpanded)
        verify(shell.form.liveWorkspace.runInformationExpanded)
        verify(shell.form.liveWorkspace.triggerTimingExpanded)
        verify(shell.form.liveWorkspace.outputRecordingExpanded)
        verify(!shell.form.liveWorkspace.runningExpanded)
        verify(!shell.form.liveWorkspace.runningHeadingEnabled)
        verify(!shell.form.liveWorkspace.runningHeadingButton.enabled)

        shell.form.liveWorkspace.setupProfileHeadingButton.clicked()
        shell.form.liveWorkspace.runInformationHeadingButton.clicked()
        shell.form.liveWorkspace.triggerTimingHeadingButton.clicked()
        shell.form.liveWorkspace.outputRecordingHeadingButton.clicked()
        verify(!shell.form.liveWorkspace.setupProfileExpanded)
        verify(!shell.form.liveWorkspace.runInformationExpanded)
        verify(!shell.form.liveWorkspace.triggerTimingExpanded)
        verify(!shell.form.liveWorkspace.outputRecordingExpanded)
        compare(shell.mockState.activeOperation, "")
        compare(shell.mockState.livePresentation, "ready")

        shell.form.liveWorkspace.setupProfileHeadingButton.clicked()
        shell.form.liveWorkspace.runInformationHeadingButton.clicked()
        shell.form.liveWorkspace.triggerTimingHeadingButton.clicked()
        shell.form.liveWorkspace.outputRecordingHeadingButton.clicked()
        verify(shell.form.liveWorkspace.setupProfileExpanded)
        verify(shell.form.liveWorkspace.runInformationExpanded)
        verify(shell.form.liveWorkspace.triggerTimingExpanded)
        verify(shell.form.liveWorkspace.outputRecordingExpanded)

        shell.form.liveWorkspace.sendTestPulseButton.clicked()
        compare(shell.mockState.activeOperation, "")
        compare(shell.mockState.livePresentation, "ready")

        shell.form.liveWorkspace.secondaryActionButton.clicked()
        compare(shell.mockState.livePresentation, "running")
        compare(shell.mockState.activeOperation, "live")
        verify(!shell.form.liveWorkspace.setupProfileExpanded)
        verify(!shell.form.liveWorkspace.runInformationExpanded)
        verify(!shell.form.liveWorkspace.triggerTimingExpanded)
        verify(!shell.form.liveWorkspace.outputRecordingExpanded)
        verify(!shell.form.liveWorkspace.setupProfileHeadingButton.enabled)
        verify(!shell.form.liveWorkspace.runInformationHeadingButton.enabled)
        verify(!shell.form.liveWorkspace.triggerTimingHeadingButton.enabled)
        verify(!shell.form.liveWorkspace.outputRecordingHeadingButton.enabled)
        verify(shell.form.liveWorkspace.runningExpanded)
        verify(shell.form.liveWorkspace.runningHeadingEnabled)
        verify(shell.form.liveWorkspace.runningHeadingButton.enabled)

        shell.form.liveWorkspace.setupProfileHeadingButton.clicked()
        verify(shell.mockState.liveSetupProfileExpanded)
        verify(!shell.form.liveWorkspace.setupProfileExpanded)
        shell.form.liveWorkspace.runningHeadingButton.clicked()
        verify(!shell.mockState.liveRunningExpanded)
        verify(!shell.form.liveWorkspace.runningExpanded)
        shell.form.liveWorkspace.runningHeadingButton.clicked()
        verify(shell.form.liveWorkspace.runningExpanded)

        shell.form.liveWorkspace.primaryActionButton.clicked()
        compare(shell.mockState.livePresentation, "paused")
        verify(shell.form.liveWorkspace.runningExpanded)
        shell.form.liveWorkspace.secondaryActionButton.clicked()
        compare(shell.mockState.livePresentation, "completed")
        compare(shell.mockState.activeOperation, "")
        verify(!shell.form.liveWorkspace.setupProfileExpanded)
        verify(!shell.form.liveWorkspace.setupProfileHeadingButton.enabled)
        verify(shell.form.liveWorkspace.runningExpanded)
        verify(shell.form.liveWorkspace.runningHeadingEnabled)
        shell.form.liveWorkspace.runningHeadingButton.clicked()
        verify(!shell.form.liveWorkspace.runningExpanded)
        shell.form.liveWorkspace.runningHeadingButton.clicked()
        verify(shell.form.liveWorkspace.runningExpanded)

        shell.form.liveWorkspace.primaryActionButton.clicked()
        compare(shell.mockState.livePresentation, "ready")
        verify(shell.form.liveWorkspace.setupProfileExpanded)
        verify(!shell.form.liveWorkspace.runningExpanded)
    }

    function test_workspaceLocalHitBoundaryCalibration() {
        const previewSource =
                "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII="
        shell.liveSortingController = liveSortingController
        shell.sequenceTestController = sequenceTestController
        shell.daqController = daqController
        shell.cameraController = unavailableCameraController
        unavailableCameraController.previewSource = previewSource
        shell.form.navLiveButton.clicked()
        const liveInput = shell.form.liveWorkspace.hitBoundaryInputArea
        tryCompare(shell.form.liveWorkspace.cameraPreviewImage,
                   "status", Image.Ready)
        tryVerify(function() { return liveInput.width > 0 })
        tryVerify(function() { return liveInput.height > 0 })
        const liveX = Math.floor(liveInput.width * 0.75)
        const liveY = Math.floor(liveInput.height * 0.25)
        mouseClick(liveInput, liveX, liveY, Qt.LeftButton)
        tryCompare(shell.form.liveWorkspace, "hitBoundaryDefined", true)
        const liveOverlay = liveInput.parent
        const liveLine = liveOverlay.children[1]
        verify(!!liveLine, "Object exists")
        tryCompare(liveLine, "x", 0)
        tryCompare(liveLine, "width", liveX)
        shell.form.liveWorkspace.bottomIsHitControl.clicked()
        compare(shell.liveHitBoundarySide, "bottom")

        liveSortingController.presentation = "running"
        tryCompare(liveInput, "enabled", true)
        const runningLiveX = Math.floor(liveInput.width * 0.2)
        const runningLiveY = Math.floor(liveInput.height * 0.8)
        mouseClick(liveInput, runningLiveX, runningLiveY, Qt.LeftButton)
        tryCompare(liveLine, "x", 0)
        tryCompare(liveLine, "width", runningLiveX)
        const runningLiveXRatio = shell.liveHitBoundaryXRatio
        const runningLiveYRatio = shell.liveHitBoundaryYRatio
        compare(liveSortingController.primaryActionCallCount, 0)
        compare(liveSortingController.secondaryActionCallCount, 0)
        compare(liveSortingController.saveProfileCallCount, 0)

        sequenceTestController.previewUrl = previewSource
        shell.form.navSequenceTestButton.clicked()
        const sequenceInput =
                shell.form.sequenceTestWorkspace.hitBoundaryInputArea
        tryCompare(shell.form.sequenceTestWorkspace.sequencePreviewImage,
                   "status", Image.Ready)
        tryVerify(function() { return sequenceInput.width > 0 })
        tryVerify(function() { return sequenceInput.height > 0 })
        const sequenceX = Math.floor(sequenceInput.width * 0.6)
        const sequenceY = Math.floor(sequenceInput.height * 0.4)
        mouseClick(sequenceInput, sequenceX, sequenceY, Qt.LeftButton)
        tryCompare(shell.form.sequenceTestWorkspace,
                   "hitBoundaryDefined", true)
        const sequenceOverlay = sequenceInput.parent
        const sequenceLine = sequenceOverlay.children[1]
        verify(!!sequenceLine, "Object exists")
        tryCompare(sequenceLine, "x", 0)
        tryVerify(function() {
            return Math.abs(sequenceLine.width - sequenceX) <= 0.51
        })
        tryCompare(shell, "liveHitBoundaryXRatio", runningLiveXRatio)
        tryCompare(shell, "liveHitBoundaryYRatio", runningLiveYRatio)
        shell.form.sequenceTestWorkspace.bottomIsHitControl.clicked()
        compare(shell.sequenceHitBoundarySide, "bottom")
        compare(sequenceTestController.startCallCount, 0)
        compare(sequenceTestController.stopCallCount, 0)
        compare(daqController.refreshDevicesCallCount, 0)
        compare(daqController.applyCallCount, 0)
        compare(daqController.sendTestSineWaveCallCount, 0)
        compare(daqController.toggleContinuousWaveformCallCount, 0)

        sequenceTestController.previewUrl = ""
        unavailableCameraController.previewSource = ""
        shell.cameraController = null
    }

    function test_trainAndModelTestDisclosures() {
        shell.form.navTrainButton.clicked()
        verify(shell.form.trainWorkspace.trainingSetupExpanded)
        shell.form.trainWorkspace.trainingSetupHeadingButton.clicked()
        verify(!shell.mockState.trainingSetupExpanded)
        verify(!shell.form.trainWorkspace.trainingSetupExpanded)
        compare(shell.mockState.trainPresentation, "empty")
        compare(shell.mockState.activeOperation, "")
        shell.form.trainWorkspace.trainingSetupHeadingButton.clicked()
        verify(shell.form.trainWorkspace.trainingSetupExpanded)

        shell.mockState.trainPresentation = "running"
        shell.mockState.activeOperation = "training"
        verify(shell.form.trainWorkspace.showRunning)
        shell.form.trainWorkspace.trainingStatusHeadingButton.clicked()
        verify(!shell.mockState.trainingStatusExpanded)
        verify(!shell.form.trainWorkspace.trainingStatusExpanded)
        shell.form.trainWorkspace.trainingStatusHeadingButton.clicked()
        verify(shell.form.trainWorkspace.trainingStatusExpanded)
        shell.mockState.trainPresentation = "completed"
        shell.mockState.activeOperation = ""
        verify(shell.form.trainWorkspace.showCompleted)
        verify(shell.form.trainWorkspace.openInModelTestButton !== null)
        verify(shell.form.trainWorkspace.operationPanelToggleButton !== null)
        shell.form.trainWorkspace.operationPanelToggleButton.clicked()
        verify(!shell.form.trainWorkspace.operationPanelExpanded)
        shell.form.trainWorkspace.operationPanelToggleButton.clicked()
        verify(shell.form.trainWorkspace.operationPanelExpanded)

        shell.form.navModelTestButton.clicked()
        verify(shell.form.modelTestWorkspace.modelTestSetupExpanded)
        shell.form.modelTestWorkspace.modelTestSetupHeadingButton.clicked()
        verify(!shell.mockState.modelTestSetupExpanded)
        verify(!shell.form.modelTestWorkspace.modelTestSetupExpanded)
        compare(shell.mockState.modelTestPresentation, "empty")
        compare(shell.mockState.activeOperation, "")
        shell.form.modelTestWorkspace.modelTestSetupHeadingButton.clicked()
        verify(shell.form.modelTestWorkspace.modelTestSetupExpanded)

        shell.mockState.modelTestPresentation = "running"
        shell.mockState.activeOperation = "modelTest"
        verify(shell.form.modelTestWorkspace.showRunning)
        shell.form.modelTestWorkspace.modelTestStatusHeadingButton.clicked()
        verify(!shell.mockState.modelTestStatusExpanded)
        verify(!shell.form.modelTestWorkspace.modelTestStatusExpanded)
        shell.form.modelTestWorkspace.modelTestStatusHeadingButton.clicked()
        verify(shell.form.modelTestWorkspace.modelTestStatusExpanded)
        shell.mockState.modelTestPresentation = "completedTwoClass"
        shell.mockState.activeOperation = ""
        verify(shell.form.modelTestWorkspace.showCompleted)
        verify(shell.form.modelTestWorkspace.openPredictionsButton !== null)
        verify(shell.form.modelTestWorkspace.openSummaryButton !== null)
        shell.form.modelTestWorkspace.openPredictionsButton.clicked()
        compare(shell.mockState.modelTestOutputLocationDraft, "Illustrative mock — no file opened")
        shell.form.modelTestWorkspace.operationPanelToggleButton.clicked()
        verify(!shell.form.modelTestWorkspace.operationPanelExpanded)
        shell.form.modelTestWorkspace.operationPanelToggleButton.clicked()
        verify(shell.form.modelTestWorkspace.operationPanelExpanded)
    }

    function test_libraryAndSequenceTestDisclosures() {
        shell.form.navLibraryButton.clicked()
        verify(shell.form.modelLibraryWorkspace.selectedModelExpanded)
        shell.form.modelLibraryWorkspace.selectedModelHeadingButton.clicked()
        verify(!shell.mockState.selectedModelExpanded)
        verify(!shell.form.modelLibraryWorkspace.selectedModelExpanded)
        compare(shell.mockState.modelLibraryPresentation, "readySelected")
        compare(shell.mockState.activeOperation, "")
        shell.form.modelLibraryWorkspace.selectedModelHeadingButton.clicked()
        verify(shell.form.modelLibraryWorkspace.selectedModelExpanded)

        shell.form.navSequenceTestButton.clicked()
        verify(shell.form.sequenceTestWorkspace.sequenceTestExpanded)
        shell.form.sequenceTestWorkspace.sequenceTestHeadingButton.clicked()
        verify(!shell.mockState.sequenceTestExpanded)
        verify(!shell.form.sequenceTestWorkspace.sequenceTestExpanded)
        compare(shell.mockState.sequenceTestPresentation, "empty")
        compare(shell.mockState.activeOperation, "")
        shell.form.sequenceTestWorkspace.sequenceTestHeadingButton.clicked()
        verify(shell.form.sequenceTestWorkspace.sequenceTestExpanded)
    }

    function test_finalLabelStructureAndActions() {
        shell.form.navLabelButton.clicked()
        shell.form.labelWorkspace.openDatasetButton.clicked()
        compare(shell.mockState.labelPresentation, "ready")
        shell.form.labelWorkspace.twoClassChoice.clicked()
        compare(shell.mockState.labelClassCount, 2)
        verify(shell.form.labelWorkspace.class0Button.visible)
        verify(shell.form.labelWorkspace.class1Button.visible)
        verify(shell.form.labelWorkspace.class2Button.visible)
        verify(!shell.form.labelWorkspace.class2Button.enabled)
        shell.form.labelWorkspace.threeClassChoice.clicked()
        compare(shell.mockState.labelClassCount, 3)
        verify(shell.form.labelWorkspace.class2Button.enabled)

        let labeledBefore = shell.mockState.labelLabeledCount
        shell.form.labelWorkspace.class0Button.clicked()
        compare(shell.mockState.labelLabeledCount, labeledBefore + 1)
        shell.form.labelWorkspace.excludeButton.clicked()
        compare(shell.mockState.labelLabeledCount, labeledBefore + 2)
        shell.form.labelWorkspace.undoButton.clicked()
        compare(shell.mockState.labelLabeledCount, labeledBefore + 1)
        shell.form.labelWorkspace.previousButton.clicked()
        compare(shell.mockState.labelSelectionIndex, 0)
        shell.form.labelWorkspace.nextButton.clicked()
        compare(shell.mockState.labelSelectionIndex, 1)
        shell.form.labelWorkspace.saveAsButton.clicked()
        compare(shell.mockState.labelDatasetName, "Droplet Dataset Copy")

        shell.form.labelWorkspace.datasetSummaryHeadingButton.clicked()
        verify(!shell.form.labelWorkspace.datasetSummaryExpanded)
        shell.form.labelWorkspace.labelHeadingButton.clicked()
        verify(!shell.form.labelWorkspace.labelExpanded)
        shell.form.labelWorkspace.filterHeadingButton.clicked()
        verify(!shell.form.labelWorkspace.filterExpanded)
        shell.form.labelWorkspace.filterHeadingButton.clicked()
        verify(shell.form.labelWorkspace.filterExpanded)
        shell.form.labelWorkspace.class0FilterButton.clicked()
        compare(shell.mockState.selectedLabelFilter, "class0")
        shell.form.labelWorkspace.class1FilterButton.clicked()
        compare(shell.mockState.selectedLabelFilter, "class1")
        shell.form.labelWorkspace.class2FilterButton.clicked()
        compare(shell.mockState.selectedLabelFilter, "class2")
        verifyLabelFilterSelection("class2", shell.form.labelWorkspace.class2FilterButton)
        shell.form.labelWorkspace.excludedFilterButton.clicked()
        compare(shell.mockState.selectedLabelFilter, "excluded")
        shell.form.labelWorkspace.unreviewedFilterButton.clicked()
        compare(shell.mockState.selectedLabelFilter, "unreviewed")
        shell.form.labelWorkspace.allFilterButton.clicked()
        compare(shell.mockState.selectedLabelFilter, "all")
        verifyLabelFilterSelection("all", shell.form.labelWorkspace.allFilterButton)
        shell.form.labelWorkspace.rightPanelToggleButton.clicked()
        verify(!shell.form.labelWorkspace.rightPanelExpanded)
        shell.form.labelWorkspace.rightPanelToggleButton.clicked()
        verify(shell.form.labelWorkspace.rightPanelExpanded)
        verify(typeof shell.form.labelWorkspace.useInTrainButton === "undefined")
        verify(typeof shell.form.labelWorkspace.selectedCropHeadingButton === "undefined")
        verify(typeof shell.form.labelWorkspace.classesFilterHeadingButton === "undefined")
        verify(typeof shell.form.trainWorkspace.trainingResultsHeadingButton === "undefined")
        verify(typeof shell.form.modelTestWorkspace.modelTestResultsHeadingButton === "undefined")
    }

    function test_labelControllerDirectWiring() {
        shell.datasetLabelController = labelController
        shell.form.navLabelButton.clicked()

        tryCompare(shell.form.labelWorkspace, "presentation", "ready")
        compare(shell.form.labelWorkspace.datasetName, "fixture-dataset")
        compare(shell.form.labelWorkspace.totalCount, 2)
        compare(shell.form.labelWorkspace.labeledCount, 1)
        compare(shell.form.labelWorkspace.class0Count, 1)
        compare(shell.form.labelWorkspace.unreviewedCount, 1)
        compare(shell.form.labelWorkspace.classNames[1], "Single cell")
        compare(shell.form.labelWorkspace.class0Button.text, "Empty")
        compare(shell.form.labelWorkspace.class1Button.text, "Single cell")
        compare(shell.form.labelWorkspace.class2Button.text, "Multiple cells")
        compare(shell.form.labelWorkspace.class0FilterButton.text, "Empty (1)")
        compare(shell.form.labelWorkspace.class1FilterButton.text, "Single cell (0)")
        compare(shell.form.labelWorkspace.class2FilterButton.text, "Multiple cells (0)")
        const cropGrid = shell.form.labelWorkspace.cropGridHost
        compare(cropGrid.model, labelController)
        compare(cropGrid.count, 2)
        verify(cropGrid.delegate !== null)
        compare(shell.form.labelWorkspace.selectedCropId, "r1")
        compare(shell.form.labelWorkspace.selectedCropIndex, 0)
        compare(shell.form.labelWorkspace.selectedCropSource.toString(),
                labelController.selectedCropUrl.toString())
        verify(shell.form.labelWorkspace.canUndo)
        const firstCrop = labelCropDelegate("r1")
        verify(firstCrop !== null)
        compare(firstCrop.width, shell.form.labelWorkspace.cropGridHost.cellWidth)
        compare(firstCrop.height, shell.form.labelWorkspace.cropGridHost.cellHeight)
        compare(firstCrop.width, 185)
        compare(firstCrop.height, 185)
        const firstCropBorder = findChild(firstCrop, "labelCropBorderLayer")
        const firstSelectionIndicator =
                findChild(firstCrop, "labelCropSelectionIndicator")
        const firstFocusRing =
                findChild(firstCrop, "labelCropKeyboardFocusRing")
        verify(firstCropBorder !== null)
        verify(firstSelectionIndicator !== null)
        verify(firstFocusRing !== null)
        compare(firstCropBorder.border.width, 6)
        verify(firstSelectionIndicator.visible)
        verify(firstCropBorder !== firstSelectionIndicator)
        verify(firstCropBorder !== firstFocusRing)
        verify(firstSelectionIndicator !== firstFocusRing)
        cropGrid.positionViewAtIndex(1, GridView.Contain)
        tryVerify(function() { return labelCropDelegate("r2") !== null })
        const secondCrop = labelCropDelegate("r2")
        verify(secondCrop !== null)
        const secondCropBorder = findChild(secondCrop, "labelCropBorderLayer")
        const secondSelectionIndicator =
                findChild(secondCrop, "labelCropSelectionIndicator")
        const secondFocusRing =
                findChild(secondCrop, "labelCropKeyboardFocusRing")
        verify(secondCropBorder !== null)
        verify(secondSelectionIndicator !== null)
        verify(secondFocusRing !== null)
        compare(secondCropBorder.border.width, 6)
        verify(!secondSelectionIndicator.visible)
        verify(!secondFocusRing.visible)
        secondCrop.forceActiveFocus()
        tryCompare(secondFocusRing, "visible", true)
        compare(secondCropBorder.border.width, 6)
        verify(!secondSelectionIndicator.visible)

        labelController.classNames = ["", "Target"]
        wait(0)
        compare(shell.form.labelWorkspace.class0Button.text, "Class 0")
        compare(shell.form.labelWorkspace.class1Button.text, "Target")
        compare(shell.form.labelWorkspace.class2Button.text, "Class 2")
        compare(shell.form.labelWorkspace.class0FilterButton.text, "Class 0 (1)")
        compare(shell.form.labelWorkspace.class1FilterButton.text, "Target (0)")
        compare(shell.form.labelWorkspace.class2FilterButton.text, "Class 2 (0)")
        labelController.classNames = ["Empty", "Single cell", "Multiple cells"]

        labelController.errorMessage = "Dataset is in use by Training"
        compare(shell.form.labelWorkspace.errorMessage, "Dataset is in use by Training")
        labelController.errorMessage = ""

        shell.form.labelWorkspace.twoClassChoice.clicked()
        compare(labelController.configuredClassCount, 2)
        shell.form.labelWorkspace.threeClassChoice.clicked()
        compare(labelController.configuredClassCount, 3)

        shell.form.labelWorkspace.class1Button.clicked()
        compare(labelController.assignedClassId, "1")
        shell.form.labelWorkspace.excludeButton.clicked()
        compare(labelController.excludeCallCount, 1)
        shell.form.labelWorkspace.undoButton.clicked()
        compare(labelController.undoCallCount, 1)
        shell.form.labelWorkspace.previousButton.clicked()
        compare(labelController.previousCallCount, 1)
        shell.form.labelWorkspace.nextButton.clicked()
        compare(labelController.nextCallCount, 1)
        labelController.filter = "excluded"
        verifyLabelFilterSelection("excluded",
                                   shell.form.labelWorkspace.excludedFilterButton)
        shell.form.labelWorkspace.unreviewedFilterButton.clicked()
        compare(labelController.filterArgument, "unreviewed")
        verifyLabelFilterSelection("unreviewed",
                                   shell.form.labelWorkspace.unreviewedFilterButton)
        mouseClick(shell.form.labelWorkspace.unreviewedFilterButton)
        compare(labelController.filter, "unreviewed")
        compare(labelController.filterArgument, "unreviewed")
        verifyLabelFilterSelection("unreviewed",
                                   shell.form.labelWorkspace.unreviewedFilterButton)
        shell.form.labelWorkspace.unreviewedFilterButton.forceActiveFocus()
        keyClick(Qt.Key_Space)
        compare(labelController.filter, "unreviewed")
        compare(labelController.filterArgument, "unreviewed")
        verifyLabelFilterSelection("unreviewed",
                                   shell.form.labelWorkspace.unreviewedFilterButton)

        shell.form.labelWorkspace.class1NameField.text = "Single"
        shell.form.labelWorkspace.class1NameField.editingFinished()
        compare(labelController.renamedClassIndex, 1)
        compare(labelController.renamedClassName, "Single")

        cropGrid.positionViewAtIndex(0, GridView.Contain)
        tryVerify(function() { return labelCropDelegate("r1") !== null })
        const crop = labelCropDelegate("r1")
        mouseClick(crop)
        tryCompare(labelController, "selectedRecordArgument", "r1")
        tryCompare(labelController, "selectCallCount", 1)
        crop.forceActiveFocus()
        keyClick(Qt.Key_Return)
        compare(labelController.selectCallCount, 2)

        for (let index = 2; index < 1000; ++index) {
            labelController.append({
                recordId: "r" + (index + 1),
                cropUrl: "",
                state: "unreviewed",
                selected: false
            })
        }
        tryCompare(cropGrid, "count", 1000)
        let instantiatedDelegates = 0
        for (let index = 0; index < cropGrid.count; ++index) {
            if (cropGrid.itemAtIndex(index) !== null)
                ++instantiatedDelegates
        }
        verify(instantiatedDelegates < cropGrid.count)
        const visibleColumns = Math.max(
                                 1, Math.floor(cropGrid.width
                                               / cropGrid.cellWidth))
        const visibleRows = Math.max(
                              1, Math.ceil(cropGrid.height
                                           / cropGrid.cellHeight))
        verify(instantiatedDelegates
               <= visibleColumns * (visibleRows + 2))
    }

    function test_outerPanelToggleKeepsWorkspace() {
        shell.form.navCaptureButton.clicked()
        mouseClick(shell.form.capturePanelToggleButton)
        tryCompare(shell.form, "capturePanelExpanded", false)
        compare(shell.form.selectedWorkspace, "capture")
        mouseClick(shell.form.capturePanelToggleButton)
        tryCompare(shell.form, "capturePanelExpanded", true)

        shell.form.navLabelButton.clicked()
        shell.form.labelWorkspace.rightPanelToggleButton.clicked()
        verify(!shell.form.labelWorkspace.rightPanelExpanded)
        compare(shell.form.selectedWorkspace, "label")

        shell.form.navTrainButton.clicked()
        shell.form.trainWorkspace.operationPanelToggleButton.clicked()
        verify(!shell.form.trainWorkspace.operationPanelExpanded)
        compare(shell.form.selectedWorkspace, "train")

        shell.form.navModelTestButton.clicked()
        shell.form.modelTestWorkspace.operationPanelToggleButton.clicked()
        verify(!shell.form.modelTestWorkspace.operationPanelExpanded)
        compare(shell.form.selectedWorkspace, "modelTest")
    }

    function test_remainingOuterPanelTogglesKeepWorkspaceAndDisclosures() {
        shell.form.navLibraryButton.clicked()
        verify(shell.form.modelLibraryWorkspace.rightPanelExpanded)
        verify(shell.form.modelLibraryWorkspace.selectedModelExpanded)
        mouseClick(shell.form.modelLibraryWorkspace.rightPanelToggleButton)
        tryCompare(shell.form.modelLibraryWorkspace, "rightPanelExpanded", false)
        tryCompare(shell.form, "selectedWorkspace", "library")
        verify(shell.form.modelLibraryWorkspace.selectedModelExpanded)
        shell.form.modelLibraryWorkspace.rightPanelToggleButton.clicked()
        verify(shell.form.modelLibraryWorkspace.rightPanelExpanded)

        shell.form.navLiveButton.clicked()
        verify(shell.form.liveWorkspace.rightPanelExpanded)
        verify(shell.form.liveWorkspace.setupProfileExpanded)
        shell.form.liveWorkspace.rightPanelToggleButton.clicked()
        verify(!shell.form.liveWorkspace.rightPanelExpanded)
        compare(shell.form.selectedWorkspace, "live")
        verify(shell.form.liveWorkspace.setupProfileExpanded)
        shell.form.liveWorkspace.rightPanelToggleButton.clicked()
        verify(shell.form.liveWorkspace.rightPanelExpanded)

        shell.form.navSequenceTestButton.clicked()
        verify(shell.form.sequenceTestWorkspace.rightPanelExpanded)
        verify(shell.form.sequenceTestWorkspace.sequenceTestExpanded)
        shell.form.sequenceTestWorkspace.rightPanelToggleButton.clicked()
        verify(!shell.form.sequenceTestWorkspace.rightPanelExpanded)
        compare(shell.form.selectedWorkspace, "sequenceTest")
        verify(shell.form.sequenceTestWorkspace.sequenceTestExpanded)
        shell.form.sequenceTestWorkspace.rightPanelToggleButton.clicked()
        verify(shell.form.sequenceTestWorkspace.rightPanelExpanded)

        shell.form.navRunsButton.clicked()
        verify(shell.form.runsWorkspace.rightPanelExpanded)
        verify(shell.form.runsWorkspace.runsPanelExpanded)
        shell.form.runsWorkspace.rightPanelToggleButton.clicked()
        verify(!shell.form.runsWorkspace.rightPanelExpanded)
        compare(shell.form.selectedWorkspace, "runs")
        verify(shell.form.runsWorkspace.runsPanelExpanded)
        shell.form.runsWorkspace.rightPanelToggleButton.clicked()
        verify(shell.form.runsWorkspace.rightPanelExpanded)
    }

    function test_textSizeProjection() {
        compare(textSizeController.textSizePercent, 100)
        compare(Constants.textSizePercent, 100)
        textSizeController.textSizePercent = 80
        compare(shell.form.settingsWorkspace.textSizeSelector.currentIndex, 0)
        shell.form.settingsWorkspace.textSizeSelector.activated(0)
        compare(textSizeController.lastRequestedTextSizePercent, 80)
        compare(textSizeController.textSizePercent, 80)
        compare(Constants.textSizePercent, 80)
        textSizeController.textSizePercent = 100
        compare(shell.form.settingsWorkspace.textSizeSelector.currentIndex, 1)
        shell.form.settingsWorkspace.textSizeSelector.activated(1)
        compare(textSizeController.lastRequestedTextSizePercent, 100)
        textSizeController.textSizePercent = 125
        compare(shell.form.settingsWorkspace.textSizeSelector.currentIndex, 2)
        shell.form.settingsWorkspace.textSizeSelector.activated(2)
        compare(textSizeController.lastRequestedTextSizePercent, 125)
        compare(Constants.textSizePercent, 125)
    }

    function test_settingsStorageControllerWiring() {
        compare(shell.form.settingsWorkspace.defaultDataRoot, "C:/OpenDSS/Settings Root")

        shell.form.settingsWorkspace.openDataRootButton.clicked()
        compare(textSizeController.openStorageRootCallCount, 1)
        compare(shell.form.settingsWorkspace.settingsPresentation, "ready")

        textSizeController.openStorageRootError = "Unable to request opening the storage root."
        shell.form.settingsWorkspace.openDataRootButton.clicked()
        compare(textSizeController.openStorageRootCallCount, 2)
        compare(shell.form.settingsWorkspace.settingsPresentation, "error")

        shell.settingsController = null
        shell.mockState.settingsPresentation = "settingsError"
        compare(shell.form.settingsWorkspace.settingsPresentation, "error")
        compare(shell.form.settingsWorkspace.defaultDataRoot,
                "C:/Users/Scientist/Documents/OpenDropletSortingSuite")
        shell.form.settingsWorkspace.openDataRootButton.clicked()
        compare(textSizeController.openStorageRootCallCount, 2)
    }

    function test_sequenceViewerTransitions() {
        shell.form.navSequenceViewerButton.clicked()
        shell.form.sequenceViewerWorkspace.openSequenceButton.clicked()
        compare(shell.mockState.sequenceViewerPresentation, "firstFrame")
        compare(shell.form.sequenceViewerWorkspace.currentFrame, 1)
        compare(shell.form.sequenceViewerWorkspace.frameSlider.from, 1)
        compare(shell.form.sequenceViewerWorkspace.frameSlider.to, 120)
        verify(shell.form.sequenceViewerWorkspace.jumpBack50Button.enabled)
        verify(shell.form.sequenceViewerWorkspace.jumpBack10Button.enabled)
        verify(shell.form.sequenceViewerWorkspace.jumpForward10Button.enabled)
        verify(shell.form.sequenceViewerWorkspace.jumpForward50Button.enabled)
        verify(shell.form.sequenceViewerWorkspace.zoomOutButton.enabled)
        verify(shell.form.sequenceViewerWorkspace.zoomInButton.enabled)
        verify(shell.form.sequenceViewerWorkspace.fitButton.enabled)
        verify(shell.form.sequenceViewerWorkspace.actualSizeButton.enabled)
        shell.form.sequenceViewerWorkspace.nextButton.clicked()
        compare(shell.mockState.sequenceViewerPresentation, "middleFrame")
        compare(shell.form.sequenceViewerWorkspace.currentFrame, 60)
        shell.form.sequenceViewerWorkspace.directSeekField.text = "42"
        shell.form.sequenceViewerWorkspace.directSeekField.accepted()
        compare(shell.mockState.sequenceViewerPresentation, "middleFrame")
    }

    function test_captureRuntimeFormSeams() {
        shell.form.sequenceNameField.text = "Sequence A"
        shell.form.sequenceExperimentTypeField.text = "Experiment A"
        shell.form.sequenceNotesField.text = "Sequence notes"
        shell.form.sequenceDurationField.text = "10"
        shell.form.datasetNameField.text = "Dataset A"
        shell.form.datasetExperimentTypeField.text = "Experiment B"
        shell.form.datasetNotesField.text = "Dataset notes"
        shell.form.datasetDurationField.text = "20"

        compare(shell.form.sequenceNameField.text, "Sequence A")
        compare(shell.form.sequenceExperimentTypeField.text, "Experiment A")
        compare(shell.form.sequenceNotesField.text, "Sequence notes")
        compare(shell.form.sequenceDurationField.text, "10")
        compare(shell.form.datasetNameField.text, "Dataset A")
        compare(shell.form.datasetExperimentTypeField.text, "Experiment B")
        compare(shell.form.datasetNotesField.text, "Dataset notes")
        compare(shell.form.datasetDurationField.text, "20")
        verify(shell.form.continuousSineWaveButton !== null)

        shell.form.sequenceFrameCount = 37
        shell.form.sequenceElapsedTimeText = "00:00:12"
        shell.form.sequenceCompletionText = "Sequence complete"
        shell.form.sequenceErrorText = "Sequence write failed"
        shell.form.datasetFrameCount = 41
        shell.form.datasetCropCount = 13
        shell.form.datasetElapsedTimeText = "00:00:18"
        shell.form.datasetCompletionText = "Dataset complete"
        shell.form.datasetErrorText = "Dataset write failed"

        compare(shell.form.sequenceFrameCount, 37)
        compare(shell.form.sequenceElapsedTimeText, "00:00:12")
        compare(shell.form.sequenceCompletionText, "Sequence complete")
        compare(shell.form.sequenceErrorText, "Sequence write failed")
        compare(shell.form.datasetFrameCount, 41)
        compare(shell.form.datasetCropCount, 13)
        compare(shell.form.datasetElapsedTimeText, "00:00:18")
        compare(shell.form.datasetCompletionText, "Dataset complete")
        compare(shell.form.datasetErrorText, "Dataset write failed")
    }

    function test_liveCameraPreviewSourcePort() {
        shell.form.navLiveButton.clicked()
        const workspace = shell.form.liveWorkspace
        verify(shell.form.cameraPreviewImage.retainWhileLoading)
        verify(workspace.cameraPreviewImage.retainWhileLoading)
        workspace.presentation = "ready"
        workspace.cameraPreviewSource = ""
        verify(!workspace.cameraPreviewImage.visible)

        workspace.cameraPreviewSource =
                "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII="
        verify(workspace.cameraPreviewImage.visible)

        workspace.presentation = "unavailable"
        verify(!workspace.cameraPreviewImage.visible)
    }

    function test_appStartsMaximized() {
        const component = Qt.createComponent(
                "qrc:/qt/qml/Desktop_app_v2Content/App.qml")
        tryCompare(component, "status", Component.Ready)
        verify(component.status === Component.Ready, component.errorString())

        const appWindow = component.createObject(null)
        verify(appWindow !== null)
        tryCompare(appWindow, "visibility", Window.Maximized)

        appWindow.destroy()
        component.destroy()
    }

    function test_sequenceViewerLocalFilePaths() {
        compare(shell.localFilePath("file:///C:/OpenDSS/sequence.json"),
                "C:/OpenDSS/sequence.json")
        compare(shell.localFilePath("file:///C:/Open%20DSS/sequence.json"),
                "C:/Open DSS/sequence.json")
        compare(shell.localFilePath("file://server/share/sequence.json"),
                "//server/share/sequence.json")
        compare(shell.localFilePath("file:///tmp/OpenDSS/sequence.json"),
                "/tmp/OpenDSS/sequence.json")
        compare(shell.localFilePath("https://example.com/sequence.json"), "")
    }

    function test_trainStartRequirementsAndInterrupt() {
        shell.form.navTrainButton.clicked()
        verify(shell.form.trainWorkspace.datasetClassesPlaceholder.visible)
        verify(shell.form.trainWorkspace.eligibleCropsPlaceholder.visible)
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

    function test_trainingControllerDirectWiring() {
        shell.trainingController = trainingController
        shell.form.navTrainButton.clicked()

        compare(shell.form.trainWorkspace.presentation, "readyGpu")
        compare(shell.form.trainWorkspace.datasetText,
                "C:/OpenDSS/Datasets/fixture/dataset.json")
        compare(shell.form.trainWorkspace.modelNameText, "DropletNet-Test")
        compare(shell.form.trainWorkspace.saveLocationText,
                "C:/OpenDSS/Training Output")
        compare(shell.form.trainWorkspace.requestedDeviceText, "GPU")
        compare(shell.form.trainWorkspace.effectiveDeviceText, "")
        compare(shell.form.trainWorkspace.stageText, "fine_tune")
        compare(shell.form.trainWorkspace.currentEpoch, 5)
        compare(shell.form.trainWorkspace.totalEpochs, 20)
        compare(shell.form.trainWorkspace.overallProgress, 0.25)
        verify(shell.form.trainWorkspace.startEnabled)
        verify(shell.form.trainWorkspace.weightsSelector.enabled)
        verify(shell.form.trainWorkspace.weightsSelector.visible)
        verify(shell.form.trainWorkspace.loadWeightsButton.enabled)
        verify(shell.form.trainWorkspace.loadWeightsButton.visible)
        verify(!shell.form.trainWorkspace.showMetrics)
        verify(!shell.form.trainWorkspace.showTiming)
        verify(!shell.form.trainWorkspace.showActiveModelConfirmation)
        verify(!shell.form.trainWorkspace.showRetrySave)
        verify(!shell.form.trainWorkspace.datasetClassesPlaceholder.visible)
        verify(!shell.form.trainWorkspace.eligibleCropsPlaceholder.visible)

        shell.form.trainWorkspace.architectureSelector.activated(1)
        compare(trainingController.architecture, "efficientnet")
        shell.form.trainWorkspace.trainingDeviceSelector.activated(1)
        compare(trainingController.requestedDevice, "cpu")
        shell.form.trainWorkspace.modelNameField.text = "Renamed Model"
        shell.form.trainWorkspace.modelNameField.textEdited()
        compare(trainingController.modelName, "Renamed Model")
        shell.form.trainWorkspace.startButton.clicked()
        compare(trainingController.startCallCount, 1)

        trainingController.presentation = "running"
        verify(shell.form.trainWorkspace.showRunning)
        verify(!shell.form.trainWorkspace.selectDatasetButton.enabled)
        verify(!shell.form.trainWorkspace.architectureSelector.enabled)
        verify(!shell.form.trainWorkspace.trainingDeviceSelector.enabled)
        verify(!shell.form.trainWorkspace.modelNameField.enabled)
        verify(!shell.form.trainWorkspace.saveLocationField.enabled)
        verify(!shell.form.trainWorkspace.browseButton.enabled)
        verify(shell.form.trainWorkspace.stopButton.enabled)
        shell.form.trainWorkspace.stopButton.clicked()
        compare(trainingController.stopCallCount, 1)

        trainingController.resultDirectoryUrl =
                "file:///C:/OpenDSS/Training%20Output/run-001"
        trainingController.modelOnnxUrl =
                "file:///C:/OpenDSS/Training%20Output/run-001/model.onnx"
        trainingController.metadataUrl =
                "file:///C:/OpenDSS/Training%20Output/run-001/metadata.json"
        trainingController.registeredPackageUrl =
                "file:///C:/OpenDSS/Models/Renamed%20Model"
        trainingController.presentation = "completed"
        verify(shell.form.trainWorkspace.showCompleted)
        compare(shell.form.trainWorkspace.resultPath,
                "C:/OpenDSS/Models/Renamed Model")
        compare(shell.form.trainWorkspace.modelOnnxPath,
                "C:/OpenDSS/Training Output/run-001/model.onnx")
        compare(shell.form.trainWorkspace.metadataPath,
                "C:/OpenDSS/Training Output/run-001/metadata.json")
        verify(shell.form.trainWorkspace.showActiveModelConfirmation)
        verify(shell.form.trainWorkspace.openInModelTestButton.visible)
        verify(shell.form.trainWorkspace.openInModelTestButton.enabled)
        shell.form.trainWorkspace.openInModelTestButton.clicked()
        compare(shell.mockState.selectedWorkspace, "modelTest")

        shell.form.navTrainButton.clicked()
        trainingController.errorMessage = "Model package save failed."
        trainingController.retrySaveAvailable = true
        trainingController.presentation = "saveFailed"
        verify(shell.form.trainWorkspace.showError)
        compare(shell.form.trainWorkspace.errorText, "Model package save failed.")
        verify(shell.form.trainWorkspace.retrySaveButton.visible)
        verify(shell.form.trainWorkspace.retrySaveButton.enabled)
        verify(!shell.form.trainWorkspace.openInModelTestButton.visible)
        shell.form.trainWorkspace.retrySaveButton.clicked()
        compare(trainingController.retrySaveCallCount, 1)
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
        verify(shell.form.modelTestWorkspace.showError)
        verify(!shell.form.modelTestWorkspace.setupVisible)
    }

    function test_modelTestControllerDirectWiring() {
        shell.modelLibraryController = null
        shell.modelTestController = modelTestController
        shell.form.navModelTestButton.clicked()

        verify(shell.form.modelTestWorkspace.serviceFactsOnly)
        compare(shell.form.modelTestWorkspace.activeModelText, "Active Test Model")
        compare(shell.form.modelTestWorkspace.datasetText,
                "C:/OpenDSS/Datasets/fixture/dataset.json")
        compare(shell.form.modelTestWorkspace.outputLocationText,
                "C:/OpenDSS/Model Tests")
        compare(shell.form.modelTestWorkspace.deviceText,
                "Determined at start")
        verify(shell.form.modelTestWorkspace.startEnabled)
        verify(!shell.form.modelTestWorkspace.resultFactsVisible)
        verify(shell.form.modelTestWorkspace.outputLocationField.readOnly)
        verify(!shell.form.modelTestWorkspace.startAnotherButton.visible)

        shell.form.modelTestWorkspace.startButton.clicked()
        compare(modelTestController.startCallCount, 1)
        modelTestController.presentation = "running"
        modelTestController.processedImages = 1
        modelTestController.eligibleImages = 2
        modelTestController.progress = 0.5
        verify(shell.form.modelTestWorkspace.showRunning)
        shell.modelLibraryController = modelLibraryController
        modelLibraryController.select(1)
        verify(shell.form.modelLibraryWorkspace.modelLocked)
        verify(!shell.form.modelLibraryWorkspace.setActiveButton.enabled)
        shell.form.modelLibraryWorkspace.setActiveButton.clicked()
        compare(modelLibraryController.setActiveCallCount, 0)
        compare(shell.form.modelTestWorkspace.processedCount, 1)
        compare(shell.form.modelTestWorkspace.eligibleCount, 2)
        compare(shell.form.modelTestWorkspace.progressValue, 0.5)
        shell.form.modelTestWorkspace.stopButton.clicked()
        compare(modelTestController.stopCallCount, 1)

        modelTestController.resultSummary = {
            status: "completed",
            activeModelId: "model-active",
            activeModelName: "Active Model",
            datasetId: "fixture",
            effectiveDevice: "CPU",
            fallbackWarning: "CUDA unavailable; Auto mode used CPU.",
            eligibleImages: 2,
            processedImages: 2,
            correctPredictions: 1,
            overallAccuracy: 0.5,
            perClass: [
                {classId: "0", support: 1, correct: 1, accuracy: 1.0},
                {classId: "1", support: 1, correct: 0, accuracy: 0.0}
            ],
            confusionMatrix: [[1, 0], [1, 0]]
        }
        modelTestController.summaryUrl =
                "file:///C:/OpenDSS/Model%20Tests/result-001/model_test_summary.json"
        modelTestController.predictionsCsvUrl =
                "file:///C:/OpenDSS/Model%20Tests/result-001/predictions.csv"
        modelTestController.presentation = "completed"
        verify(shell.form.modelTestWorkspace.showCompleted)
        compare(shell.form.modelTestWorkspace.deviceText, "CPU")
        compare(shell.form.modelTestWorkspace.overallAccuracyText, "50.0%")
        verify(shell.form.modelTestWorkspace.perClassAccuracyText.indexOf(
                   "Class 0: 100.0%") >= 0)
        compare(shell.form.modelTestWorkspace.confusionMatrixText, "1  0\n1  0")
        compare(shell.form.modelTestWorkspace.predictionSummaryText,
                "Processed 2 of 2; correct 1")
        verify(shell.form.modelTestWorkspace.openSummaryButton.enabled)
        verify(shell.form.modelTestWorkspace.openPredictionsButton.enabled)
        shell.form.modelTestWorkspace.openSummaryButton.clicked()
        shell.form.modelTestWorkspace.openPredictionsButton.clicked()
        compare(modelTestController.openSummaryCallCount, 1)
        compare(modelTestController.openPredictionsCallCount, 1)
        modelTestController.actionError = "Model Test summary is unavailable."
        compare(shell.form.modelTestWorkspace.actionErrorText,
                "Model Test summary is unavailable.")
        verify(shell.form.modelTestWorkspace.showCompleted)
        verify(shell.form.modelTestWorkspace.resultFactsVisible)

        modelTestController.errorMessage = "No Active Model is available."
        modelTestController.presentation = "error"
        verify(shell.form.modelTestWorkspace.showError)
        compare(shell.form.modelTestWorkspace.blockerText,
                "No Active Model is available.")
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

    function test_liveAndSequenceTestUseProductionControllers() {
        shell.liveSortingController = liveSortingController
        shell.sequenceTestController = sequenceTestController
        shell.daqController = daqController
        shell.form.navLiveButton.clicked()
        wait(0)

        compare(shell.form.liveWorkspace.presentation, "ready")
        compare(shell.form.liveWorkspace.runNameField.text, "Production Run")
        compare(shell.form.liveWorkspace.activeModelText, "Production Model")
        compare(shell.form.liveWorkspace.hitBoundaryText,
                "Click the Live preview to set its Hit boundary calibration.")
        compare(shell.form.liveWorkspace.hitClassControl.currentIndex, 1)
        compare(shell.form.liveWorkspace.hitClassControl.currentText, "Single")
        shell.form.liveWorkspace.hitClassControl.activated(2)
        compare(liveSortingController.hitClassId, "0")
        compare(shell.form.liveWorkspace.hitClassControl.model[0], "MoreThanOne")
        verify(shell.form.liveWorkspace.integrityStatusText.indexOf("Queue rejections: 2") >= 0)
        verify(shell.form.liveWorkspace.sendTestPulseButton.enabled)
        daqController.continuousWaveformActive = true
        wait(0)
        verify(!shell.form.liveWorkspace.sendTestPulseButton.enabled)
        daqController.continuousWaveformActive = false
        wait(0)
        verify(shell.form.liveWorkspace.sendTestPulseButton.enabled)
        verify(shell.form.liveWorkspace.openProfileButton.enabled)
        verify(shell.form.liveWorkspace.saveProfileButton.enabled)
        verify(shell.form.liveWorkspace.saveProfileAsButton.enabled)
        liveSortingController.profilePath = "C:/OpenDSS/profile.json"
        liveSortingController.profileStatus = "Profile loaded: profile.json"
        wait(0)
        verify(shell.form.liveWorkspace.saveProfileButton.enabled)
        compare(shell.form.liveWorkspace.profileAvailabilityText,
                "Profile loaded: profile.json")
        shell.form.liveWorkspace.saveProfileButton.clicked()
        compare(liveSortingController.saveProfileCallCount, 1)
        shell.form.liveWorkspace.primaryActionButton.clicked()
        compare(liveSortingController.primaryActionCallCount, 1)
        liveSortingController.daqOutputEnabled = true
        wait(0)
        verify(shell.form.liveWorkspace.daqOutputControl.checked)

        liveSortingController.presentation = "running"
        wait(0)
        verify(!shell.form.liveWorkspace.setupProfileExpanded)
        verify(shell.form.liveWorkspace.runningExpanded)
        verify(shell.form.liveWorkspace.runningHeadingEnabled)
        verify(!shell.form.hardwareButton.enabled)
        verify(!shell.form.startCameraButton.enabled)
        verify(!shell.form.daqRefreshDevicesButton.enabled)
        liveSortingController.presentation = "ready"
        wait(0)
        verify(shell.form.liveWorkspace.setupProfileExpanded)
        verify(!shell.form.liveWorkspace.runningExpanded)
        verify(!shell.form.liveWorkspace.runningHeadingEnabled)
        verify(shell.form.hardwareButton.enabled)
        verify(shell.form.startCameraButton.enabled)
        verify(shell.form.daqRefreshDevicesButton.enabled)

        compare(shell.form.sequenceTestWorkspace.presentation, "selected")
        compare(shell.form.sequenceTestWorkspace.sequenceNameField.text, "Test Sequence")
        compare(shell.form.sequenceTestWorkspace.frameCountText.text, "Frames: 24")
        compare(shell.form.sequenceTestWorkspace.hitClassControl.currentIndex, 2)
        compare(shell.form.sequenceTestWorkspace.hitClassControl.currentText, "Single")
        shell.form.sequenceTestWorkspace.hitClassControl.activated(0)
        compare(sequenceTestController.selectedHitClassId, "2")
        compare(shell.form.sequenceTestWorkspace.hitClassControl.model[1], "Empty")
        verify(shell.form.sequenceTestWorkspace.sequenceValidationText.text.indexOf(
                   "Click the Sequence Test preview") >= 0)
        shell.form.sequenceTestWorkspace.loadToMemoryButton.clicked()
        compare(sequenceTestController.loadToMemoryCallCount, 1)

        sequenceTestController.presentation = "ready"
        sequenceTestController.canStart = true
        sequenceTestController.memoryReady = true
        wait(0)
        compare(shell.form.sequenceTestWorkspace.loadReadinessText.text,
                "Load readiness: Ready in memory")
        shell.form.sequenceTestWorkspace.startStopButton.clicked()
        compare(sequenceTestController.startCallCount, 1)

        sequenceTestController.presentation = "running"
        sequenceTestController.physicalDaqOutputEnabled = true
        wait(0)
        verify(!shell.form.daqRefreshDevicesButton.enabled)
        sequenceTestController.physicalDaqOutputEnabled = false
        wait(0)
        verify(shell.form.daqRefreshDevicesButton.enabled)
        shell.form.sequenceTestWorkspace.startStopButton.clicked()
        compare(sequenceTestController.stopCallCount, 1)

        sequenceTestController.errorMessage = "DAQ hardware is not ready."
        sequenceTestController.presentation = "ready"
        sequenceTestController.canStart = false
        sequenceTestController.physicalDaqOutputEnabled = true
        wait(0)
        verify(!shell.form.sequenceTestWorkspace.startStopButton.enabled)
        compare(shell.form.sequenceTestWorkspace.loadReadinessText.text,
                "Load readiness: Ready in memory")
        verify(shell.form.sequenceTestWorkspace.sequenceValidationText.text.indexOf(
                   "DAQ hardware is not ready.") >= 0)

        sequenceTestController.presentation = "error"
        sequenceTestController.errorMessage = "The previous sequence failed."
        wait(0)
        verify(shell.form.sequenceTestWorkspace.loadSequenceButton.enabled)
        shell.form.sequenceTestWorkspace.loadSequenceButton.clicked()
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

    function test_runNotesAndSequenceTestCompletionActions() {
        shell.runsResultsController = runsControllerMock
        shell.sequenceTestController = sequenceTestController
        runsControllerMock.loadedRun = ({ notes: "Persisted notes" })
        runsControllerMock.updateNotesCallCount = 0
        runsControllerMock.openSummaryCallCount = 0

        shell.form.runsWorkspace.editNotesButton.clicked()
        shell.form.runsWorkspace.notesEditor.text = "Updated factual notes"
        shell.form.runsWorkspace.saveNotesButton.clicked()
        compare(runsControllerMock.updateNotesCallCount, 1)
        compare(runsControllerMock.updatedNotes, "Updated factual notes")

        shell.form.runsWorkspace.editNotesButton.clicked()
        shell.form.runsWorkspace.notesEditor.text = "Discarded draft"
        shell.form.runsWorkspace.cancelNotesButton.clicked()
        compare(shell.form.runsWorkspace.notesEditor.text,
                "Updated factual notes")
        runsControllerMock.loadedRun = ({ notes: "Notes from another run" })
        compare(shell.form.runsWorkspace.notesEditor.text,
                "Notes from another run")

        sequenceTestController.presentation = "completed"
        sequenceTestController.runFolderUrl =
                "file:///C:/OpenDSS/Runs/sequence-test"
        sequenceTestController.runSummaryUrl =
                "file:///C:/OpenDSS/Runs/sequence-test/run_summary.json"
        shell.mockState.selectWorkspace("sequenceTest")
        wait(0)
        verify(shell.form.sequenceTestWorkspace.openRunSummaryButton.visible)
        verify(shell.form.sequenceTestWorkspace.openRunFolderButton.visible)
        verify(shell.form.sequenceTestWorkspace.startAnotherTestButton.visible)
        shell.form.sequenceTestWorkspace.openRunSummaryButton.clicked()
        compare(runsControllerMock.openSummaryCallCount, 1)
        compare(shell.mockState.selectedWorkspace, "runs")

        shell.mockState.selectWorkspace("sequenceTest")
        shell.form.sequenceTestWorkspace.startAnotherTestButton.clicked()
        compare(sequenceTestController.startAnotherCallCount, 1)
        compare(sequenceTestController.presentation, "ready")
    }

    function test_productionCaptureControllerActions() {
        shell.captureWorkflowController = captureWorkflowController
        shell.mockState.imageSequenceOpen = true
        shell.form.sequenceStartButton.clicked()
        compare(captureWorkflowController.startSequenceCallCount, 1)
        compare(shell.form.sequencePresentation, "running")
        shell.form.capturePauseButton.clicked()
        compare(captureWorkflowController.pauseSequenceCallCount, 1)
        compare(shell.form.sequencePresentation, "paused")
        shell.form.capturePauseButton.clicked()
        compare(shell.form.sequencePresentation, "running")
        shell.form.captureStopButton.clicked()
        compare(captureWorkflowController.stopSequenceCallCount, 1)
        compare(shell.form.sequencePresentation, "completed")

        captureWorkflowController.newSequence()
        shell.mockState.imageSequenceOpen = false
        shell.mockState.datasetOpen = true
        shell.form.datasetStartButton.clicked()
        compare(captureWorkflowController.startDatasetCallCount, 1)
        compare(shell.form.datasetPresentation, "running")
        shell.form.datasetPauseButton.clicked()
        compare(captureWorkflowController.pauseDatasetCallCount, 1)
        compare(shell.form.datasetPresentation, "paused")
        shell.form.datasetPauseButton.clicked()
        compare(shell.form.datasetPresentation, "running")
        shell.form.datasetStopButton.clicked()
        compare(captureWorkflowController.stopDatasetCallCount, 1)
        compare(shell.form.datasetPresentation, "completed")
    }

    function test_trainingWeightsAndSequenceViewerViewActions() {
        shell.trainingController = trainingController
        compare(shell.form.trainWorkspace.weightsSelector.model.length, 2)
        shell.form.trainWorkspace.weightsSelector.currentIndex = 1
        shell.form.trainWorkspace.loadWeightsButton.clicked()
        compare(trainingController.loadWeightsCallCount, 1)
        compare(trainingController.loadedWeightIndex, 1)

        shell.mockState.sequenceViewerPresentation = "middleFrame"
        const initialScale = shell.form.sequenceViewerWorkspace.zoomScale
        shell.form.sequenceViewerWorkspace.zoomInButton.clicked()
        verify(shell.form.sequenceViewerWorkspace.zoomScale > initialScale)
        shell.form.sequenceViewerWorkspace.zoomOutButton.clicked()
        compare(shell.form.sequenceViewerWorkspace.zoomScale, initialScale)
        shell.form.sequenceViewerWorkspace.actualSizeButton.clicked()
        verify(shell.form.sequenceViewerWorkspace.actualSize)
        shell.form.sequenceViewerWorkspace.fitButton.clicked()
        verify(!shell.form.sequenceViewerWorkspace.actualSize)
        compare(shell.form.sequenceViewerWorkspace.zoomScale, 1)
    }
    }
}
