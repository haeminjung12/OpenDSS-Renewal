import QtQuick
import Desktop_app_v2

Window {
    width: mainScreen.width
    height: mainScreen.height

    visible: true
    title: "Desktop_app_v2"

    Screen01 {
        id: mainScreen

        anchors.centerIn: parent
    }

}

