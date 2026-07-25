/* This is a UI file (.ui.qml) intended for Qt Design Studio editing. */
import QtQuick
import QtQuick.Controls
import Desktop_app_v2

Item {
    id: root
    property string presentation: "empty"
    property string activeModelText: qsTr("No Active Model")
    property bool sequenceTestExpanded: true
    property bool rightPanelExpanded: true
    property alias loadSequenceButton: loadSequenceButton
    property alias loadToMemoryButton: loadToMemoryButton
    property alias startStopButton: startStopButton
    property alias physicalDaqOutputControl: physicalDaqOutputControl
    property alias sequenceTestHeadingButton: sequenceTestSection.headingButton
    property alias rightPanelToggleButton: rightPanelToggleButton
    readonly property bool loaded: presentation === "ready" || presentation === "running" || presentation === "completed"
    readonly property bool running: presentation === "running"
    readonly property bool unavailable: presentation === "unavailable"
    readonly property bool error: presentation === "error"

    Rectangle {
        anchors.fill: parent
        color: Constants.backgroundColor

        Text {
            id: workspaceTitle
            text: qsTr("Sequence Test")
            font: Constants.largeFont
            color: Constants.textColor
            height: Constants.controlHeight
            verticalAlignment: Text.AlignVCenter
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: Constants.workspaceMargin
        }

        SplitView {
            font: Constants.font
            anchors.top: workspaceTitle.bottom
            anchors.topMargin: Constants.spacing
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: Constants.workspaceMargin

            SplitView {
                orientation: Qt.Vertical
                SplitView.fillWidth: true

                Rectangle {
                    SplitView.fillWidth: true
                    SplitView.preferredHeight: parent.height * 0.5
                    SplitView.minimumHeight: Math.round(180 * Constants.textScale)
                    color: Constants.viewerColor
                    border.color: Constants.borderColor
                    Text { anchors.centerIn: parent; text: root.loaded ? qsTr("First-frame preview") : qsTr("No Image Sequence selected\nLoad a Sequence to begin."); horizontalAlignment: Text.AlignHCenter; color: Constants.surfaceColor; font: Constants.headingFont }
                }

                Rectangle {
                    SplitView.fillWidth: true
                    SplitView.fillHeight: true
                    SplitView.minimumHeight: Math.round(160 * Constants.textScale)
                    color: Constants.surfaceColor
                    border.color: Constants.borderColor
                    Column {
                        anchors.fill: parent
                        anchors.margins: Constants.spacing * 2
                        spacing: Constants.spacing
                        Text { text: qsTr("Results"); color: Constants.textColor; font: Constants.headingFont }
                        Text { visible: !root.error; text: root.unavailable ? qsTr("Unavailable") : (root.running ? qsTr("Processing") : (root.presentation === "completed" ? qsTr("Completed") : qsTr("No results"))); color: Constants.mutedTextColor; font: Constants.smallFont }
                        Rectangle {
                            visible: root.error
                            width: parent.width
                            height: Constants.controlHeight
                            color: Constants.errorSurfaceColor
                            border.color: Constants.faultColor
                            Text { anchors.centerIn: parent; text: qsTr("Error"); color: Constants.faultColor; font: Constants.headingFont }
                        }
                    }
                }
            }

            Rectangle {
                id: rightPanel
                SplitView.preferredWidth: Constants.operationPanelWidth
                SplitView.minimumWidth: Constants.collapsedOperationPanelWidth
                SplitView.maximumWidth: root.rightPanelExpanded ? Math.max(Constants.collapsedOperationPanelWidth, parent.width * 0.75) : Constants.collapsedOperationPanelWidth
                color: Constants.surfaceColor
                border.color: Constants.borderColor

                Rectangle {
                    id: panelTopStrip
                    height: Constants.controlHeight
                    color: Constants.backgroundColor
                    border.color: Constants.borderColor
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    Text {
                        text: qsTr("Sequence Test")
                        visible: root.rightPanelExpanded
                        font: Constants.headingFont
                        color: Constants.textColor
                        anchors.left: parent.left
                        anchors.leftMargin: Constants.spacing
                        anchors.right: rightPanelToggleButton.left
                        anchors.rightMargin: Constants.spacing
                        anchors.verticalCenter: parent.verticalCenter
                        elide: Text.ElideRight
                    }
                }
                AppInspectorRail {
                    id: rightPanelToggleButton
                    text: root.rightPanelExpanded ? "›" : "‹"
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    z: 1
                    Accessible.name: root.rightPanelExpanded ? qsTr("Collapse Sequence Test panel") : qsTr("Expand Sequence Test panel")
                }

                ScrollView {
                    id: rightPanelScroll
                    visible: root.rightPanelExpanded
                    anchors.top: panelTopStrip.bottom
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: Constants.spacing
                    clip: true
                    contentWidth: availableWidth

                    Column {
                        width: rightPanelScroll.availableWidth
                        height: implicitHeight
                        spacing: Constants.spacing

                        AppAccordion {
                        id: sequenceTestSection
                        width: rightPanelScroll.availableWidth
                        sectionTitle: qsTr("Sequence Test")
                        expanded: root.sequenceTestExpanded
                        useIntrinsicBodyHeight: true
                        Item {
                            width: parent.width
                            height: sequenceTestContent.implicitHeight + Constants.spacing * 2
                            Column {
                                id: sequenceTestContent
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.margins: Constants.spacing
                                spacing: Constants.spacing
                                Text { text: qsTr("Active Model: %1").arg(root.activeModelText); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: root.loaded ? qsTr("Sequence loaded") : (root.presentation === "selected" ? qsTr("Sequence selected") : qsTr("No sequence selected")); color: Constants.mutedTextColor; font: Constants.smallFont }
                                Text { text: root.running ? qsTr("Processing progress") : qsTr("Load status"); color: Constants.mutedTextColor; font: Constants.smallFont }
                                Row {
                                    width: parent.width
                                    spacing: Constants.spacing
                                    AppButton { id: loadSequenceButton; text: qsTr("Load Sequence"); enabled: !root.running && !root.error; width: (parent.width - parent.spacing) / 2; height: Constants.appStandardControlHeight }
                                    AppButton { id: loadToMemoryButton; text: qsTr("Load to Memory"); enabled: root.presentation === "selected"; width: (parent.width - parent.spacing) / 2; height: Constants.appStandardControlHeight }
                                }
                                AppCheckBox { id: physicalDaqOutputControl; text: qsTr("Physical DAQ Output"); enabled: !root.running }
                                Text {
                                    visible: !root.running && root.presentation !== "ready"
                                    text: root.activeModelText === qsTr("No Active Model") ? qsTr("Start requires an Active Model.") : (root.presentation === "selected" ? qsTr("Load the selected Sequence to memory before Start.") : (root.error ? qsTr("Resolve the current Error before Start.") : qsTr("Load a Sequence before Start.")))
                                    color: Constants.warningColor
                                    font: Constants.smallFont
                                    wrapMode: Text.WordWrap
                                    width: parent.width
                                }
                                AppButton { id: startStopButton; text: root.running ? qsTr("Stop") : qsTr("Start Sequence Test"); visualRole: root.running ? "destructive" : "primary"; enabled: root.running || root.presentation === "ready"; height: Constants.appPrimaryButtonHeight }
                            }
                        }
                        }
                    }
                }
            }
        }
    }
}
