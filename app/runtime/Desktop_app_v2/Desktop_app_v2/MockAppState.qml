import QtQml

QtObject {
    id: root

    property string cameraStatus: qsTr("Streaming")
    property bool conflictingOperation: false
    property bool hardwareDrawerOpen: false
    property bool nextCaptureFails: false
    property string fileNameDraft: ""
    property string saveLocationDraft: qsTr("C:/OpenDSS/Images")
    property bool capturing: false
    property bool captureFailed: false
    property bool captureWillFail: false
    property string savedPath: ""

    readonly property string daqStatus: qsTr("Ready")
    readonly property string activityText: capturing ? qsTr("Capturing Image") : qsTr("Idle")
    readonly property bool captureEnabled: !capturing && !captureFailed && cameraStatus === qsTr("Streaming") && !conflictingOperation
    readonly property string disabledReason: capturing ? qsTr("Capture is already in progress")
                                                   : conflictingOperation ? qsTr("Another operation is active")
                                                   : cameraStatus !== qsTr("Streaming") ? qsTr("Camera unavailable")
                                                   : captureFailed ? qsTr("Output folder is not writable") : ""
    readonly property bool showBanner: captureFailed
    readonly property string bannerHeading: qsTr("Capture Image failed")
    readonly property string bannerText: qsTr("The selected output folder could not be written. No image was saved.")
    readonly property bool showSavedPath: savedPath !== ""

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
        capturing = true
        captureTimer.restart()
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
