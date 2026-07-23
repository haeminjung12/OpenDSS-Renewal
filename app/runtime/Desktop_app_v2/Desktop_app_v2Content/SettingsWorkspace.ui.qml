/* This is a UI file (.ui.qml) intended for Qt Design Studio editing. */
import QtQuick
import QtQuick.Controls
import Desktop_app_v2

Rectangle {
    id: root
    width: Constants.width
    height: Constants.height
    color: Constants.backgroundColor

    property string settingsPresentation: "ready"
    property string defaultDataRoot: qsTr("C:/Users/Scientist/Documents/OpenDropletSortingSuite")
    property int textSizePercent: 100
    property alias textSizeSlider: textSizeSlider

    Text {
        id: workspaceTitle
        text: qsTr("Settings")
        font: Constants.largeFont
        color: Constants.textColor
        height: Constants.controlHeight
        verticalAlignment: Text.AlignVCenter
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: Constants.workspaceMargin
    }

    Column {
        width: parent.width * 0.66
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: workspaceTitle.bottom
        anchors.topMargin: Constants.spacing
        anchors.margins: Constants.workspaceMargin
        spacing: Constants.spacing

        Rectangle {
            visible: root.settingsPresentation === "error"
            width: parent.width
            height: 52
            color: Constants.errorSurfaceColor
            border.color: Constants.faultColor
            Text { text: qsTr("Error"); color: Constants.faultColor; font: Constants.headingFont; anchors.centerIn: parent }
        }

        Rectangle {
            width: parent.width
            height: storageContent.implicitHeight + Constants.spacing * 2
            color: Constants.surfaceColor
            border.color: Constants.borderColor
            Column { id: storageContent; anchors.fill: parent; anchors.margins: Constants.spacing; spacing: 8
                Text { text: qsTr("Storage"); font: Constants.headingFont; color: Constants.textColor }
                Text { text: qsTr("Default Data Root"); color: Constants.textColor; font: Constants.smallFont }
                Rectangle { width: parent.width; height: 28; color: Constants.backgroundColor; border.color: Constants.borderColor; Text { text: root.defaultDataRoot; elide: Text.ElideMiddle; color: Constants.textColor; font: Constants.smallFont; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.right: parent.right; anchors.margins: 6 } }
                Flow { width: parent.width; height: implicitHeight; spacing: Constants.spacing
                    Button { id: chooseDataRootButton; text: qsTr("Choose Default Data Root"); height: Constants.controlHeight }
                    Button { id: openDataRootButton; text: qsTr("Open Data Root"); height: Constants.controlHeight }
                }
            }
        }

        Rectangle {
            width: parent.width
            height: applicationInformationContent.implicitHeight + Constants.spacing * 2
            color: Constants.surfaceColor
            border.color: Constants.borderColor
            Column { id: applicationInformationContent; anchors.fill: parent; anchors.margins: Constants.spacing; spacing: 6
                Text { text: qsTr("Application Information"); font: Constants.headingFont; color: Constants.textColor }
                Text { text: qsTr("OpenDSS Version: 2.0.0"); color: Constants.textColor; font: Constants.smallFont }
                Text { text: qsTr("Schema Versions: Run Summary v2, Sequence v2"); color: Constants.textColor; font: Constants.smallFont }
                Text { text: qsTr("Runtime Availability: Available"); color: Constants.textColor; font: Constants.smallFont }
                Text { text: qsTr("Camera Driver Availability: Available"); color: Constants.textColor; font: Constants.smallFont }
                Text { text: qsTr("DAQ Driver Availability: Available"); color: Constants.textColor; font: Constants.smallFont }
                Text { text: qsTr("GPU Environment Availability: Available"); color: Constants.textColor; font: Constants.smallFont }
            }
        }

        Rectangle {
            width: parent.width
            height: diagnosticsContent.implicitHeight + Constants.spacing * 2
            color: Constants.surfaceColor
            border.color: Constants.borderColor
            Column { id: diagnosticsContent; anchors.fill: parent; anchors.margins: Constants.spacing; spacing: 8
                Text { text: qsTr("Diagnostics"); font: Constants.headingFont; color: Constants.textColor }
                Text { text: qsTr("Diagnostic Folder: C:/Users/Scientist/AppData/Local/OpenDSS/Logs"); elide: Text.ElideMiddle; color: Constants.textColor; font: Constants.smallFont; width: parent.width }
                Button { id: openDiagnosticFolderButton; text: qsTr("Open Diagnostic Folder"); height: Constants.controlHeight }
            }
        }

        Rectangle {
            width: parent.width
            height: visualsContent.implicitHeight + Constants.spacing * 2
            color: Constants.surfaceColor
            border.color: Constants.borderColor
            Column { id: visualsContent; anchors.fill: parent; anchors.margins: Constants.spacing; spacing: 8
                Text { text: qsTr("Visuals"); font: Constants.headingFont; color: Constants.textColor }
                Row {
                    width: parent.width
                    spacing: Constants.spacing
                    Text { text: qsTr("Text Size"); width: 120; verticalAlignment: Text.AlignVCenter; color: Constants.textColor }
                    Slider { id: textSizeSlider; from: 80; to: 200; value: root.textSizePercent; width: parent.width - 190; height: Constants.controlHeight }
                    Text { text: qsTr("%1%").arg(root.textSizePercent); width: 50; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignRight; color: Constants.textColor }
                }
                Text { text: qsTr("80%–200%; default 100%"); color: Constants.mutedTextColor; font: Constants.smallFont }
            }
        }
    }

    states: [
        State { name: "settingsReady"; PropertyChanges { root.settingsPresentation: "ready"; root.defaultDataRoot: qsTr("C:/Users/Scientist/Documents/OpenDropletSortingSuite") } },
        State { name: "settingsLongDataRoot"; PropertyChanges { root.settingsPresentation: "ready"; root.defaultDataRoot: qsTr("C:/Users/Scientist/Documents/OpenDropletSortingSuite/Research/2026/July/Long-Experiment-Name/Run-Archive") } },
        State { name: "settingsError"; PropertyChanges { root.settingsPresentation: "error" } }
    ]
}
