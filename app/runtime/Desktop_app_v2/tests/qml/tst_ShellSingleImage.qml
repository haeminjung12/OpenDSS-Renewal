import QtQuick
import QtTest
import Desktop_app_v2
import Desktop_app_v2Content

Item {
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

    QtObject {
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
        property var filteredRecords: [
            { recordId: "r1", cropUrl: "", state: "class0" },
            { recordId: "r2", cropUrl: "", state: "unreviewed" }
        ]
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
        property int refreshCallCount: 0
        property int selectCallCount: 0
        property int setActiveCallCount: 0
        property int renameCallCount: 0
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
            selectCallCount = 0
            setActiveCallCount = 0
            renameCallCount = 0
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
        property int startCallCount: 0
        property int stopCallCount: 0
        property int retrySaveCallCount: 0

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

    ShellSingleImage {
        id: shell
        anchors.fill: parent
        settingsController: textSizeController
        modelLibraryController: modelLibraryController
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
        shell.singleImageCaptureController = null
        shell.settingsActionError = ""
        modelLibraryController.reset()
        modelTestController.reset()
        singleImageCaptureController.reset()
        shell.mockState.cameraAvailable = true
        shell.mockState.cameraStreaming = true
        shell.mockState.selectedWorkspace = "capture"
        shell.mockState.daqAvailable = true
        textSizeController.textSizePercent = 100
        textSizeController.lastRequestedTextSizePercent = -1
        textSizeController.storageRoot = "file:///C:/OpenDSS/Settings%20Root"
        textSizeController.openStorageRootCallCount = 0
        textSizeController.openStorageRootError = ""
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

    function labelCropDelegate(recordId) {
        const children = shell.form.labelWorkspace.cropGridHost.children
        for (let index = 0; index < children.length; ++index) {
            const child = children[index]
            if (child.modelData !== undefined && child.modelData.recordId === recordId)
                return child
        }
        return null
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

    function test_modelLibraryControllerWiring() {
        compare(modelLibraryController.refreshCallCount, 1)
        shell.modelLibraryController = modelLibraryController
        shell.form.navLibraryButton.clicked()

        tryCompare(shell.form.modelLibraryWorkspace.modelListView, "count", 2)
        compare(shell.form.modelLibraryWorkspace.presentation, "ready")
        compare(shell.form.modelLibraryWorkspace.modelRows[1].name, "Candidate Model")
        verify(!shell.form.modelLibraryWorkspace.hasSelection)
        verify(!shell.form.modelLibraryWorkspace.importButton.enabled)
        verify(!shell.form.modelLibraryWorkspace.exportButton.enabled)
        verify(!shell.form.modelLibraryWorkspace.duplicateButton.enabled)
        verify(!shell.form.modelLibraryWorkspace.deleteButton.enabled)

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
        compare(shell.form.labelWorkspace.filteredCropRecords.length, 2)
        compare(shell.form.labelWorkspace.selectedCropId, "r1")
        compare(shell.form.labelWorkspace.selectedCropIndex, 0)
        compare(shell.form.labelWorkspace.selectedCropSource.toString(),
                labelController.selectedCropUrl.toString())
        verify(shell.form.labelWorkspace.canUndo)

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

        tryVerify(function() { return labelCropDelegate("r1") !== null })
        const crop = labelCropDelegate("r1")
        const pointerArea = crop.childAt(crop.width / 2, crop.height / 2)
        verify(pointerArea)
        pointerArea.clicked(null)
        compare(labelController.selectedRecordArgument, "r1")
        compare(labelController.selectCallCount, 1)
        crop.forceActiveFocus()
        keyClick(Qt.Key_Return)
        compare(labelController.selectCallCount, 2)
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
        shell.form.sequenceViewerWorkspace.nextButton.clicked()
        compare(shell.mockState.sequenceViewerPresentation, "middleFrame")
        compare(shell.form.sequenceViewerWorkspace.currentFrame, 60)
        shell.form.sequenceViewerWorkspace.directSeekField.text = "42"
        shell.form.sequenceViewerWorkspace.directSeekField.accepted()
        compare(shell.mockState.sequenceViewerPresentation, "middleFrame")
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
        verify(!shell.form.trainWorkspace.weightsSelector.enabled)
        verify(shell.form.trainWorkspace.weightsSelector.visible)
        verify(!shell.form.trainWorkspace.loadWeightsButton.enabled)
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
}
