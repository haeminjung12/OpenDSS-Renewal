import QtQuick
import Desktop_app_v2

Window {
    id: window

    property var settingsController: null
    property var runsResultsController: null
    property var sequenceViewerController: null

    width: Constants.width
    height: Constants.height
    minimumWidth: Constants.width
    minimumHeight: Constants.height

    title: qsTr("OpenDSS")

    Component.onCompleted: window.showMaximized()

    ShellSingleImage {
        anchors.fill: parent
        settingsController: window.settingsController
        property var runsResultsController: window.runsResultsController
        onCloseRequested: window.close()
    }

}

