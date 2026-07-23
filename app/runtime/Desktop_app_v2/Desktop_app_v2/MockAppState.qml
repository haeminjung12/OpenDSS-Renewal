import QtQml

QtObject {
    id: root

    property string selectedWorkspace: "capture"
    property string cameraStatus: qsTr("Unavailable")
    property string daqStatus: qsTr("Ready")
    property string activeModelText: qsTr("No Active Model")
    property bool conflictingOperation: false
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

    readonly property string activityText: capturing ? qsTr("Capturing Image") : qsTr("Idle")
    readonly property bool captureEnabled: !capturing && !captureFailed && cameraStatus === qsTr("Streaming") && !conflictingOperation
    readonly property string disabledReason: capturing ? qsTr("Capture is already in progress")
                                                   : conflictingOperation ? qsTr("Another operation is active")
                                                   : cameraStatus !== qsTr("Streaming") ? qsTr("Camera unavailable")
                                                   : ""
    readonly property bool showBanner: captureFailed
    readonly property string bannerHeading: qsTr("Error")
    readonly property string bannerText: ""
    readonly property bool showSavedPath: savedPath !== ""
    readonly property string singleImagePresentation: capturing ? "capturing"
                                                             : captureFailed ? "error"
                                                             : cameraStatus === qsTr("Streaming") ? (savedPath !== "" ? "completed" : "ready")
                                                             : "unavailable"
    readonly property bool otherCaptureHeadingsDisabled: capturing
    readonly property bool cameraPromptVisible: cameraStatus === qsTr("Unavailable") && !cameraPromptHandled

    function browse() {
        saveLocationDraft = qsTr("C:/OpenDSS/MockImages")
    }

    function capture() {
        if (!captureEnabled)
            return

        captureWillFail = nextCaptureFails
        nextCaptureFails = false
        captureFailed = false
        savedPath = ""
        singleImageOpen = true
        capturing = true
        captureTimer.restart()
    }

    function selectWorkspace(workspace) {
        selectedWorkspace = workspace
    }

    function toggleSingleImage() {
        if (!capturing)
            singleImageOpen = !singleImageOpen
    }

    function toggleImageSequence() {
        if (!capturing)
            imageSequenceOpen = !imageSequenceOpen
    }

    function toggleDataset() {
        if (!capturing)
            datasetOpen = !datasetOpen
    }

    function continueWithoutCamera() {
        cameraPromptHandled = true
        cameraPromptChoice = "yes"
    }

    function declineCamera() {
        cameraPromptHandled = true
        cameraPromptChoice = "no"
    }

    property Timer captureTimer: Timer {
        interval: 100
        repeat: false
        onTriggered: {
            root.capturing = false
            root.captureFailed = root.captureWillFail
            root.captureWillFail = false
            if (!root.captureFailed)
                root.savedPath = qsTr("Illustrative mock path — no file written: C:/OpenDSS/MockImages/%1.tiff").arg(root.fileNameDraft === "" ? qsTr("single_image") : root.fileNameDraft)
        }
    }
}
