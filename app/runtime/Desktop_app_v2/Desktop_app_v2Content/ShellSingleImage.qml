import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Desktop_app_v2

Item {
    id: root

    anchors.fill: parent
    property alias mockState: state
    property alias form: screen
    property var settingsController
    property var daqController
    property var runsResultsController
    property var sequenceViewerController
    property var datasetLabelController
    property var trainingController
    property var modelLibraryController
    property var modelTestController
    property bool daqDraftCommitInProgress: false
    property var cameraController: typeof cameraRuntimeController !== "undefined"
                                           ? cameraRuntimeController : null
    property var singleImageCaptureController:
        typeof singleImageRuntimeController !== "undefined"
        ? singleImageRuntimeController : null
    readonly property bool singleImageCapturing:
        root.singleImageCaptureController
        ? root.singleImageCaptureController.presentation === "capturing"
        : state.capturing
    property string settingsActionError: ""
    property alias modelRenameDialog: modelRenameDialog
    property alias modelRenameField: modelRenameField
    property alias modelImportFolderDialog: modelImportFolderDialog
    property alias modelExportFolderDialog: modelExportFolderDialog
    property alias modelDuplicateDialog: modelDuplicateDialog
    property alias modelDuplicateNameField: modelDuplicateNameField
    property alias modelDuplicateFolderDialog: modelDuplicateFolderDialog
    property alias modelDeleteDialog: modelDeleteDialog
    signal closeRequested()

    function discoveredDaqDeviceText() {
        if (!root.daqController || !root.daqController.devices)
            return ""

        const names = []
        for (let index = 0; index < root.daqController.devices.length; ++index) {
            const device = root.daqController.devices[index]
            if (device && device.deviceId)
                names.push(device.deviceId)
        }
        return names.join(", ")
    }

    function commitDaqDraft(spinBox, propertyName, scale) {
        if (!root.daqController || root.daqDraftCommitInProgress
                || !root.daqController.canApply)
            return

        const value = Number(spinBox.value)
        if (!Number.isFinite(value) || value < spinBox.from || value > spinBox.to)
            return

        root.daqDraftCommitInProgress = true
        root.daqController[propertyName] = value * scale
        root.daqController.apply()
        root.daqDraftCommitInProgress = false
    }

    function commitDaqChannel(index) {
        if (!root.daqController || root.daqDraftCommitInProgress
                || !root.daqController.canApply
                || index < 0 || index >= root.daqController.outputChannels.length)
            return

        root.daqDraftCommitInProgress = true
        root.daqController.selectedOutputChannel = root.daqController.outputChannels[index]
        root.daqController.apply()
        root.daqDraftCommitInProgress = false
    }

    function focusCameraPrompt() {
        if (screen.cameraPromptVisible)
            screen.cameraPromptYesButton.forceActiveFocus()
    }

    function localFilePath(fileUrl) {
        try {
            const parsed = new URL(fileUrl)
            if (parsed.protocol !== "file:")
                return ""

            const path = decodeURIComponent(parsed.pathname)
            if (parsed.hostname !== "")
                return "//" + decodeURIComponent(parsed.hostname) + path
            if (/^\/[A-Za-z]:\//.test(path))
                return path.substring(1)
            return path
        } catch (error) {
            return ""
        }
    }

    function importModelPackage(packageUrl) {
        return root.modelLibraryController
                ? root.modelLibraryController.importModel(packageUrl) : false
    }

    function exportSelectedModel(destinationUrl) {
        return root.modelLibraryController
                ? root.modelLibraryController.exportSelected(destinationUrl) : false
    }

    function duplicateSelectedModel(name, destinationUrl) {
        return root.modelLibraryController
                ? root.modelLibraryController.duplicateSelected(name, destinationUrl) : false
    }

    function modelTestPresentation() {
        if (!root.modelTestController)
            return state.modelTestPresentation
        if (root.modelTestController.presentation === "empty")
            return "empty"
        if (root.modelTestController.presentation === "ready")
            return "readyCpu"
        if (root.modelTestController.presentation === "starting"
                || root.modelTestController.presentation === "running"
                || root.modelTestController.presentation === "stopping")
            return "running"
        if (root.modelTestController.presentation === "completed")
            return root.modelTestController.resultSummary.perClass
                    && root.modelTestController.resultSummary.perClass.length === 3
                    ? "completedThreeClass" : "completedTwoClass"
        return root.modelTestController.presentation
    }

    function modelTestAccuracyText(value) {
        return typeof value === "number"
                ? qsTr("%1%").arg((value * 100).toFixed(1)) : qsTr("—")
    }

    function modelTestPerClassText() {
        if (!root.modelTestController)
            return ""
        const metrics = root.modelTestController.resultSummary.perClass || []
        const lines = []
        for (let index = 0; index < metrics.length; ++index) {
            lines.push(qsTr("Class %1: %2").arg(metrics[index].classId)
                       .arg(root.modelTestAccuracyText(metrics[index].accuracy)))
        }
        return lines.join("\n")
    }

    function modelTestConfusionText() {
        if (!root.modelTestController)
            return ""
        const matrix = root.modelTestController.resultSummary.confusionMatrix || []
        const lines = []
        for (let index = 0; index < matrix.length; ++index)
            lines.push(matrix[index].join("  "))
        return lines.join("\n")
    }

    Component.onCompleted: {
        if (root.cameraController
                && root.cameraController.cameraStatus !== "Unavailable")
            state.cameraPromptHandled = true
        focusCameraPrompt()
        if (root.runsResultsController)
            root.runsResultsController.refresh()
        if (root.modelLibraryController)
            root.modelLibraryController.refresh()
    }

    MockAppState {
        id: state
    }

    Screen01 {
        id: screen
        anchors.fill: parent
        cameraStatus: root.cameraController ? root.cameraController.cameraStatus
                                            : state.cameraStatus
        cameraPreviewSource: root.cameraController ? root.cameraController.previewSource : ""
        daqStatus: root.daqController ? root.daqController.daqStatus : state.projectedDaqStatus
        activeModelText: root.modelTestController
                         ? root.modelTestController.activeModelName
                           || root.modelTestController.activeModelId
                           || qsTr("No Active Model")
                         : state.activeModelText
        activityText: root.singleImageCaptureController
                      && root.singleImageCaptureController.presentation === "capturing"
                      ? qsTr("Capturing Image") : state.activityText
        fileNameText: root.singleImageCaptureController
                      ? root.singleImageCaptureController.fileName : state.fileNameDraft
        saveLocationText: root.singleImageCaptureController
                          ? root.localFilePath(
                                root.singleImageCaptureController.outputFolder.toString())
                          : state.saveLocationDraft
        disabledReason: root.singleImageCaptureController
                        ? root.singleImageCaptureController.disabledReason
                        : state.disabledReason
        savedPath: root.singleImageCaptureController
                   ? root.localFilePath(
                         root.singleImageCaptureController.savedArtifactUrl.toString())
                   : state.savedPath
        bannerHeading: state.bannerHeading
        bannerText: root.singleImageCaptureController
                    ? root.singleImageCaptureController.error : state.bannerText
        captureEnabled: root.singleImageCaptureController
                        ? root.singleImageCaptureController.canCapture
                        : state.captureEnabled
        showSavedPath: root.singleImageCaptureController
                       ? root.singleImageCaptureController.savedArtifactUrl.toString() !== ""
                       : state.showSavedPath
        showBanner: root.singleImageCaptureController
                    ? root.singleImageCaptureController.error !== "" : state.showBanner
        drawerOpen: state.hardwareDrawerOpen
        capturePanelExpanded: state.capturePanelExpanded
        selectedWorkspace: state.selectedWorkspace
        singleImagePresentation: root.singleImageCaptureController
                                 ? root.singleImageCaptureController.presentation
                                 : state.singleImagePresentation
        singleImageOpen: root.singleImageCapturing || state.singleImageOpen
        imageSequenceOpen: state.imageSequenceOpen
        datasetOpen: state.datasetOpen
        otherCaptureHeadingsDisabled:
            state.otherCaptureHeadingsDisabled
            || root.singleImageCapturing
        cameraPromptVisible: root.cameraController
                             ? !root.cameraController.busy
                               && root.cameraController.cameraStatus === "Unavailable"
                               && !state.cameraPromptHandled
                             : state.cameraPromptVisible
        cameraPromptChoice: state.cameraPromptChoice
        sequencePresentation: state.capturePresentation === "sequence" ? state.capturePhase : "ready"
        datasetPresentation: state.capturePresentation === "dataset" ? state.capturePhase : "ready"
        sequenceFrameCount: state.sequenceFrameCount
        datasetFrameCount: state.datasetFrameCount
        datasetCropCount: state.datasetCropCount
        cameraLocked: state.cameraLocked
                      || (root.cameraController && root.cameraController.busy)
                      || root.singleImageCapturing
        cameraResolution: state.cameraResolution
        cameraCustomWidth: state.cameraCustomWidth
        cameraCustomHeight: state.cameraCustomHeight
        cameraBitDepth: state.cameraBitDepth
        cameraExposure: state.cameraExposure
        cameraReadoutMode: state.cameraReadoutMode
        cameraLut: state.cameraLut
        daqDevice: root.daqController ? root.discoveredDaqDeviceText() : state.daqDevice
        daqOutputChannel: root.daqController
                          ? root.daqController.selectedOutputChannel : state.daqOutputChannel
        sequenceLocationText: state.sequenceLocationDraft
        datasetLocationText: state.datasetLocationDraft
        datasetHandoffText: state.datasetHandoffText
        hardwareActionEnabled: !state.liveOwnsOperation
        captureStartsAvailable: state.activeOperation === ""
    }

    Binding {
        target: screen.daqStatusText
        property: "text"
        value: !root.daqController || root.daqController.error === ""
               ? root.daqController ? qsTr("Status: %1").arg(root.daqController.daqStatus) : ""
               : qsTr("Status: %1 — %2").arg(root.daqController.daqStatus)
                                      .arg(root.daqController.error)
        when: root.daqController !== null
    }
    Binding { target: screen.daqChannelSelector; property: "model"; value: root.daqController ? root.daqController.outputChannels : []; when: root.daqController !== null }
    Binding { target: screen.daqChannelSelector; property: "currentIndex"; value: root.daqController ? root.daqController.outputChannels.indexOf(root.daqController.selectedOutputChannel) : -1; when: root.daqController !== null }
    Binding { target: screen.daqVppSpinBox; property: "value"; value: root.daqController ? root.daqController.amplitudeVpp : 0; when: root.daqController !== null }
    Binding { target: screen.daqFrequencySpinBox; property: "value"; value: root.daqController ? root.daqController.frequencyHz / 1000 : 0; when: root.daqController !== null }
    Binding { target: screen.daqEventDurationSpinBox; property: "value"; value: root.daqController ? root.daqController.durationMs : 0; when: root.daqController !== null }
    Binding { target: screen.daqDecisionDelaySpinBox; property: "value"; value: root.daqController ? root.daqController.delayMs : 0; when: root.daqController !== null }
    Binding { target: screen.daqRefreshDevicesButton; property: "enabled"; value: root.daqController !== null; when: root.daqController !== null }
    Binding { target: screen.daqChannelSelector; property: "enabled"; value: root.daqController ? root.daqController.canApply : false; when: root.daqController !== null }
    Binding { target: screen.daqVppSpinBox; property: "enabled"; value: root.daqController ? root.daqController.canApply : false; when: root.daqController !== null }
    Binding { target: screen.daqFrequencySpinBox; property: "enabled"; value: root.daqController ? root.daqController.canApply : false; when: root.daqController !== null }
    Binding { target: screen.daqEventDurationSpinBox; property: "enabled"; value: root.daqController ? root.daqController.canApply : false; when: root.daqController !== null }
    Binding { target: screen.daqDecisionDelaySpinBox; property: "enabled"; value: root.daqController ? root.daqController.canApply : false; when: root.daqController !== null }

    Binding { target: screen.labelWorkspace; property: "presentation"; value: root.datasetLabelController ? root.datasetLabelController.presentation : state.labelPresentation }
    Binding { target: screen.labelWorkspace; property: "currentFilter"; value: root.datasetLabelController ? root.datasetLabelController.filter : state.selectedLabelFilter }
    Binding { target: screen.labelWorkspace; property: "classCount"; value: root.datasetLabelController ? root.datasetLabelController.classCount : state.labelClassCount }
    Binding { target: screen.labelWorkspace; property: "datasetName"; value: root.datasetLabelController ? root.datasetLabelController.datasetId : state.labelDatasetName }
    Binding { target: screen.labelWorkspace; property: "totalCount"; value: root.datasetLabelController ? root.datasetLabelController.totalCount : state.labelTotalCount }
    Binding { target: screen.labelWorkspace; property: "labeledCount"; value: root.datasetLabelController ? root.datasetLabelController.labeledCount : state.labelLabeledCount }
    Binding { target: screen.labelWorkspace; property: "class0Count"; value: root.datasetLabelController ? root.datasetLabelController.class0Count : 0; when: !!root.datasetLabelController }
    Binding { target: screen.labelWorkspace; property: "class1Count"; value: root.datasetLabelController ? root.datasetLabelController.class1Count : 0; when: !!root.datasetLabelController }
    Binding { target: screen.labelWorkspace; property: "class2Count"; value: root.datasetLabelController ? root.datasetLabelController.class2Count : 0; when: !!root.datasetLabelController }
    Binding { target: screen.labelWorkspace; property: "excludedCount"; value: root.datasetLabelController ? root.datasetLabelController.excludedCount : 0; when: !!root.datasetLabelController }
    Binding { target: screen.labelWorkspace; property: "unreviewedCount"; value: root.datasetLabelController ? root.datasetLabelController.unreviewedCount : 0; when: !!root.datasetLabelController }
    Binding { target: screen.labelWorkspace; property: "classNames"; value: root.datasetLabelController ? root.datasetLabelController.classNames : []; when: !!root.datasetLabelController }
    Binding { target: screen.labelWorkspace; property: "filteredCropRecords"; value: root.datasetLabelController ? root.datasetLabelController.filteredRecords : []; when: !!root.datasetLabelController }
    Binding { target: screen.labelWorkspace; property: "selectedCropId"; value: root.datasetLabelController ? root.datasetLabelController.selectedRecordId : ""; when: !!root.datasetLabelController }
    Binding { target: screen.labelWorkspace; property: "selectedCropIndex"; value: root.datasetLabelController ? root.datasetLabelController.selectedIndex : -1; when: !!root.datasetLabelController }
    Binding { target: screen.labelWorkspace; property: "selectedCropSource"; value: root.datasetLabelController ? root.datasetLabelController.selectedCropUrl : ""; when: !!root.datasetLabelController }
    Binding { target: screen.labelWorkspace; property: "canUndo"; value: root.datasetLabelController ? root.datasetLabelController.canUndo : false; when: !!root.datasetLabelController }
    Binding { target: screen.labelWorkspace; property: "errorMessage"; value: root.datasetLabelController ? root.datasetLabelController.errorMessage : ""; when: !!root.datasetLabelController }
    Binding { target: screen.labelWorkspace; property: "rightPanelExpanded"; value: state.labelRightPanelExpanded }
    Binding { target: screen.labelWorkspace; property: "datasetSummaryExpanded"; value: state.labelDatasetSummaryExpanded }
    Binding { target: screen.labelWorkspace; property: "labelExpanded"; value: state.labelExpanded }
    Binding { target: screen.labelWorkspace; property: "filterExpanded"; value: state.labelFilterExpanded }

    Component {
        id: labelCropDelegate

        Rectangle {
            id: cropDelegate
            required property var modelData
            readonly property bool selected: modelData.recordId === screen.labelWorkspace.selectedCropId
            width: 106
            height: 82
            color: modelData.state === "excluded" ? "#d1d5db" : Constants.backgroundColor
            border.width: activeFocus || selected ? 4 : 2
            border.color: activeFocus || selected ? Constants.accentColor
                                                  : modelData.state === "class0" ? Constants.appClass0Color
                                                  : modelData.state === "class1" ? Constants.appClass1Color
                                                  : modelData.state === "class2" ? Constants.appClass2Color
                                                  : Constants.borderColor
            activeFocusOnTab: !!root.datasetLabelController
            Accessible.name: qsTr("Droplet Crop %1").arg(modelData.recordId)
            Accessible.role: Accessible.Button

            Image {
                anchors.fill: parent
                anchors.margins: 5
                source: cropDelegate.modelData.cropUrl
                fillMode: Image.PreserveAspectFit
                sourceSize: Qt.size(Math.round(width), Math.round(height))
                asynchronous: true
                cache: false
            }

            Text {
                visible: cropDelegate.modelData.state === "excluded"
                anchors.centerIn: parent
                text: "×"
                color: Constants.textColor
                font: Constants.largeFont
            }

            MouseArea {
                anchors.fill: parent
                enabled: !!root.datasetLabelController
                onClicked: {
                    cropDelegate.forceActiveFocus()
                    root.datasetLabelController.select(cropDelegate.modelData.recordId)
                }
            }
            Keys.onReturnPressed: root.datasetLabelController.select(modelData.recordId)
            Keys.onSpacePressed: root.datasetLabelController.select(modelData.recordId)
            Keys.enabled: !!root.datasetLabelController
        }
    }

    Repeater {
        parent: screen.labelWorkspace.cropGridHost
        model: root.datasetLabelController ? root.datasetLabelController.filteredRecords : []
        delegate: labelCropDelegate
    }

    Binding { target: screen.sequenceViewerWorkspace; property: "presentation"; value: root.sequenceViewerController ? root.sequenceViewerController.presentation : state.sequenceViewerPresentation === "empty" || state.sequenceViewerPresentation === "error" ? state.sequenceViewerPresentation : "ready" }
    Binding { target: screen.sequenceViewerWorkspace; property: "currentFrame"; value: root.sequenceViewerController ? root.sequenceViewerController.currentFrame : state.sequenceViewerPresentation === "firstFrame" ? 1 : state.sequenceViewerPresentation === "middleFrame" ? 60 : state.sequenceViewerPresentation === "finalFrame" ? 120 : 0 }
    Binding { target: screen.sequenceViewerWorkspace; property: "totalFrames"; value: root.sequenceViewerController ? root.sequenceViewerController.totalFrames : state.sequenceViewerPresentation === "empty" || state.sequenceViewerPresentation === "error" ? 0 : 120 }
    Binding { target: screen.sequenceViewerWorkspace; property: "currentFrameSource"; value: root.sequenceViewerController ? root.sequenceViewerController.currentFrameImageUrl : "" }

    Binding { target: screen.trainWorkspace; property: "presentation"; value: root.trainingController ? root.trainingController.presentation === "ready" ? root.trainingController.requestedDevice === "cpu" ? "readyCpu" : "readyGpu" : root.trainingController.presentation === "failed" || root.trainingController.presentation === "saveFailed" ? "error" : root.trainingController.presentation === "saving" ? "running" : root.trainingController.presentation : state.trainPresentation }
    Binding { target: screen.trainWorkspace; property: "datasetText"; value: root.trainingController ? root.trainingController.datasetManifestUrl.toString() === "" ? qsTr("No Dataset selected") : root.localFilePath(root.trainingController.datasetManifestUrl) : state.trainPresentation === "empty" ? qsTr("No Dataset selected") : qsTr("Dataset-042") }
    Binding { target: screen.trainWorkspace; property: "deviceText"; value: root.trainingController ? root.trainingController.requestedDevice === "cpu" ? qsTr("CPU") : qsTr("GPU") : state.trainPresentation === "readyCpu" ? qsTr("CPU (automatic)") : qsTr("GPU (automatic)") }
    Binding { target: screen.trainWorkspace; property: "disabledReason"; value: root.trainingController ? root.trainingController.errorMessage : state.activeOperation !== "" ? qsTr("Another operation is active") : state.trainPresentation === "empty" ? qsTr("No dataset selected") : state.trainPresentation === "unavailable" ? qsTr("No Labeled Droplet Crops") : state.trainModelNameDraft.trim() === "" ? qsTr("Model name required") : "" }
    Binding { target: screen.trainWorkspace; property: "modelNameText"; value: root.trainingController ? root.trainingController.modelName : state.trainModelNameDraft }
    Binding { target: screen.trainWorkspace; property: "saveLocationText"; value: root.trainingController ? root.localFilePath(root.trainingController.outputDirectoryUrl) : state.trainSaveLocationDraft }
    Binding { target: screen.trainWorkspace; property: "resultPath"; value: root.trainingController ? root.localFilePath(root.trainingController.registeredPackageUrl.toString() !== "" ? root.trainingController.registeredPackageUrl : root.trainingController.resultDirectoryUrl) : qsTr("C:/OpenDSS/Models/DropletNet-04.opendssmodel") }
    Binding { target: screen.trainWorkspace; property: "modelOnnxPath"; value: root.trainingController ? root.localFilePath(root.trainingController.modelOnnxUrl) : "" }
    Binding { target: screen.trainWorkspace; property: "metadataPath"; value: root.trainingController ? root.localFilePath(root.trainingController.metadataUrl) : "" }
    Binding { target: screen.trainWorkspace; property: "startEnabled"; value: root.trainingController ? root.trainingController.presentation === "ready" : (state.trainPresentation === "readyCpu" || state.trainPresentation === "readyGpu") && state.trainModelNameDraft.trim() !== "" && state.activeOperation === "" }
    Binding { target: screen.trainWorkspace; property: "showRunning"; value: root.trainingController ? root.trainingController.presentation === "running" || root.trainingController.presentation === "saving" : state.trainPresentation === "running" }
    Binding { target: screen.trainWorkspace; property: "showCompleted"; value: root.trainingController ? root.trainingController.presentation === "completed" : state.trainPresentation === "completed" }
    Binding { target: screen.trainWorkspace; property: "showError"; value: root.trainingController ? root.trainingController.presentation === "failed" || root.trainingController.presentation === "saveFailed" : state.trainPresentation === "error" }
    Binding { target: screen.trainWorkspace; property: "showInterrupted"; value: root.trainingController ? root.trainingController.presentation === "interrupted" : state.trainPresentation === "interrupted" }
    Binding { target: screen.trainWorkspace; property: "requestedDeviceText"; value: root.trainingController ? root.trainingController.requestedDevice === "cpu" ? qsTr("CPU") : qsTr("GPU") : "" }
    Binding { target: screen.trainWorkspace; property: "effectiveDeviceText"; value: ""; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace; property: "stageText"; value: root.trainingController ? root.trainingController.stage : "" }
    Binding { target: screen.trainWorkspace; property: "currentEpoch"; value: root.trainingController ? root.trainingController.epoch : 0 }
    Binding { target: screen.trainWorkspace; property: "totalEpochs"; value: root.trainingController ? root.trainingController.stageEpochs : 0 }
    Binding { target: screen.trainWorkspace; property: "overallProgress"; value: root.trainingController && root.trainingController.stageEpochs > 0 ? root.trainingController.epoch / root.trainingController.stageEpochs : 0 }
    Binding { target: screen.trainWorkspace; property: "elapsedText"; value: ""; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace; property: "remainingText"; value: ""; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace; property: "errorText"; value: root.trainingController ? root.trainingController.errorMessage : qsTr("Error") }
    Binding { target: screen.trainWorkspace; property: "lossSeries"; value: []; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace; property: "accuracySeries"; value: []; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace; property: "resultMetrics"; value: []; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace; property: "showMetrics"; value: false; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace; property: "showTiming"; value: false; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace; property: "showActiveModelConfirmation"; value: root.trainingController && root.trainingController.presentation === "completed"; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace; property: "showRetrySave"; value: root.trainingController && root.trainingController.retrySaveAvailable; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace; property: "serviceFactsOnly"; value: true; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace.architectureSelector; property: "currentIndex"; value: root.trainingController && root.trainingController.architecture === "efficientnet" ? 1 : 0; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace.trainingDeviceSelector; property: "currentIndex"; value: root.trainingController && root.trainingController.requestedDevice === "cpu" ? 1 : 0; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace.selectDatasetButton; property: "enabled"; value: root.trainingController && root.trainingController.presentation !== "running"; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace.architectureSelector; property: "enabled"; value: root.trainingController && root.trainingController.presentation !== "running"; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace.trainingDeviceSelector; property: "enabled"; value: root.trainingController && root.trainingController.presentation !== "running"; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace.modelNameField; property: "enabled"; value: root.trainingController && root.trainingController.presentation !== "running"; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace.saveLocationField; property: "enabled"; value: root.trainingController && root.trainingController.presentation !== "running"; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace.saveLocationField; property: "readOnly"; value: true; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace.browseButton; property: "enabled"; value: root.trainingController && root.trainingController.presentation !== "running"; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace.stopButton; property: "enabled"; value: root.trainingController && root.trainingController.presentation === "running"; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace.weightsSelector; property: "enabled"; value: false; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace.loadWeightsButton; property: "enabled"; value: false; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace.retrySaveButton; property: "enabled"; value: root.trainingController && root.trainingController.retrySaveAvailable; when: !!root.trainingController }
    Binding { target: screen.trainWorkspace.openInModelTestButton; property: "enabled"; value: root.trainingController && root.trainingController.presentation === "completed"; when: !!root.trainingController }
    Binding { target: screen.fileNameField; property: "enabled"; value: !root.singleImageCapturing }
    Binding { target: screen.saveLocationField; property: "enabled"; value: !root.singleImageCapturing }
    Binding { target: screen.browseButton; property: "enabled"; value: !root.singleImageCapturing }
    Binding { target: screen.trainWorkspace; property: "trainingSetupExpanded"; value: state.trainingSetupExpanded }
    Binding { target: screen.trainWorkspace; property: "trainingStatusExpanded"; value: state.trainingStatusExpanded }
    Binding { target: screen.trainWorkspace; property: "operationPanelExpanded"; value: state.trainOperationPanelExpanded }

    Binding { target: screen.modelLibraryWorkspace; property: "presentation"; value: root.modelLibraryController ? root.modelLibraryController.presentation : state.modelLibraryPresentation }
    Binding { target: screen.modelLibraryWorkspace; property: "hasSelection"; value: root.modelLibraryController ? root.modelLibraryController.selectedIndex >= 0 : state.modelLibraryPresentation !== "empty" && state.modelLibraryPresentation !== "error" }
    Binding { target: screen.modelLibraryWorkspace; property: "selectedActive"; value: root.modelLibraryController ? root.modelLibraryController.selectedId !== "" && root.modelLibraryController.selectedId === root.modelLibraryController.activeId : state.modelLibraryPresentation === "readyActive" || state.modelLibraryPresentation === "locked" }
    Binding { target: screen.modelLibraryWorkspace; property: "modelLocked"; value: (root.modelTestController ? root.modelTestController.presentation === "starting" || root.modelTestController.presentation === "running" || root.modelTestController.presentation === "stopping" : state.modelLibraryPresentation === "locked") || (root.modelLibraryController ? root.modelLibraryController.operationInProgress || (root.modelLibraryController.selectedId !== "" && !root.modelLibraryController.canDelete) : false) }
    Binding { target: screen.modelLibraryWorkspace; property: "showError"; value: root.modelLibraryController ? root.modelLibraryController.errorMessage !== "" : state.modelLibraryPresentation === "error" }
    Binding { target: screen.modelLibraryWorkspace; property: "modelRows"; value: root.modelLibraryController ? root.modelLibraryController.modelRows : [] }
    Binding { target: screen.modelLibraryWorkspace; property: "selectedModelIndex"; value: root.modelLibraryController ? root.modelLibraryController.selectedIndex : -1 }
    Binding { target: screen.modelLibraryWorkspace; property: "selectedModelName"; value: root.modelLibraryController ? root.modelLibraryController.selectedDetail.name || "" : "" }
    Binding { target: screen.modelLibraryWorkspace; property: "selectedModelArchitecture"; value: root.modelLibraryController ? root.modelLibraryController.selectedDetail.architecture || "" : "" }
    Binding { target: screen.modelLibraryWorkspace; property: "selectedModelPerformanceLabel"; value: root.modelLibraryController ? root.modelLibraryController.selectedDetail.performanceLabel || "" : "" }
    Binding { target: screen.modelLibraryWorkspace; property: "selectedModelClassSummary"; value: root.modelLibraryController ? root.modelLibraryController.selectedDetail.classSummary || "" : "" }
    Binding { target: screen.modelLibraryWorkspace; property: "selectedModelSourceDataset"; value: root.modelLibraryController ? root.modelLibraryController.selectedDetail.sourceDataset || "" : "" }
    Binding { target: screen.modelLibraryWorkspace; property: "selectedModelCreationDate"; value: root.modelLibraryController ? root.modelLibraryController.selectedDetail.createdAt || "" : "" }
    Binding { target: screen.modelLibraryWorkspace; property: "selectedModelPackageLocation"; value: root.modelLibraryController ? root.modelLibraryController.selectedDetail.packageLocation || "" : "" }
    Binding { target: screen.modelLibraryWorkspace; property: "selectedModelTrainingMetrics"; value: root.modelLibraryController ? root.modelLibraryController.selectedDetail.trainingMetrics || "" : "" }
    Binding { target: screen.modelLibraryWorkspace.importButton; property: "enabled"; value: root.modelLibraryController && root.modelLibraryController.canImport; when: !!root.modelLibraryController }
    Binding { target: screen.modelLibraryWorkspace.exportButton; property: "enabled"; value: root.modelLibraryController && root.modelLibraryController.canExport; when: !!root.modelLibraryController }
    Binding { target: screen.modelLibraryWorkspace.duplicateButton; property: "enabled"; value: root.modelLibraryController && root.modelLibraryController.canDuplicate; when: !!root.modelLibraryController }
    Binding { target: screen.modelLibraryWorkspace.deleteButton; property: "enabled"; value: root.modelLibraryController && root.modelLibraryController.canDelete; when: !!root.modelLibraryController }
    Binding { target: screen.modelLibraryWorkspace.openInModelTestButton; property: "enabled"; value: root.modelLibraryController && root.modelLibraryController.selectedId !== "" && root.modelLibraryController.selectedId === root.modelLibraryController.activeId; when: !!root.modelLibraryController }
    Binding { target: screen.modelLibraryWorkspace; property: "selectedModelExpanded"; value: state.selectedModelExpanded }
    Binding { target: screen.modelLibraryWorkspace; property: "rightPanelExpanded"; value: state.modelLibraryRightPanelExpanded }

    Dialog {
        id: modelRenameDialog
        anchors.centerIn: parent
        modal: true
        title: qsTr("Rename Model")
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: {
            modelRenameField.text = root.modelLibraryController
                    ? root.modelLibraryController.selectedDetail.name || "" : ""
            modelRenameField.selectAll()
            modelRenameField.forceActiveFocus()
        }
        onAccepted: {
            if (root.modelLibraryController)
                root.modelLibraryController.renameSelected(modelRenameField.text)
        }

        TextField {
            id: modelRenameField
            width: Math.round(320 * Constants.textScale)
            placeholderText: qsTr("Model Name")
        }
    }

    FolderDialog {
        id: modelImportFolderDialog
        title: qsTr("Import OpenDSS v2 Model Package")
        onAccepted: {
            if (selectedFolder.toString() !== "")
                root.importModelPackage(selectedFolder)
        }
    }

    FolderDialog {
        id: modelExportFolderDialog
        title: qsTr("Choose Model Export Destination")
        onAccepted: {
            if (selectedFolder.toString() !== "")
                root.exportSelectedModel(selectedFolder)
        }
    }

    Dialog {
        id: modelDuplicateDialog
        anchors.centerIn: parent
        modal: true
        title: qsTr("Duplicate Model")
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: {
            modelDuplicateNameField.text = root.modelLibraryController
                    ? qsTr("%1 Copy").arg(
                          root.modelLibraryController.selectedDetail.name || "") : ""
            modelDuplicateNameField.selectAll()
            modelDuplicateNameField.forceActiveFocus()
        }
        onAccepted: modelDuplicateFolderDialog.open()

        TextField {
            id: modelDuplicateNameField
            width: Math.round(320 * Constants.textScale)
            placeholderText: qsTr("Model Name")
        }
    }

    FolderDialog {
        id: modelDuplicateFolderDialog
        title: qsTr("Choose Duplicate Model Location")
        onAccepted: {
            if (selectedFolder.toString() !== "")
                root.duplicateSelectedModel(modelDuplicateNameField.text,
                                            selectedFolder)
        }
    }

    Dialog {
        id: modelDeleteDialog
        anchors.centerIn: parent
        modal: true
        title: qsTr("Delete Model")
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: {
            if (root.modelLibraryController)
                root.modelLibraryController.deleteSelected()
        }

        Label {
            text: qsTr("Delete Model Package “%1”?").arg(
                      root.modelLibraryController
                      ? root.modelLibraryController.selectedDetail.name || "" : "")
            wrapMode: Text.WordWrap
            width: Math.round(360 * Constants.textScale)
        }
    }

    Binding { target: screen.modelTestWorkspace; property: "presentation"; value: root.modelTestPresentation() }
    Binding { target: screen.modelTestWorkspace; property: "activeModelText"; value: root.modelTestController ? root.modelTestController.activeModelName || root.modelTestController.activeModelId || qsTr("No Active Model") : root.modelLibraryController ? root.modelLibraryController.selectedId !== "" && root.modelLibraryController.selectedId === root.modelLibraryController.activeId ? root.modelLibraryController.selectedDetail.name || root.modelLibraryController.activeId : root.modelLibraryController.activeId : state.activeModelText }
    Binding { target: screen.modelTestWorkspace; property: "datasetText"; value: root.modelTestController ? root.modelTestController.datasetManifestUrl.toString() === "" ? qsTr("No Dataset selected") : root.localFilePath(root.modelTestController.datasetManifestUrl) : state.modelTestDatasetSelected ? qsTr("Dataset-042") : qsTr("No Dataset selected") }
    Binding { target: screen.modelTestWorkspace; property: "deviceText"; value: root.modelTestController && root.modelTestController.resultSummary.effectiveDevice ? root.modelTestController.resultSummary.effectiveDevice : root.modelTestController ? root.modelTestController.plannedDeviceText : state.modelTestPresentation === "readyCpu" ? qsTr("CPU (automatic)") : qsTr("GPU (automatic)") }
    Binding { target: screen.modelTestWorkspace; property: "outputLocationText"; value: root.modelTestController ? root.localFilePath(root.modelTestController.outputFolderUrl) : state.modelTestOutputLocationDraft }
    Binding { target: screen.modelTestWorkspace; property: "blockerText"; value: root.modelTestController ? root.modelTestController.errorMessage : state.activeOperation !== "" ? qsTr("Another operation is active") : root.modelLibraryController ? root.modelLibraryController.activeId === "" ? qsTr("No Active Model") : !state.modelTestDatasetSelected ? qsTr("No dataset selected") : "" : state.activeModelId === "" ? qsTr("No Active Model") : !state.modelTestDatasetSelected ? qsTr("No dataset selected") : "" }
    Binding { target: screen.modelTestWorkspace; property: "startEnabled"; value: root.modelTestController ? root.modelTestController.canStart : (state.modelTestPresentation === "readyCpu" || state.modelTestPresentation === "readyGpu") && state.activeOperation === "" }
    Binding { target: screen.modelTestWorkspace; property: "showRunning"; value: root.modelTestController ? root.modelTestController.presentation === "starting" || root.modelTestController.presentation === "running" || root.modelTestController.presentation === "stopping" : state.modelTestPresentation === "running" }
    Binding { target: screen.modelTestWorkspace; property: "showCompleted"; value: root.modelTestController ? root.modelTestController.presentation === "completed" && root.modelTestController.resultSummary.status === "completed" : state.modelTestPresentation === "completedTwoClass" || state.modelTestPresentation === "completedThreeClass" }
    Binding { target: screen.modelTestWorkspace; property: "showError"; value: root.modelTestController ? root.modelTestController.presentation === "interrupted" || root.modelTestController.presentation === "error" : state.modelTestPresentation === "interrupted" || state.modelTestPresentation === "error" }
    Binding { target: screen.modelTestWorkspace; property: "threeClassResult"; value: root.modelTestController ? root.modelTestController.resultSummary.perClass && root.modelTestController.resultSummary.perClass.length === 3 : state.modelTestPresentation === "completedThreeClass" }
    Binding { target: screen.modelTestWorkspace; property: "processedCount"; value: root.modelTestController ? root.modelTestController.processedImages : 360 }
    Binding { target: screen.modelTestWorkspace; property: "eligibleCount"; value: root.modelTestController ? root.modelTestController.eligibleImages : 1200 }
    Binding { target: screen.modelTestWorkspace; property: "progressValue"; value: root.modelTestController ? root.modelTestController.progress : 0.3 }
    Binding { target: screen.modelTestWorkspace; property: "overallAccuracyText"; value: root.modelTestController ? root.modelTestAccuracyText(root.modelTestController.resultSummary.overallAccuracy) : "" }
    Binding { target: screen.modelTestWorkspace; property: "perClassAccuracyText"; value: root.modelTestController ? root.modelTestPerClassText() : "" }
    Binding { target: screen.modelTestWorkspace; property: "confusionMatrixText"; value: root.modelTestController ? root.modelTestConfusionText() : "" }
    Binding { target: screen.modelTestWorkspace; property: "predictionSummaryText"; value: root.modelTestController && root.modelTestController.resultSummary.status === "completed" ? qsTr("Processed %1 of %2; correct %3").arg(root.modelTestController.resultSummary.processedImages).arg(root.modelTestController.resultSummary.eligibleImages).arg(root.modelTestController.resultSummary.correctPredictions) : "" }
    Binding { target: screen.modelTestWorkspace; property: "fallbackWarningText"; value: root.modelTestController ? root.modelTestController.resultSummary.fallbackWarning || "" : "" }
    Binding { target: screen.modelTestWorkspace; property: "summaryPathText"; value: root.modelTestController ? root.localFilePath(root.modelTestController.summaryUrl) : "" }
    Binding { target: screen.modelTestWorkspace; property: "predictionsPathText"; value: root.modelTestController ? root.localFilePath(root.modelTestController.predictionsCsvUrl) : "" }
    Binding { target: screen.modelTestWorkspace; property: "actionErrorText"; value: root.modelTestController ? root.modelTestController.actionError : "" }
    Binding { target: screen.modelTestWorkspace; property: "serviceFactsOnly"; value: !!root.modelTestController }
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

    Binding { target: screen.runsWorkspace; property: "selectedRunId"; value: root.runsResultsController ? root.runsResultsController.selectedRunId : state.runsPresentation === "runsEmpty" || state.runsPresentation === "runsError" ? "" : "Run-042" }
    Binding { target: screen.runsWorkspace; property: "loadedRunId"; value: root.runsResultsController ? root.runsResultsController.loadedRun.id || "" : state.runsPresentation === "runsLoaded" || state.runsPresentation === "runsNotesEditing" ? "Run-042" : "" }
    Binding { target: screen.runsWorkspace; property: "runsError"; value: root.runsResultsController ? root.runsResultsController.errorMessage !== "" : state.runsPresentation === "runsError" }
    Binding { target: screen.runsWorkspace; property: "hasRuns"; value: root.runsResultsController ? root.runsResultsController.runs.length > 0 : state.runsPresentation !== "runsEmpty" && state.runsPresentation !== "runsError" }
    Binding { target: screen.runsWorkspace; property: "runsPanelExpanded"; value: state.runsPanelExpanded }
    Binding { target: screen.runsWorkspace; property: "rightPanelExpanded"; value: state.runsRightPanelExpanded }
    Binding { target: screen.runsWorkspace; property: "notesEditing"; value: state.runsPresentation === "runsNotesEditing" }
    Binding { target: screen.runsWorkspace; property: "loadedRunStatusText"; value: state.loadedRunStatusText }
    Binding { target: screen.runsWorkspace; property: "loadedRunStopReasonText"; value: state.loadedRunStopReasonText }
    Binding { target: screen.runsWorkspace; property: "run042RowStatusText"; value: state.run042RowStatusText }

    Binding { target: screen.settingsWorkspace; property: "settingsPresentation"; value: root.settingsController ? (root.settingsActionError === "" ? "ready" : "error") : (state.settingsPresentation === "settingsError" ? "error" : "ready") }
    Binding { target: screen.settingsWorkspace; property: "defaultDataRoot"; value: root.settingsController ? root.localFilePath(String(root.settingsController.storageRoot)) : ""; when: !!root.settingsController }
    Binding { target: screen.settingsWorkspace; property: "textSizePercent"; value: root.settingsController ? root.settingsController.textSizePercent : 100; when: !!root.settingsController }
    Binding { target: Constants; property: "textSizePercent"; value: root.settingsController ? root.settingsController.textSizePercent : 100; when: !!root.settingsController }

    Connections {
        target: screen
        function onCameraPromptVisibleChanged() {
            root.focusCameraPrompt()
        }
    }

    Connections {
        target: root.cameraController
        function onStateChanged() {
            if (root.cameraController.cameraStatus !== "Unavailable")
                state.cameraPromptHandled = true
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
    Connections {
        target: screen.startCameraButton
        function onClicked() {
            if (root.cameraController) {
                if (root.cameraController.streaming)
                    root.cameraController.stop()
                else
                    root.cameraController.start()
            } else {
                state.toggleCameraStreaming()
            }
        }
    }
    Connections { target: screen.restoreCameraButton; function onClicked() { if (root.cameraController) root.cameraController.recover(); else state.selectCameraDevice(true) } }
    Connections { target: screen.cameraDeviceSelector; function onActivated(index) { state.selectCameraDevice(index === 1) } }
    Connections { target: screen.cameraResolutionSelector; function onActivated(index) { state.cameraResolution = index === 1 ? qsTr("2048 × 2048") : index === 2 ? qsTr("Custom") : qsTr("1024 × 1024") } }
    Connections { target: screen.cameraCustomWidthField; function onTextEdited() { state.cameraCustomWidth = screen.cameraCustomWidthField.text } }
    Connections { target: screen.cameraCustomHeightField; function onTextEdited() { state.cameraCustomHeight = screen.cameraCustomHeightField.text } }
    Connections { target: screen.cameraExposureField; function onTextEdited() { state.cameraExposure = screen.cameraExposureField.text } }
    Connections { target: screen.cameraLutSelector; function onActivated(index) { state.cameraLut = index === 1 ? qsTr("High contrast") : qsTr("Linear") } }
    Connections {
        target: screen.daqRefreshDevicesButton
        function onClicked() {
            if (root.daqController)
                root.daqController.refreshDevices()
        }
    }
    Connections {
        target: screen.daqChannelSelector
        function onActivated(index) {
            if (root.daqController)
                root.commitDaqChannel(index)
            else
                state.daqOutputChannel = index === 1 ? qsTr("ao1") : qsTr("ao0")
        }
    }
    Connections { target: screen.daqVppSpinBox; function onValueModified() { root.commitDaqDraft(screen.daqVppSpinBox, "amplitudeVpp", 1) } }
    Connections { target: screen.daqFrequencySpinBox; function onValueModified() { root.commitDaqDraft(screen.daqFrequencySpinBox, "frequencyHz", 1000) } }
    Connections { target: screen.daqEventDurationSpinBox; function onValueModified() { root.commitDaqDraft(screen.daqEventDurationSpinBox, "durationMs", 1) } }
    Connections { target: screen.daqDecisionDelaySpinBox; function onValueModified() { root.commitDaqDraft(screen.daqDecisionDelaySpinBox, "delayMs", 1) } }
    Connections { target: screen.sequenceViewerButton; function onClicked() { state.openSequenceViewer() } }
    Connections { target: screen.sequenceTestButton; function onClicked() { state.openSequenceTest() } }
    Connections { target: screen.sequenceNewButton; function onClicked() { state.startNewSequence() } }
    Connections { target: screen.datasetLabelButton; function onClicked() { state.openLabel() } }
    Connections { target: screen.datasetFolderButton; function onClicked() { state.showMockFolder() } }
    Connections { target: screen.datasetNewButton; function onClicked() { state.startNewDataset() } }
    Connections { target: screen.capturePanelToggleButton; function onClicked() { state.toggleCapturePanel() } }

    Connections { target: screen.labelWorkspace.openDatasetButton; function onClicked() { if (root.datasetLabelController) labelDatasetFileDialog.open(); else state.openLabelDataset() } }
    Connections { target: screen.labelWorkspace.twoClassChoice; function onClicked() { if (root.datasetLabelController) root.datasetLabelController.configureClassCount(2); else state.defineLabelClasses(2) } }
    Connections { target: screen.labelWorkspace.threeClassChoice; function onClicked() { if (root.datasetLabelController) root.datasetLabelController.configureClassCount(3); else state.defineLabelClasses(3) } }
    Connections { target: screen.labelWorkspace.rightPanelToggleButton; function onClicked() { state.toggleLabelPanel() } }
    Connections { target: screen.labelWorkspace.datasetSummaryHeadingButton; function onClicked() { state.toggleLabelDatasetSummary() } }
    Connections { target: screen.labelWorkspace.labelHeadingButton; function onClicked() { state.toggleLabelSection() } }
    Connections { target: screen.labelWorkspace.filterHeadingButton; function onClicked() { state.toggleLabelFilter() } }
    Connections { target: screen.labelWorkspace.allFilterButton; function onClicked() { if (root.datasetLabelController) root.datasetLabelController.setFilter("all"); else state.selectLabelFilter("all") } }
    Connections { target: screen.labelWorkspace.class0FilterButton; function onClicked() { if (root.datasetLabelController) root.datasetLabelController.setFilter("class0"); else state.selectLabelFilter("class0") } }
    Connections { target: screen.labelWorkspace.class1FilterButton; function onClicked() { if (root.datasetLabelController) root.datasetLabelController.setFilter("class1"); else state.selectLabelFilter("class1") } }
    Connections { target: screen.labelWorkspace.class2FilterButton; function onClicked() { if (root.datasetLabelController) root.datasetLabelController.setFilter("class2"); else state.selectLabelFilter("class2") } }
    Connections { target: screen.labelWorkspace.excludedFilterButton; function onClicked() { if (root.datasetLabelController) root.datasetLabelController.setFilter("excluded"); else state.selectLabelFilter("excluded") } }
    Connections { target: screen.labelWorkspace.unreviewedFilterButton; function onClicked() { if (root.datasetLabelController) root.datasetLabelController.setFilter("unreviewed"); else state.selectLabelFilter("unreviewed") } }
    Connections { target: screen.labelWorkspace.class0Button; function onClicked() { if (root.datasetLabelController) root.datasetLabelController.assignClass("0"); else state.recordLabel() } }
    Connections { target: screen.labelWorkspace.class1Button; function onClicked() { if (root.datasetLabelController) root.datasetLabelController.assignClass("1"); else state.recordLabel() } }
    Connections { target: screen.labelWorkspace.class2Button; function onClicked() { if (root.datasetLabelController) root.datasetLabelController.assignClass("2"); else state.recordLabel() } }
    Connections { target: screen.labelWorkspace.excludeButton; function onClicked() { if (root.datasetLabelController) root.datasetLabelController.exclude(); else state.recordLabel() } }
    Connections { target: screen.labelWorkspace.undoButton; function onClicked() { if (root.datasetLabelController) root.datasetLabelController.undo(); else state.undoLabel() } }
    Connections { target: screen.labelWorkspace.previousButton; function onClicked() { if (root.datasetLabelController) root.datasetLabelController.previous(); else state.moveLabelSelection(-1) } }
    Connections { target: screen.labelWorkspace.nextButton; function onClicked() { if (root.datasetLabelController) root.datasetLabelController.next(); else state.moveLabelSelection(1) } }
    Connections { target: screen.labelWorkspace.class0NameField; function onEditingFinished() { if (root.datasetLabelController) root.datasetLabelController.renameClass(0, screen.labelWorkspace.class0NameField.text) } }
    Connections { target: screen.labelWorkspace.class1NameField; function onEditingFinished() { if (root.datasetLabelController) root.datasetLabelController.renameClass(1, screen.labelWorkspace.class1NameField.text) } }
    Connections { target: screen.labelWorkspace.class2NameField; function onEditingFinished() { if (root.datasetLabelController) root.datasetLabelController.renameClass(2, screen.labelWorkspace.class2NameField.text) } }
    Connections { target: screen.labelWorkspace.saveAsButton; function onClicked() { if (root.datasetLabelController) labelDatasetSaveAsDialog.open(); else state.saveLabelDatasetAs() } }

    FileDialog {
        id: labelDatasetFileDialog
        title: qsTr("Open Dataset")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("OpenDSS Dataset (dataset.json)")]
        onAccepted: {
            if (root.datasetLabelController && selectedFile.toString() !== "")
                root.datasetLabelController.open(selectedFile)
        }
    }

    FolderDialog {
        id: labelDatasetSaveAsDialog
        title: qsTr("Save Dataset As")
        onAccepted: {
            if (root.datasetLabelController && selectedFolder.toString() !== "")
                root.datasetLabelController.saveAs(selectedFolder)
        }
    }

    Connections { target: screen.sequenceViewerWorkspace.openSequenceButton; function onClicked() { if (root.sequenceViewerController) sequenceFileDialog.open(); else state.openViewerSequence() } }
    Connections { target: screen.sequenceViewerWorkspace.previousButton; function onClicked() { if (root.sequenceViewerController) root.sequenceViewerController.previous(); else state.previousViewerFrame() } }
    Connections { target: screen.sequenceViewerWorkspace.nextButton; function onClicked() { if (root.sequenceViewerController) root.sequenceViewerController.next(); else state.nextViewerFrame() } }
    Connections { target: screen.sequenceViewerWorkspace.directSeekField; function onAccepted() { if (root.sequenceViewerController) root.sequenceViewerController.seek(Number(screen.sequenceViewerWorkspace.directSeekField.text)); else state.seekViewerFrame(screen.sequenceViewerWorkspace.directSeekField.text) } }

    FileDialog {
        id: sequenceFileDialog
        title: qsTr("Open Image Sequence")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("OpenDSS Image Sequence (sequence.json)")]
        onAccepted: {
            const path = root.localFilePath(selectedFile)
            if (root.sequenceViewerController && path !== "")
                root.sequenceViewerController.open(path)
        }
    }

    FolderDialog {
        id: settingsStorageRootDialog
        title: qsTr("Choose Default Data Root")
        currentFolder: root.settingsController ? root.settingsController.storageRoot : ""
        onAccepted: {
            if (root.settingsController)
                root.settingsActionError = root.settingsController.setStorageRoot(selectedFolder)
        }
    }

    FileDialog {
        id: trainingDatasetFileDialog
        title: qsTr("Open Dataset")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("OpenDSS Dataset (dataset.json)")]
        onAccepted: {
            if (root.trainingController && selectedFile.toString() !== "")
                root.trainingController.datasetManifestUrl = selectedFile
        }
    }

    FolderDialog {
        id: trainingOutputDirectoryDialog
        title: qsTr("Choose Training Output Folder")
        currentFolder: root.trainingController ? root.trainingController.outputDirectoryUrl : ""
        onAccepted: {
            if (root.trainingController && selectedFolder.toString() !== "")
                root.trainingController.outputDirectoryUrl = selectedFolder
        }
    }

    Connections { target: screen.trainWorkspace.selectDatasetButton; function onClicked() { if (root.trainingController) trainingDatasetFileDialog.open(); else state.selectTrainDataset() } }
    Connections { target: screen.trainWorkspace.modelNameField; function onTextEdited() { if (root.trainingController) root.trainingController.modelName = screen.trainWorkspace.modelNameField.text; else state.trainModelNameDraft = screen.trainWorkspace.modelNameField.text } }
    Connections { target: screen.trainWorkspace.saveLocationField; function onTextEdited() { if (!root.trainingController) state.trainSaveLocationDraft = screen.trainWorkspace.saveLocationField.text } }
    Connections { target: screen.trainWorkspace.browseButton; function onClicked() { if (root.trainingController) trainingOutputDirectoryDialog.open(); else state.browseTrainSaveLocation() } }
    Connections { target: screen.trainWorkspace.architectureSelector; function onActivated(index) { if (root.trainingController) root.trainingController.architecture = index === 1 ? "efficientnet" : "mobilenet" } }
    Connections { target: screen.trainWorkspace.trainingDeviceSelector; function onActivated(index) { if (root.trainingController) root.trainingController.requestedDevice = index === 1 ? "cpu" : "gpu" } }
    Connections { target: screen.trainWorkspace.startButton; function onClicked() { if (root.trainingController) root.trainingController.start(); else state.startTraining() } }
    Connections { target: screen.trainWorkspace.stopButton; function onClicked() { if (root.trainingController) root.trainingController.stop(); else state.stopTraining() } }
    Connections { target: screen.trainWorkspace.retrySaveButton; function onClicked() { if (root.trainingController) root.trainingController.retrySave(); else state.retryTrainingSave() } }
    Connections {
        target: screen.trainWorkspace.openInModelTestButton
        function onClicked() {
            if (root.trainingController) {
                if (root.trainingController.presentation === "completed") {
                    state.modelTestDatasetSelected = false
                    state.modelTestPresentation = "modelOnly"
                    state.selectWorkspace("modelTest")
                }
            } else {
                state.openTrainingInModelTest()
            }
        }
    }
    Connections { target: screen.trainWorkspace.trainingSetupHeadingButton; function onClicked() { state.toggleTrainingSetup() } }
    Connections { target: screen.trainWorkspace.trainingStatusHeadingButton; function onClicked() { state.toggleTrainingStatus() } }
    Connections { target: screen.trainWorkspace.operationPanelToggleButton; function onClicked() { state.toggleTrainOperationPanel() } }

    Connections {
        target: screen.modelLibraryWorkspace.modelRowButtonGroup
        function onClicked(button) {
            if (root.modelLibraryController)
                root.modelLibraryController.select(button.rowIndex)
        }
    }
    Connections { target: screen.modelLibraryWorkspace.activeModelRowButton; function onClicked() { if (!root.modelLibraryController) state.selectActiveLibraryModel() } }
    Connections { target: screen.modelLibraryWorkspace.candidateModelRowButton; function onClicked() { if (!root.modelLibraryController) state.selectCandidateLibraryModel() } }
    Connections { target: screen.modelLibraryWorkspace.setActiveButton; function onClicked() { if (root.modelLibraryController && !screen.modelLibraryWorkspace.modelLocked) root.modelLibraryController.setActive(); else if (!root.modelLibraryController) state.setCandidateModelActive() } }
    Connections {
        target: screen.modelLibraryWorkspace.openInModelTestButton
        function onClicked() {
            if (root.modelLibraryController) {
                if (root.modelLibraryController.selectedId !== ""
                        && root.modelLibraryController.selectedId === root.modelLibraryController.activeId) {
                    state.modelTestDatasetSelected = false
                    state.modelTestPresentation = "modelOnly"
                    state.selectWorkspace("modelTest")
                }
            } else {
                state.openLibraryModelTest()
            }
        }
    }
    Connections { target: screen.modelLibraryWorkspace.importButton; function onClicked() { if (root.modelLibraryController && root.modelLibraryController.canImport) modelImportFolderDialog.open() } }
    Connections { target: screen.modelLibraryWorkspace.exportButton; function onClicked() { if (root.modelLibraryController && root.modelLibraryController.canExport) modelExportFolderDialog.open() } }
    Connections { target: screen.modelLibraryWorkspace.duplicateButton; function onClicked() { if (root.modelLibraryController && root.modelLibraryController.canDuplicate) modelDuplicateDialog.open() } }
    Connections { target: screen.modelLibraryWorkspace.deleteButton; function onClicked() { if (root.modelLibraryController && root.modelLibraryController.canDelete) modelDeleteDialog.open() } }
    Connections { target: screen.modelLibraryWorkspace.renameButton; function onClicked() { if (root.modelLibraryController && !screen.modelLibraryWorkspace.modelLocked) modelRenameDialog.open() } }
    Connections { target: screen.modelLibraryWorkspace.selectedModelHeadingButton; function onClicked() { state.toggleSelectedModel() } }
    Connections { target: screen.modelLibraryWorkspace.rightPanelToggleButton; function onClicked() { state.toggleModelLibraryRightPanel() } }

    FileDialog {
        id: modelTestDatasetFileDialog
        title: qsTr("Open Dataset")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("OpenDSS Dataset (dataset.json)")]
        onAccepted: {
            if (root.modelTestController && selectedFile.toString() !== "")
                root.modelTestController.datasetManifestUrl = selectedFile
        }
    }

    FolderDialog {
        id: modelTestOutputFolderDialog
        title: qsTr("Choose Model Test Output Parent Folder")
        currentFolder: root.modelTestController ? root.modelTestController.outputFolderUrl : ""
        onAccepted: {
            if (root.modelTestController && selectedFolder.toString() !== "")
                root.modelTestController.outputFolderUrl = selectedFolder
        }
    }

    Connections {
        target: screen.modelTestWorkspace.selectDatasetButton
        function onClicked() {
            if (root.modelTestController) {
                modelTestDatasetFileDialog.open()
            } else if (root.modelLibraryController && root.modelLibraryController.activeId !== "") {
                state.modelTestDatasetSelected = true
                state.modelTestPresentation = "readyGpu"
            } else {
                state.selectModelTestDataset()
            }
        }
    }
    Connections { target: screen.modelTestWorkspace.outputLocationField; function onTextEdited() { if (!root.modelTestController) state.modelTestOutputLocationDraft = screen.modelTestWorkspace.outputLocationField.text } }
    Connections { target: screen.modelTestWorkspace.browseButton; function onClicked() { if (root.modelTestController) modelTestOutputFolderDialog.open(); else state.browseModelTestOutput() } }
    Connections { target: screen.modelTestWorkspace.startButton; function onClicked() { if (root.modelTestController) root.modelTestController.start(); else state.startModelTest() } }
    Connections { target: screen.modelTestWorkspace.stopButton; function onClicked() { if (root.modelTestController) root.modelTestController.stop(); else state.stopModelTest() } }
    Connections { target: screen.modelTestWorkspace.openPredictionsButton; function onClicked() { if (root.modelTestController) root.modelTestController.openPredictions(); else state.openModelTestArtifact() } }
    Connections { target: screen.modelTestWorkspace.openSummaryButton; function onClicked() { if (root.modelTestController) root.modelTestController.openSummary(); else state.openModelTestArtifact() } }
    Connections { target: screen.modelTestWorkspace.startAnotherButton; function onClicked() { if (!root.modelTestController) state.startAnotherModelTest() } }
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
    Connections { target: screen.runsWorkspace.loadSelectedRunButton; function onClicked() { if (root.runsResultsController) root.runsResultsController.loadSelected(); else state.loadSelectedRun() } }
    Connections { target: screen.runsWorkspace.editNotesButton; function onClicked() { state.editRunNotes() } }
    Connections { target: screen.runsWorkspace.saveNotesButton; function onClicked() { state.finishRunNotesEditing() } }
    Connections {
        target: screen.settingsWorkspace.textSizeSelector
        function onActivated() {
            if (root.settingsController)
                root.settingsController.setTextSizePercent([80, 100, 125][screen.settingsWorkspace.textSizeSelector.currentIndex])
        }
    }
    Connections { target: screen.settingsWorkspace.chooseDataRootButton; function onClicked() { if (root.settingsController) settingsStorageRootDialog.open() } }
    Connections { target: screen.settingsWorkspace.openDataRootButton; function onClicked() { if (root.settingsController) root.settingsActionError = root.settingsController.openStorageRoot() } }
    Connections { target: screen.runsWorkspace.cancelNotesButton; function onClicked() { state.finishRunNotesEditing() } }

    Component {
        id: runRowDelegate

        Rectangle {
            id: runRow
            required property var modelData
            readonly property var run: modelData
            readonly property bool selected: screen.runsWorkspace.selectedRunId === run.id
            width: parent.width
            height: 94
            color: selected ? "#e8f0fa" : Constants.backgroundColor
            border.color: activeFocus || selected ? Constants.accentColor : Constants.borderColor
            activeFocusOnTab: root.runsResultsController && run.loadable
            Accessible.name: run.runName
            Accessible.role: Accessible.ListItem

            Column {
                anchors.fill: parent
                anchors.margins: 7
                Text { text: qsTr("Run Name: %1").arg(run.runName); color: Constants.textColor; font: Constants.smallFont }
                Text { text: run.loadable ? run.statusText || qsTr("%1  |  %2").arg(run.operation).arg(run.status) : qsTr("Unavailable"); color: Constants.textColor; font: Constants.smallFont }
                Text { text: run.loadable ? run.timingText || qsTr("Started: %1  |  Duration: %2 s").arg(run.startedAt).arg(run.durationSeconds) : run.reason; color: Constants.mutedTextColor; font: Constants.smallFont; elide: Text.ElideRight; width: parent.width }
                Text { text: run.loadable ? run.summaryText || qsTr("Total Droplets: %1  |  Model: %2").arg(run.totalCount).arg(run.modelName === "" ? qsTr("No model") : run.modelName) : ""; color: Constants.mutedTextColor; font: Constants.smallFont; elide: Text.ElideRight; width: parent.width }
            }

            TapHandler {
                enabled: root.runsResultsController && runRow.run.loadable
                onTapped: {
                    runRow.forceActiveFocus()
                    root.runsResultsController.selectRun(runRow.run.id)
                }
            }
            Keys.onReturnPressed: root.runsResultsController.selectRun(run.id)
            Keys.onSpacePressed: root.runsResultsController.selectRun(run.id)
            Keys.enabled: root.runsResultsController && run.loadable
        }
    }

    Repeater {
        parent: screen.runsWorkspace.runsRowsHost
        model: root.runsResultsController ? root.runsResultsController.runs
                                          : state.runsPresentation === "runsEmpty" || state.runsPresentation === "runsError" ? []
                                          : [
                                                {
                                                    id: "Run-042",
                                                    runName: "Run-042",
                                                    loadable: true,
                                                    statusText: state.run042RowStatusText,
                                                    timingText: qsTr("Started: 2026-07-23 10:41  |  Duration: 00:03:12"),
                                                    summaryText: qsTr("Total Droplets: 1,248  |  Model: DropletNet-04")
                                                },
                                                {
                                                    id: "Run-043",
                                                    runName: "Run-043",
                                                    loadable: true,
                                                    statusText: qsTr("Sequence Test  |  Stopped"),
                                                    timingText: qsTr("Started: 2026-07-23 11:08  |  Duration: 00:02:26"),
                                                    summaryText: qsTr("Total Droplets: 876  |  Model: No model")
                                                }
                                            ]
        delegate: runRowDelegate
    }

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

    Connections {
        target: screen.singleImageSection.headingButton
        function onClicked() {
            if (!root.singleImageCapturing)
                state.toggleSingleImage()
        }
    }
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
            if (root.singleImageCapturing)
                return
            if (root.singleImageCaptureController)
                root.singleImageCaptureController.fileName = screen.fileNameField.text
            else
                state.fileNameDraft = screen.fileNameField.text
        }
    }

    Connections {
        target: screen.saveLocationField
        function onTextEdited() {
            if (root.singleImageCapturing)
                return
            if (root.singleImageCaptureController)
                root.singleImageCaptureController.setOutputFolderPath(
                            screen.saveLocationField.text)
            else
                state.saveLocationDraft = screen.saveLocationField.text
        }
    }

    FolderDialog {
        id: singleImageFolderDialog
        title: qsTr("Choose Image Save Location")
        currentFolder: root.singleImageCaptureController
                       ? root.singleImageCaptureController.outputFolder : ""
        onAccepted: {
            if (root.singleImageCaptureController && !root.singleImageCapturing)
                root.singleImageCaptureController.outputFolder = selectedFolder
        }
    }

    Connections {
        target: screen.browseButton
        function onClicked() {
            if (root.singleImageCapturing)
                return
            if (root.singleImageCaptureController)
                singleImageFolderDialog.open()
            else
                state.browse()
        }
    }

    Connections {
        target: screen.captureButton
        function onClicked() {
            if (root.singleImageCaptureController)
                root.singleImageCaptureController.capture()
            else
                state.capture()
        }
    }
}
