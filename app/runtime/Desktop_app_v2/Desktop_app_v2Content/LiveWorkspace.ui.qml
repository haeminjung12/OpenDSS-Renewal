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
    property alias setupProfileHeadingButton: setupProfileSection.headingButton
    property alias runInformationHeadingButton: runInformationSection.headingButton
    property alias triggerTimingHeadingButton: triggerTimingSection.headingButton
    property alias outputRecordingHeadingButton: outputRecordingSection.headingButton
    property alias runningHeadingButton: runningSection.headingButton
    property alias runningHeadingEnabled: runningSection.headingEnabled
    property alias openProfileButton: openProfileButton
    property alias saveProfileButton: saveProfileButton
    property alias saveProfileAsButton: saveProfileAsButton
    property alias runNameField: runNameField
    property alias experimentTypeField: experimentTypeField
    property alias notesField: notesField
    property alias durationField: durationField
    property alias saveLocationField: saveLocationField
    property alias sendTestPulseButton: sendTestPulseButton
    property alias hitClassControl: hitClassControl
    property alias triggerEveryDropletControl: triggerEveryDropletControl
    property alias daqOutputControl: daqOutputControl
    property alias recordFullImageSequenceControl: recordFullImageSequenceControl
    property bool setupProfileExpanded: !setupLocked
    property bool runInformationExpanded: !setupLocked
    property bool triggerTimingExpanded: !setupLocked
    property bool outputRecordingExpanded: !setupLocked
    property bool runningExpanded: active || completed
    property string activeModelText: qsTr("Model-042")
    property string hitBoundaryText: qsTr("Hit boundary: calibrated outlet region")
    readonly property bool active: presentation === "running" || presentation === "paused"
    readonly property bool unavailable: presentation === "unavailable"
    readonly property bool completed: presentation === "completed"
    readonly property bool error: presentation === "error"
    readonly property bool setupLocked: active || completed

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

                    CollapsibleSection {
                        id: setupProfileSection
                        sectionTitle: qsTr("Setup Profile")
                        expanded: root.setupProfileExpanded
                        headingEnabled: !root.setupLocked
                        useIntrinsicBodyHeight: true

                        Item {
                            width: parent.width
                            height: setupProfileBody.implicitHeight + Constants.spacing * 2

                            Column {
                                id: setupProfileBody
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: Constants.spacing
                                spacing: Constants.spacing

                                Text { text: qsTr("Profile: Default Live Setup"); color: Constants.textColor; font: Constants.smallFont }
                                Row {
                                    spacing: Constants.spacing
                                    Button { id: openProfileButton; text: qsTr("Open Profile"); enabled: !root.setupLocked }
                                    Button { id: saveProfileButton; text: qsTr("Save Profile"); enabled: !root.setupLocked }
                                    Button { id: saveProfileAsButton; text: qsTr("Save Profile As"); enabled: !root.setupLocked }
                                }
                            }
                        }
                    }

                    CollapsibleSection {
                        id: runInformationSection
                        sectionTitle: qsTr("Run Information")
                        expanded: root.runInformationExpanded
                        headingEnabled: !root.setupLocked
                        useIntrinsicBodyHeight: true

                        Item {
                            width: parent.width
                            height: runInformationBody.implicitHeight + Constants.spacing * 2

                            Column {
                                id: runInformationBody
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: Constants.spacing
                                spacing: Constants.spacing

                                Text { text: qsTr("Run Name"); color: Constants.textColor; font: Constants.smallFont }
                                TextField { id: runNameField; width: parent.width; text: qsTr("Run-042"); readOnly: root.setupLocked; Accessible.name: qsTr("Run Name") }
                                Text { text: qsTr("Experiment Type"); color: Constants.textColor; font: Constants.smallFont }
                                TextField { id: experimentTypeField; width: parent.width; text: qsTr("Droplet sorting"); readOnly: root.setupLocked; Accessible.name: qsTr("Experiment Type") }
                                Text { text: qsTr("Notes"); color: Constants.textColor; font: Constants.smallFont }
                                TextField { id: notesField; width: parent.width; text: qsTr("Deterministic visual review run"); readOnly: root.setupLocked; Accessible.name: qsTr("Notes") }
                                Text { text: qsTr("Duration"); color: Constants.textColor; font: Constants.smallFont }
                                TextField { id: durationField; width: parent.width; text: ""; placeholderText: qsTr("Optional — continue until Stop"); readOnly: root.setupLocked; Accessible.name: qsTr("Duration") }
                                Text { text: qsTr("Save Location"); color: Constants.textColor; font: Constants.smallFont }
                                TextField { id: saveLocationField; width: parent.width; text: qsTr("C:/OpenDSS/Runs"); readOnly: root.setupLocked; Accessible.name: qsTr("Save Location") }
                            }
                        }
                    }

                    CollapsibleSection {
                        id: triggerTimingSection
                        sectionTitle: qsTr("Trigger & Timing")
                        expanded: root.triggerTimingExpanded
                        headingEnabled: !root.setupLocked
                        useIntrinsicBodyHeight: true

                        Item {
                            width: parent.width
                            height: triggerTimingBody.implicitHeight + Constants.spacing * 2

                            Column {
                                id: triggerTimingBody
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: Constants.spacing
                                spacing: Constants.spacing

                                Text { text: qsTr("Active Model: %1").arg(root.activeModelText); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Hit Class"); color: Constants.textColor; font: Constants.smallFont }
                                ComboBox {
                                    id: hitClassControl
                                    width: parent.width
                                    model: [qsTr("Class 0"), qsTr("Class 1"), qsTr("Class 2")]
                                    currentIndex: 1
                                    enabled: !root.setupLocked
                                    Accessible.name: qsTr("Hit Class")
                                }
                                CheckBox {
                                    id: triggerEveryDropletControl
                                    text: qsTr("Trigger Every Droplet")
                                    checked: false
                                    enabled: !root.setupLocked
                                }
                                CheckBox {
                                    id: daqOutputControl
                                    text: qsTr("DAQ Output")
                                    checked: true
                                    enabled: !root.setupLocked
                                }
                                Text { text: root.hitBoundaryText; color: Constants.mutedTextColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                                Button { id: sendTestPulseButton; text: qsTr("Send Test Pulse"); enabled: !root.setupLocked }
                            }
                        }
                    }

                    CollapsibleSection {
                        id: outputRecordingSection
                        sectionTitle: qsTr("Output & Recording")
                        expanded: root.outputRecordingExpanded
                        headingEnabled: !root.setupLocked
                        useIntrinsicBodyHeight: true

                        Item {
                            width: parent.width
                            height: outputRecordingBody.implicitHeight + Constants.spacing * 2

                            Column {
                                id: outputRecordingBody
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: Constants.spacing
                                spacing: Constants.spacing

                                CheckBox {
                                    id: recordFullImageSequenceControl
                                    text: qsTr("Record Full Image Sequence")
                                    checked: false
                                    enabled: !root.setupLocked
                                }
                                Text { text: qsTr("Each detected event records a Droplet Crop and factual Droplet Log row."); color: Constants.mutedTextColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            }
                        }
                    }

                    CollapsibleSection {
                        id: runningSection
                        sectionTitle: qsTr("Running")
                        expanded: root.runningExpanded
                        useIntrinsicBodyHeight: true

                        Item {
                            width: parent.width
                            height: runningBody.implicitHeight + Constants.spacing * 2

                            Column {
                                id: runningBody
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: Constants.spacing
                                spacing: Constants.spacing

                                Text { text: root.presentation === "paused" ? qsTr("Status: Paused") : (root.completed ? qsTr("Status: Completed") : qsTr("Status: Running")); color: Constants.textColor; font: Constants.headingFont }
                                Text { visible: root.completed; text: qsTr("Stop reason: User stopped"); color: Constants.textColor; font: Constants.smallFont }
                                Text { visible: root.completed; text: qsTr("Saved location: %1").arg("C:/OpenDSS/Runs/Run-042"); color: Constants.textColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                                Text { text: qsTr("Elapsed time: 00:02:18"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Total Droplets: 428"); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Predicted Class 0: 216  •  Predicted Class 1: 212"); color: Constants.textColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                                Text { text: qsTr("Decision Hit: 212  •  Decision Waste: 216"); color: Constants.textColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                                Text { text: qsTr("Observed Hit: 205  •  Observed Waste: 211  •  Unresolved: 12"); color: Constants.textColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                                Text { text: qsTr("Camera FPS: 61.3  •  Inference Time: 4.2 ms"); color: Constants.textColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                                Text { text: root.completed ? qsTr("Run finalized. Open Run Summary or start a new run from the camera action bar.") : (root.presentation === "paused" ? qsTr("Resume or Stop this Run from the camera action bar.") : qsTr("Pause or Stop this Run from the camera action bar.")); color: Constants.mutedTextColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            }
                        }
                    }
                }
            }
        }
    }
}
