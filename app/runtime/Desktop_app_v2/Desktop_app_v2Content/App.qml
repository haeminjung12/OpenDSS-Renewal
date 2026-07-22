import QtQuick
import Desktop_app_v2

Window {
    width: Constants.width
    height: Constants.height
    minimumWidth: 1280
    minimumHeight: 720

    visible: true
    title: qsTr("OpenDSS")

    Screen01 {
        id: mainScreen

        anchors.fill: parent
    }

}

