import QtQuick
import Desktop_app_v2

Item {
    id: root

    anchors.fill: parent
    property alias mockState: state
    property alias form: screen

    MockAppState {
        id: state
    }

    Screen01 {
        id: screen
        anchors.fill: parent
        cameraStatus: state.cameraStatus
        daqStatus: state.daqStatus
        activityText: state.activityText
        fileNameText: state.fileNameDraft
        saveLocationText: state.saveLocationDraft
        disabledReason: state.disabledReason
        savedPath: state.savedPath
        bannerHeading: state.bannerHeading
        bannerText: state.bannerText
        captureEnabled: state.captureEnabled
        showSavedPath: state.showSavedPath
        showBanner: state.showBanner
        drawerOpen: state.hardwareDrawerOpen
    }

    Connections {
        target: screen.hardwareButton
        function onClicked() {
            state.hardwareDrawerOpen = !state.hardwareDrawerOpen
        }
    }

    Connections {
        target: screen.drawerCloseButton
        function onClicked() {
            state.hardwareDrawerOpen = false
            screen.hardwareButton.forceActiveFocus()
        }
    }

    Connections {
        target: screen.fileNameField
        function onTextEdited() {
            state.fileNameDraft = screen.fileNameField.text
        }
    }

    Connections {
        target: screen.saveLocationField
        function onTextEdited() {
            state.saveLocationDraft = screen.saveLocationField.text
        }
    }

    Connections {
        target: screen.browseButton
        function onClicked() {
            state.browse()
        }
    }

    Connections {
        target: screen.captureButton
        function onClicked() {
            state.capture()
        }
    }
}
