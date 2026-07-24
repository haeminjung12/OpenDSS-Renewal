import QtQuick
import Desktop_app_v2

Window {
    id: window

    width: Constants.width
    height: Constants.height
    minimumWidth: Constants.width
    minimumHeight: Constants.height

    title: qsTr("OpenDSS")

    Component.onCompleted: window.showMaximized()

    ShellSingleImage {
        anchors.fill: parent
        onCloseRequested: window.close()
    }

}

