/*
This is a UI file (.ui.qml) intended for Qt Design Studio editing.
*/

import QtQuick
import QtQuick.Controls
import Desktop_app_v2

Rectangle {
    id: root
    width: Constants.width
    height: Constants.height
    color: Constants.backgroundColor

    property string cameraStatus: qsTr("Streaming")
    property string daqStatus: qsTr("Ready")
    property string activityText: qsTr("Idle")
    property string fileNameText: ""
    property string saveLocationText: qsTr("C:/OpenDSS/Images")
    property string disabledReason: ""
    property string savedPath: ""
    property string bannerHeading: ""
    property string bannerText: ""
    property bool captureEnabled: true
    property bool showSavedPath: false
    property bool showBanner: false
    property bool drawerOpen: false
    property alias hardwareButton: hardwareButton
    property alias fileNameField: fileNameField
    property alias saveLocationField: saveLocationField
    property alias browseButton: browseButton
    property alias captureButton: captureButton
    property alias drawerCloseButton: drawerCloseButton

    Rectangle {
        id: header
        height: Constants.shellHeaderHeight
        color: Constants.surfaceColor
        border.color: Constants.borderColor
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right

        Row {
            spacing: 28
            anchors.left: parent.left
            anchors.leftMargin: Constants.spacing * 2
            anchors.verticalCenter: parent.verticalCenter

            Text { text: qsTr("Camera: %1").arg(root.cameraStatus); color: Constants.textColor; font: Constants.font }
            Text { text: qsTr("DAQ: %1").arg(root.daqStatus); color: Constants.textColor; font: Constants.font }
            Text { text: qsTr("Active Model: No Active Model"); color: Constants.textColor; font: Constants.font }
            Text { text: qsTr("Current Activity: %1").arg(root.activityText); color: Constants.textColor; font: Constants.font }
        }

        Button {
            id: hardwareButton
            text: qsTr("Hardware")
            height: Constants.controlHeight
            activeFocusOnTab: true
            Accessible.name: qsTr("Hardware drawer")
            KeyNavigation.tab: singleImageNavigationButton
            anchors.right: parent.right
            anchors.rightMargin: Constants.spacing * 2
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Rectangle {
        id: navigation
        width: Constants.navigationWidth
        color: Constants.surfaceColor
        border.color: Constants.borderColor
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left

        Column {
            spacing: 6
            anchors.fill: parent
            anchors.margins: Constants.spacing

            Text { text: qsTr("Data"); color: Constants.textColor; font.family: Constants.font.family; font.pixelSize: Constants.font.pixelSize; font.bold: true }
            Text { text: qsTr("  Capture"); color: Constants.textColor; font: Constants.font }
            Button {
                id: singleImageNavigationButton
                text: qsTr("Single Image")
                width: parent.width
                height: Constants.controlHeight
                checkable: true
                checked: true
                activeFocusOnTab: true
                Accessible.name: qsTr("Data Capture Single Image")
                KeyNavigation.backtab: hardwareButton
                KeyNavigation.tab: captureModeButton
            }
            Text { text: qsTr("    Image Sequence"); color: Constants.mutedTextColor; font: Constants.font }
            Text { text: qsTr("    Dataset Capture"); color: Constants.mutedTextColor; font: Constants.font }
            Text { text: qsTr("  Label"); color: Constants.mutedTextColor; font: Constants.font }
            Text { text: qsTr("  Sequence Player"); color: Constants.mutedTextColor; font: Constants.font }
            Item { width: 1; height: 10 }
            Text { text: qsTr("Models"); color: Constants.textColor; font.family: Constants.font.family; font.pixelSize: Constants.font.pixelSize; font.bold: true }
            Text { text: qsTr("  Train"); color: Constants.mutedTextColor; font: Constants.font }
            Text { text: qsTr("  Model Test"); color: Constants.mutedTextColor; font: Constants.font }
            Text { text: qsTr("  Library"); color: Constants.mutedTextColor; font: Constants.font }
            Item { width: 1; height: 10 }
            Text { text: qsTr("Sort"); color: Constants.textColor; font.family: Constants.font.family; font.pixelSize: Constants.font.pixelSize; font.bold: true }
            Text { text: qsTr("  Live"); color: Constants.mutedTextColor; font: Constants.font }
            Text { text: qsTr("  Sequence Test"); color: Constants.mutedTextColor; font: Constants.font }
            Item { width: 1; height: 10 }
            Text { text: qsTr("Results"); color: Constants.textColor; font.family: Constants.font.family; font.pixelSize: Constants.font.pixelSize; font.bold: true }
            Text { text: qsTr("  Runs"); color: Constants.mutedTextColor; font: Constants.font }
            Item { width: 1; height: 10 }
            Text { text: qsTr("Settings"); color: Constants.textColor; font.family: Constants.font.family; font.pixelSize: Constants.font.pixelSize; font.bold: true }
        }
    }

    Item {
        id: workspace
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.left: navigation.right
        anchors.right: operationPanel.left

        Column {
            spacing: Constants.spacing
            anchors.fill: parent
            anchors.margins: Constants.spacing * 2

            Text { text: qsTr("Data > Capture"); color: Constants.mutedTextColor; font: Constants.font }
            Row {
                spacing: 8
                Button {
                    id: captureModeButton
                    text: qsTr("Single Image")
                    height: Constants.controlHeight
                    checkable: true
                    checked: true
                    activeFocusOnTab: true
                    Accessible.name: qsTr("Single Image capture mode")
                    KeyNavigation.backtab: singleImageNavigationButton
                    KeyNavigation.tab: fileNameField
                }
                Button { text: qsTr("Image Sequence"); height: Constants.controlHeight; Accessible.name: qsTr("Image Sequence capture mode") }
                Button { text: qsTr("Dataset Capture"); height: Constants.controlHeight; Accessible.name: qsTr("Dataset Capture mode") }
            }
            Rectangle {
                width: parent.width
                height: parent.height - 112
                color: Constants.viewerColor
                border.color: Constants.borderColor
                Text {
                    text: root.cameraStatus === qsTr("Unavailable") ? qsTr("Camera unavailable") : qsTr("Live camera preview")
                    color: Constants.surfaceColor
                    font: Constants.largeFont
                    anchors.centerIn: parent
                }
            }
        }

        Rectangle {
            visible: root.showBanner
            height: 92
            color: "#fff2f0"
            border.color: Constants.faultColor
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Constants.spacing * 2
            Column {
                spacing: 4
                anchors.fill: parent
                anchors.margins: 12
                Text { text: root.bannerHeading; color: Constants.faultColor; font.family: Constants.font.family; font.pixelSize: Constants.font.pixelSize; font.bold: true; Accessible.role: Accessible.Heading }
                Text { text: root.bannerText; color: Constants.textColor; font: Constants.font; wrapMode: Text.WordWrap; width: parent.width }
            }
        }
    }

    Rectangle {
        id: operationPanel
        width: Constants.operationPanelWidth
        color: Constants.surfaceColor
        border.color: Constants.borderColor
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.right: parent.right

        Column {
            spacing: 10
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: Constants.spacing * 2

            Text { text: qsTr("Single Image"); color: Constants.textColor; font.bold: true; font.pixelSize: Constants.font.pixelSize * 1.35; Accessible.role: Accessible.Heading }
            Text { text: qsTr("File Name"); color: Constants.textColor; font: Constants.font }
            TextField {
                id: fileNameField
                text: root.fileNameText
                placeholderText: qsTr("Optional — timestamp used")
                height: Constants.controlHeight
                width: parent.width
                Accessible.name: qsTr("File Name")
                KeyNavigation.backtab: captureModeButton
                KeyNavigation.tab: saveLocationField
            }
            Text { text: qsTr("Save Location"); color: Constants.textColor; font: Constants.font }
            Row {
                spacing: 8
                width: parent.width
                TextField {
                    id: saveLocationField
                    text: root.saveLocationText
                    height: Constants.controlHeight
                    width: parent.width - browseButton.width - 8
                    Accessible.name: qsTr("Save Location")
                    KeyNavigation.backtab: fileNameField
                    KeyNavigation.tab: browseButton
                }
                Button {
                    id: browseButton
                    text: qsTr("Browse")
                    height: Constants.controlHeight
                    Accessible.name: qsTr("Browse save location")
                    KeyNavigation.backtab: saveLocationField
                    KeyNavigation.tab: captureButton
                }
            }
            Text { visible: root.showSavedPath; text: qsTr("Saved: %1").arg(root.savedPath); color: Constants.textColor; font: Constants.font; wrapMode: Text.WordWrap; width: parent.width }
        }
        Column {
            spacing: 8
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Constants.spacing * 2
            Button {
                id: captureButton
                text: root.activityText === qsTr("Capturing Image") ? qsTr("Capturing Image…") : qsTr("Capture Image")
                enabled: root.captureEnabled
                height: 44
                width: parent.width
                Accessible.name: qsTr("Capture Image")
                KeyNavigation.backtab: browseButton
                KeyNavigation.tab: hardwareButton
            }
            Text { visible: root.disabledReason !== ""; text: root.disabledReason; color: Constants.warningColor; font: Constants.font; wrapMode: Text.WordWrap; width: parent.width }
        }
    }

    Rectangle {
        id: drawer
        visible: root.drawerOpen
        width: Constants.drawerWidth
        color: Constants.surfaceColor
        border.color: Constants.borderColor
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.right: parent.right

        Column {
            spacing: 14
            anchors.fill: parent
            anchors.margins: Constants.spacing * 2
            Text { text: qsTr("Hardware"); color: Constants.textColor; font.bold: true; font.pixelSize: Constants.font.pixelSize * 1.35; Accessible.role: Accessible.Heading }
            Text { text: qsTr("Camera"); color: Constants.textColor; font.family: Constants.font.family; font.pixelSize: Constants.font.pixelSize; font.bold: true }
            Text { text: qsTr("Status: %1").arg(root.cameraStatus); color: Constants.mutedTextColor; font: Constants.font }
            Text { visible: root.cameraStatus === qsTr("Unavailable"); text: qsTr("Camera unavailable"); color: Constants.warningColor; font: Constants.font }
            Text { text: qsTr("DAQ"); color: Constants.textColor; font.family: Constants.font.family; font.pixelSize: Constants.font.pixelSize; font.bold: true }
            Text { text: qsTr("Status: %1").arg(root.daqStatus); color: Constants.mutedTextColor; font: Constants.font }
            Button { id: drawerCloseButton; text: qsTr("Close"); height: Constants.controlHeight; Accessible.name: qsTr("Close hardware drawer") }
        }
    }

    states: [
        State { name: "unavailable"; PropertyChanges { root.cameraStatus: qsTr("Unavailable"); root.activityText: qsTr("Idle"); root.captureEnabled: false; root.disabledReason: qsTr("Camera unavailable"); root.showSavedPath: false; root.showBanner: false } },
        State { name: "ready"; PropertyChanges { root.cameraStatus: qsTr("Streaming"); root.activityText: qsTr("Idle"); root.fileNameText: ""; root.saveLocationText: qsTr("C:/OpenDSS/Images"); root.captureEnabled: true; root.disabledReason: ""; root.showSavedPath: false; root.showBanner: false } },
        State { name: "customFilename"; PropertyChanges { root.cameraStatus: qsTr("Streaming"); root.fileNameText: qsTr("sample_042"); root.saveLocationText: qsTr("D:/Research/Session 12"); root.captureEnabled: true; root.disabledReason: ""; root.showSavedPath: false; root.showBanner: false } },
        State { name: "capturing"; PropertyChanges { root.cameraStatus: qsTr("Streaming"); root.activityText: qsTr("Capturing Image"); root.captureEnabled: false; root.disabledReason: ""; root.showSavedPath: false; root.showBanner: false } },
        State { name: "completed"; PropertyChanges { root.cameraStatus: qsTr("Streaming"); root.activityText: qsTr("Idle"); root.captureEnabled: true; root.savedPath: qsTr("D:/Research/Session 12/sample_042.tiff"); root.showSavedPath: true; root.showBanner: false; root.disabledReason: "" } },
        State { name: "failed"; PropertyChanges { root.cameraStatus: qsTr("Streaming"); root.activityText: qsTr("Idle"); root.captureEnabled: false; root.showSavedPath: false; root.showBanner: true; root.bannerHeading: qsTr("Capture Image failed"); root.bannerText: qsTr("The selected output folder could not be written. No image was saved."); root.disabledReason: qsTr("Output folder is not writable") } },
        State { name: "conflict"; PropertyChanges { root.cameraStatus: qsTr("Streaming"); root.activityText: qsTr("Idle"); root.captureEnabled: false; root.disabledReason: qsTr("Another operation is active"); root.showSavedPath: false; root.showBanner: false } },
        State { name: "drawerOpen"; PropertyChanges { root.drawerOpen: true } }
    ]
}
