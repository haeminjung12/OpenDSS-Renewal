/* This is a UI file (.ui.qml) intended for Qt Design Studio editing. */
import QtQuick
import QtQuick.Controls
import Desktop_app_v2

Item {
    id: root
    property string presentation: "ready"
    property bool cameraStreaming: false
    property bool startSortingEnabled: false
    property alias primaryActionButton: primaryActionButton
    property alias secondaryActionButton: secondaryActionButton
    readonly property bool active: presentation === "running" || presentation === "paused"
    readonly property bool unavailable: presentation === "unavailable"
    readonly property bool completed: presentation === "completed"
    readonly property bool error: presentation === "error"

    Rectangle {
        anchors.fill: parent
        color: Constants.backgroundColor

        Text {
            id: workspaceTitle
            text: qsTr("Live")
            font: Constants.largeFont
            color: Constants.textColor
            height: Constants.controlHeight
            verticalAlignment: Text.AlignVCenter
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: Constants.workspaceMargin
        }

        Row {
            anchors.top: workspaceTitle.bottom
            anchors.topMargin: Constants.spacing
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: Constants.workspaceMargin
            spacing: Constants.workspaceMargin

            Rectangle {
                width: parent.width - Constants.operationPanelWidth - parent.spacing
                height: parent.height
                color: Constants.viewerColor
                border.color: Constants.borderColor

                Text {
                    anchors.centerIn: parent
                    text: root.unavailable ? qsTr("Camera unavailable") : qsTr("Camera preview")
                    color: "#ffffff"
                    font: Constants.largeFont
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: Constants.spacing * 2
                    spacing: Constants.spacing

                    Button {
                        id: primaryActionButton
                        text: root.active ? (root.presentation === "paused" ? qsTr("Resume") : qsTr("Pause")) : (root.completed ? qsTr("Start New Run") : (root.presentation === "ready" && root.cameraStreaming ? qsTr("Stop Camera") : qsTr("Start Camera")))
                        enabled: !root.unavailable && !root.error
                    }
                    Button {
                        id: secondaryActionButton
                        text: root.active ? qsTr("Stop") : (root.completed ? qsTr("Open Run Summary") : qsTr("Start Sorting"))
                        enabled: root.active || root.completed || (root.presentation === "ready" && root.startSortingEnabled)
                    }
                }

                Rectangle {
                    visible: root.error
                    anchors.centerIn: parent
                    width: 180
                    height: 56
                    color: Constants.errorSurfaceColor
                    border.color: Constants.faultColor
                    Text { anchors.centerIn: parent; text: qsTr("Error"); color: Constants.faultColor; font: Constants.headingFont }
                }
            }

            ScrollView {
                width: Constants.operationPanelWidth
                height: parent.height
                clip: true
                contentWidth: availableWidth

                Column {
                    width: parent.width
                    spacing: Constants.spacing

                    CollapsibleSection { sectionTitle: qsTr("Setup Profile"); expanded: !root.active; headingEnabled: !root.active; bodyText: qsTr("Profile") }
                    CollapsibleSection { sectionTitle: qsTr("Run Information"); expanded: !root.active; headingEnabled: !root.active; bodyText: qsTr("Run") }
                    CollapsibleSection { sectionTitle: qsTr("Trigger & Timing"); expanded: !root.active; headingEnabled: !root.active; bodyText: qsTr("Active Model") }
                    CollapsibleSection { sectionTitle: qsTr("Output & Recording"); expanded: !root.active; headingEnabled: !root.active; bodyText: qsTr("Output") }
                    CollapsibleSection {
                        sectionTitle: qsTr("Running")
                        expanded: root.active
                        bodyText: root.active ? (root.presentation === "paused" ? qsTr("Paused") : qsTr("Running")) : (root.completed ? qsTr("Completed") : qsTr("Collapsed"))
                    }
                }
            }
        }
    }
}
