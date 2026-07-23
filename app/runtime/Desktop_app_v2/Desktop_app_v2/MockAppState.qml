import QtQml

QtObject {
    id: root

    property string selectedWorkspace: "capture"
    property bool cameraAvailable: false
    property bool cameraStreaming: false
    property string daqStatus: qsTr("Ready")
    property string activeModelText: qsTr("No Active Model")
    property bool hardwareDrawerOpen: false
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
    property string activeCapture: ""
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

    readonly property bool captureRunning: activeCapture !== "" && (capturePhase === "running" || capturePhase === "paused")
    readonly property bool cameraLocked: capturing || captureRunning
    readonly property string cameraStatus: !cameraAvailable ? qsTr("Unavailable") : cameraStreaming ? qsTr("Streaming") : qsTr("Connected")
    readonly property string activityText: capturing ? qsTr("Capturing Image")
                                                   : activeCapture === "sequence" && capturePhase === "running" ? qsTr("Recording Image Sequence")
                                                   : activeCapture === "sequence" && capturePhase === "paused" ? qsTr("Image Sequence paused")
                                                   : activeCapture === "dataset" && capturePhase === "running" ? qsTr("Capturing Droplet Dataset")
                                                   : activeCapture === "dataset" && capturePhase === "paused" ? qsTr("Droplet Dataset Capture paused")
                                                   : qsTr("Idle")
    readonly property bool captureEnabled: !capturing && !captureFailed && cameraStatus === qsTr("Streaming") && !captureRunning
    readonly property string disabledReason: capturing ? qsTr("Capture is already in progress")
                                                   : captureRunning ? qsTr("Another operation is active")
                                                   : cameraStatus === qsTr("Unavailable") ? qsTr("Camera unavailable")
                                                   : cameraStatus !== qsTr("Streaming") ? qsTr("Start Camera") : ""
    readonly property bool showBanner: captureFailed
    readonly property string bannerHeading: qsTr("Error")
    readonly property string bannerText: ""
    readonly property bool showSavedPath: savedPath !== ""
    readonly property string singleImagePresentation: capturing ? "capturing" : captureFailed ? "error" : cameraStatus === qsTr("Streaming") ? (savedPath !== "" ? "completed" : "ready") : "unavailable"
    readonly property bool otherCaptureHeadingsDisabled: capturing || captureRunning
    readonly property bool cameraPromptVisible: cameraStatus === qsTr("Unavailable") && !cameraPromptHandled
    function browse() { saveLocationDraft = qsTr("C:/OpenDSS/MockImages") }
    function browseSequence() { sequenceLocationDraft = qsTr("C:/OpenDSS/MockSequences") }
    function browseDataset() { datasetLocationDraft = qsTr("C:/OpenDSS/MockDatasets") }
    function selectCameraDevice(available) { cameraAvailable = available; if (!available) cameraStreaming = false }
    function toggleCameraStreaming() { if (cameraAvailable && !cameraLocked) cameraStreaming = !cameraStreaming }
    function capture() {
        if (!captureEnabled) return
        captureWillFail = nextCaptureFails; nextCaptureFails = false; captureFailed = false; savedPath = ""; singleImageOpen = true; capturing = true; captureTimer.restart()
    }
    function startSequence() { if (cameraStatus === qsTr("Streaming") && !captureRunning) { activeCapture = "sequence"; capturePhase = "running"; imageSequenceOpen = true; sequenceFrameCount = 24 } }
    function startDataset() { if (cameraStatus === qsTr("Streaming") && !captureRunning) { activeCapture = "dataset"; capturePhase = "running"; datasetOpen = true; datasetFrameCount = 18; datasetCropCount = 42 } }
    function pauseOrResumeCapture() { if (captureRunning) capturePhase = capturePhase === "running" ? "paused" : "running" }
    function stopCapture() { if (captureRunning) capturePhase = "completed" }
    function openSequenceViewer() { selectedWorkspace = "sequenceViewer" }
    function openSequenceTest() { selectedWorkspace = "sequenceTest" }
    function startNewSequence() { activeCapture = ""; capturePhase = "ready"; imageSequenceOpen = true }
    function openLabel() { selectedWorkspace = "label" }
    function showMockFolder() { datasetHandoffText = qsTr("Illustrative mock — no folder opened") }
    function startNewDataset() { activeCapture = ""; capturePhase = "ready"; datasetOpen = true }
    function selectWorkspace(workspace) { selectedWorkspace = workspace }
    function toggleSingleImage() { if (!capturing && !captureRunning) singleImageOpen = !singleImageOpen }
    function toggleImageSequence() { if (!capturing && !captureRunning) imageSequenceOpen = !imageSequenceOpen }
    function toggleDataset() { if (!capturing && !captureRunning) datasetOpen = !datasetOpen }
    function continueWithoutCamera() { cameraPromptHandled = true; cameraPromptChoice = "yes" }
    function declineCamera() { cameraPromptHandled = true; cameraPromptChoice = "no" }

    property Timer captureTimer: Timer {
        interval: 100; repeat: false
        onTriggered: { root.capturing = false; root.captureFailed = root.captureWillFail; root.captureWillFail = false; if (!root.captureFailed) root.savedPath = qsTr("Illustrative mock path — no file written: C:/OpenDSS/MockImages/%1.tiff").arg(root.fileNameDraft === "" ? qsTr("single_image") : root.fileNameDraft) }
    }
}
