import QtQuick
import Desktop_app_v2

Window {
    id: window

    property var settingsController: null

    width: Constants.width
    height: Constants.height
    minimumWidth: Constants.width
    minimumHeight: Constants.height

    title: qsTr("OpenDSS")

    Component.onCompleted: window.showMaximized()

    ShellSingleImage {
        anchors.fill: parent
        settingsController: window.settingsController
        onCloseRequested: window.close()
    }

}

