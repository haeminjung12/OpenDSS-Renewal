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

    property bool correctingAspectRatio: false

    function correctAspectRatio(fromWidth) {
        if (correctingAspectRatio || visibility === Window.Maximized || visibility === Window.FullScreen)
            return

        correctingAspectRatio = true
        if (fromWidth)
            height = Math.max(minimumHeight, Math.round(width * 9 / 16))
        else
            width = Math.max(minimumWidth, Math.round(height * 16 / 9))
        correctingAspectRatio = false
    }

    onWidthChanged: correctAspectRatio(true)
    onHeightChanged: correctAspectRatio(false)

    ShellSingleImage {
        anchors.fill: parent
        onCloseRequested: window.close()
    }

}

