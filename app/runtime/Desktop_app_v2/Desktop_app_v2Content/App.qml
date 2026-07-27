import QtQuick
import Desktop_app_v2

Window {
    id: window

    property var settingsController: null
    property var daqController: null
    property var runsResultsController: null
    property var sequenceViewerController: null
    property var datasetLabelController: null
    property var trainingController: null
    property var modelLibraryController: null
    property var modelTestController: null
    property var liveSortingController: null
    property var sequenceTestController: null
    property var captureWorkflowController: null

    title: qsTr("OpenDSS")
    visibility: Window.Maximized

    ShellSingleImage {
        anchors.fill: parent
        settingsController: window.settingsController
        daqController: window.daqController
        runsResultsController: window.runsResultsController
        sequenceViewerController: window.sequenceViewerController
        datasetLabelController: window.datasetLabelController
        trainingController: window.trainingController
        modelLibraryController: window.modelLibraryController
        modelTestController: window.modelTestController
        liveSortingController: window.liveSortingController
        sequenceTestController: window.sequenceTestController
        captureWorkflowController: window.captureWorkflowController
        onCloseRequested: window.close()
    }

}

