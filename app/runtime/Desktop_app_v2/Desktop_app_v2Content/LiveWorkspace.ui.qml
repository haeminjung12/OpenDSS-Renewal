/* This is a UI file (.ui.qml) intended for Qt Design Studio editing. */
import QtQuick
import QtQuick.Controls
import Desktop_app_v2

Item {
    id: root
    property string presentation: "ready"
    property bool cameraStreaming: false
    property url cameraPreviewSource: ""
    property bool startSortingEnabled: false
    property bool rightPanelExpanded: true
    property bool hitBoundaryDefined: false
    property real hitBoundaryXRatio: 0.0
    property real hitBoundaryYRatio: 0.5
    property string hitBoundarySide: "bottom"
    property bool hitBoundaryEditable: true
    property bool hitBoundaryPlacementArmed: false
    property bool smallDropletSelectionArmed: false
    property bool smallDropletSelectionVisible: false
    property real smallDropletSelectionStartXRatio: 0.0
    property real smallDropletSelectionStartYRatio: 0.0
    property real smallDropletSelectionEndXRatio: 0.0
    property real smallDropletSelectionEndYRatio: 0.0
    property string serviceDiagnosticText: qsTr("Resolve the current error before continuing.")
    property string runArtifactPath: qsTr("C:/OpenDSS/Runs/Run-042")
    property string elapsedTimeText: qsTr("00:02:18")
    property string persistedEventCountText: qsTr("428")
    property string rejectedCountText: qsTr("0")
    property string integrityStatusText: ""
    property string finalOutcomeText: qsTr("User stopped")
    property string outputStatusText: qsTr("Each detected event records a Droplet Crop and factual Droplet Log row.")
    property string profileAvailabilityText: ""
    property string runSummaryText: root.integrityStatusText !== "" ? root.integrityStatusText : (root.completed ? qsTr("Run finalized. Open Run Summary or start a new run from the footer.") : (root.presentation === "paused" ? qsTr("Resume or Stop this Run from the footer.") : qsTr("Pause or Stop this Run from the footer.")))
    property alias primaryActionButton: primaryActionButton
    property alias secondaryActionButton: secondaryActionButton
    property alias rightPanelToggleButton: rightPanelToggleButton
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
    property alias smallDropletSelectionInputArea: smallDropletSelectionInputArea
    property alias hitClassControl: hitClassControl
    property alias triggerEveryDropletControl: triggerEveryDropletControl
    property alias daqOutputControl: daqOutputControl
    property alias recordFullImageSequenceControl: recordFullImageSequenceControl
    property alias cameraPreviewImage: cameraPreviewViewer.image
    property alias hitBoundaryInputArea: hitBoundaryInputArea
    property alias setDecisionBoundaryButton: setDecisionBoundaryButton
    property alias resetDecisionBoundaryButton: resetDecisionBoundaryButton
    property alias topIsHitControl: topIsHitControl
    property alias bottomIsHitControl: bottomIsHitControl
    property bool setupProfileExpanded: !setupLocked
    property bool runInformationExpanded: !setupLocked
    property bool triggerTimingExpanded: !completed
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

        SplitView {
            font: Constants.font
            anchors.top: workspaceTitle.bottom
            anchors.topMargin: Constants.spacing
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: Constants.workspaceMargin

            Rectangle {
                SplitView.fillWidth: true
                color: Constants.viewerColor
                border.color: Constants.borderColor

                FullSizeImageViewer {
                    id: cameraPreviewViewer
                    anchors.fill: parent
                    source: root.cameraPreviewSource
                    placeholderText: root.unavailable
                                     ? qsTr("Camera unavailable")
                                     : qsTr("Camera preview")
                    image.visible: root.cameraPreviewSource.toString() !== ""
                                   && !root.unavailable
                    placeholder.visible: !cameraPreviewViewer.image.visible

                    Item {
                        id: hitBoundaryOverlay
                        anchors.fill: parent
                        visible: cameraPreviewViewer.image.visible
                        readonly property real boundaryX: Math.max(0, Math.min(1, root.hitBoundaryXRatio)) * width
                        readonly property real boundaryY: Math.max(0, Math.min(1, root.hitBoundaryYRatio)) * height

                        MouseArea {
                            id: hitBoundaryInputArea
                            anchors.fill: parent
                            enabled: root.hitBoundaryEditable
                                     && root.hitBoundaryPlacementArmed
                                     && !root.smallDropletSelectionArmed
                                     && cameraPreviewViewer.image.visible
                        }

                        Rectangle {
                            visible: root.hitBoundaryDefined
                            x: hitBoundaryOverlay.boundaryX
                            y: hitBoundaryOverlay.boundaryY - height / 2
                            width: hitBoundaryOverlay.width - hitBoundaryOverlay.boundaryX
                            height: 4
                            color: Constants.textColor
                            Accessible.ignored: true

                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width
                                height: 2
                                color: Constants.surfaceColor
                                Accessible.ignored: true
                            }
                        }

                        Rectangle {
                            visible: root.hitBoundaryDefined
                            x: hitBoundaryOverlay.boundaryX - width / 2
                            y: hitBoundaryOverlay.boundaryY - height / 2
                            width: 12
                            height: 12
                            radius: width / 2
                            color: Constants.surfaceColor
                            border.color: Constants.textColor
                            border.width: 2
                            Accessible.ignored: true
                        }
                    }

                    Item {
                        id: smallDropletSelectionOverlay
                        anchors.fill: parent
                        visible: cameraPreviewViewer.image.visible

                        Rectangle {
                            visible: root.smallDropletSelectionVisible
                            x: Math.min(root.smallDropletSelectionStartXRatio,
                                        root.smallDropletSelectionEndXRatio) * parent.width
                            y: Math.min(root.smallDropletSelectionStartYRatio,
                                        root.smallDropletSelectionEndYRatio) * parent.height
                            width: Math.abs(root.smallDropletSelectionEndXRatio
                                            - root.smallDropletSelectionStartXRatio) * parent.width
                            height: Math.abs(root.smallDropletSelectionEndYRatio
                                             - root.smallDropletSelectionStartYRatio) * parent.height
                            color: "transparent"
                            border.color: Constants.accentColor
                            border.width: 2
                            Accessible.ignored: true
                        }

                        MouseArea {
                            id: smallDropletSelectionInputArea
                            anchors.fill: parent
                            enabled: root.smallDropletSelectionArmed
                                     && cameraPreviewViewer.image.visible
                            cursorShape: Qt.CrossCursor
                        }
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
                        text: qsTr("Live")
                        visible: root.rightPanelExpanded
                        font: Constants.headingFont
                        color: Constants.textColor
                        anchors.left: parent.left
                        anchors.leftMargin: Constants.spacing
                        anchors.right: parent.right
                        anchors.rightMargin: rightPanelToggleButton.width + Constants.spacing
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
                    Accessible.name: root.rightPanelExpanded ? qsTr("Collapse Live panel") : qsTr("Expand Live panel")
                }

                ScrollView {
                    id: rightPanelScroll
                    visible: root.rightPanelExpanded
                    anchors.top: panelTopStrip.bottom
                    anchors.bottom: actionFooter.top
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
                        id: setupProfileSection
                        width: rightPanelScroll.availableWidth
                        sectionTitle: qsTr("Setup Profile")
                        expanded: root.setupProfileExpanded
                        headingEnabled: !root.completed
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
                                    AppButton { id: openProfileButton; text: qsTr("Open Profile"); enabled: !root.setupLocked; height: Constants.appStandardControlHeight }
                                    AppButton { id: saveProfileButton; text: qsTr("Save Profile"); enabled: !root.setupLocked; height: Constants.appStandardControlHeight }
                                    AppButton { id: saveProfileAsButton; text: qsTr("Save Profile As"); enabled: !root.setupLocked; height: Constants.appStandardControlHeight }
                                }
                                Text { text: root.profileAvailabilityText; color: Constants.warningColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            }
                        }
                    }

                        AppAccordion {
                        id: runInformationSection
                        width: rightPanelScroll.availableWidth
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

                                Text { text: qsTr("Run Name"); color: Constants.textColor; font: Constants.font }
                                AppTextField { id: runNameField; width: parent.width; height: Constants.appStandardControlHeight; text: qsTr("Run-042"); readOnly: root.setupLocked; Accessible.name: qsTr("Run Name") }
                                Text { text: qsTr("Experiment Type"); color: Constants.textColor; font: Constants.font }
                                AppTextField { id: experimentTypeField; width: parent.width; height: Constants.appStandardControlHeight; text: qsTr("Droplet sorting"); readOnly: root.setupLocked; Accessible.name: qsTr("Experiment Type") }
                                Text { text: qsTr("Notes"); color: Constants.textColor; font: Constants.font }
                                AppTextField { id: notesField; width: parent.width; height: Constants.appStandardControlHeight; text: qsTr("Deterministic visual review run"); readOnly: root.setupLocked; Accessible.name: qsTr("Notes") }
                                Text { text: qsTr("Duration"); color: Constants.textColor; font: Constants.font }
                                AppTextField { id: durationField; width: parent.width; height: Constants.appStandardControlHeight; text: ""; placeholderText: qsTr("Optional — continue until Stop"); readOnly: root.setupLocked; Accessible.name: qsTr("Duration") }
                                Text { text: qsTr("Save Location"); color: Constants.textColor; font: Constants.font }
                                AppTextField { id: saveLocationField; width: parent.width; height: Constants.appStandardControlHeight; text: qsTr("C:/OpenDSS/Runs"); readOnly: root.setupLocked; Accessible.name: qsTr("Save Location") }
                            }
                        }
                    }

                        AppAccordion {
                        id: triggerTimingSection
                        width: rightPanelScroll.availableWidth
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
                                Text { text: qsTr("Hit Class"); color: Constants.textColor; font: Constants.font }
                                AppComboBox {
                                    id: hitClassControl
                                    width: parent.width
                                    height: Constants.appStandardControlHeight
                                    model: [qsTr("Class 0"), qsTr("Class 1"), qsTr("Class 2")]
                                    currentIndex: 1
                                    enabled: !root.completed
                                    Accessible.name: qsTr("Hit Class")
                                }
                                AppSwitch {
                                    id: triggerEveryDropletControl
                                    text: qsTr("Trigger Every Droplet")
                                    checked: false
                                    enabled: !root.completed
                                }
                                AppSwitch {
                                    id: daqOutputControl
                                    text: qsTr("Physical DAQ Output")
                                    checked: true
                                    enabled: !root.completed
                                }
                                Text { text: qsTr("Decision Boundary"); color: Constants.textColor; font: Constants.font }
                                Row {
                                    spacing: Constants.spacing
                                    AppButton {
                                        id: setDecisionBoundaryButton
                                        text: qsTr("Set Decision Boundary")
                                        enabled: root.hitBoundaryEditable
                                        height: Constants.appStandardControlHeight
                                    }
                                    AppButton {
                                        id: resetDecisionBoundaryButton
                                        text: qsTr("Reset")
                                        enabled: root.hitBoundaryEditable
                                        height: Constants.appStandardControlHeight
                                    }
                                }
                                Row {
                                    spacing: Constants.spacing
                                    AppRadioButton {
                                        id: topIsHitControl
                                        text: qsTr("Top is Hit")
                                        checked: root.hitBoundarySide === "top"
                                        enabled: root.hitBoundaryEditable
                                    }
                                    AppRadioButton {
                                        id: bottomIsHitControl
                                        text: qsTr("Bottom is Hit")
                                        checked: root.hitBoundarySide === "bottom"
                                        enabled: root.hitBoundaryEditable
                                    }
                                }
                                Text {
                                    text: root.hitBoundarySide === "top" ? qsTr("Hit: −Y ↑   Waste: +Y ↓") : qsTr("Hit: +Y ↓   Waste: −Y ↑")
                                    color: Constants.mutedTextColor
                                    font: Constants.smallFont
                                    wrapMode: Text.WordWrap
                                    width: parent.width
                                }
                                AppButton { id: sendTestPulseButton; text: qsTr("Send Test Pulse"); enabled: !root.setupLocked; height: Constants.appStandardControlHeight }
                            }
                        }
                    }

                        AppAccordion {
                        id: outputRecordingSection
                        width: rightPanelScroll.availableWidth
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

                                AppCheckBox {
                                    id: recordFullImageSequenceControl
                                    text: qsTr("Record Full Image Sequence")
                                    checked: false
                                    enabled: !root.setupLocked
                                }
                                Text { text: root.outputStatusText; color: Constants.mutedTextColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            }
                        }
                    }

                        AppAccordion {
                        id: runningSection
                        width: rightPanelScroll.availableWidth
                        visible: root.active || root.completed
                        sectionTitle: qsTr("Run Status")
                        expanded: true
                        headingButton.visible: false
                        headingButton.height: 0
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
                                Text { visible: root.completed; text: qsTr("Stop reason: %1").arg(root.finalOutcomeText); color: Constants.textColor; font: Constants.smallFont }
                                Text { visible: root.completed; text: qsTr("Saved location: %1").arg(root.runArtifactPath); color: Constants.textColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                                Text { text: qsTr("Elapsed time: %1").arg(root.elapsedTimeText); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Total Droplets: %1").arg(root.persistedEventCountText); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: qsTr("Rejected: %1").arg(root.rejectedCountText); color: Constants.textColor; font: Constants.smallFont }
                                Text { text: root.runSummaryText; color: Constants.mutedTextColor; font: Constants.smallFont; wrapMode: Text.WordWrap; width: parent.width }
                            }
                        }
                        }
                    }
                }

                Rectangle {
                    id: actionFooter
                    visible: root.rightPanelExpanded
                    height: Constants.controlHeight * 2 + Constants.spacing * 3
                    color: Constants.backgroundColor
                    border.color: Constants.borderColor
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom

                    Text {
                        text: root.unavailable ? qsTr("Camera unavailable — restore Configuration to continue.") : (root.error ? root.serviceDiagnosticText : (root.active ? (root.presentation === "paused" ? qsTr("Run paused.") : qsTr("Run in progress.")) : (root.completed ? qsTr("Run complete.") : (root.cameraStreaming ? (root.startSortingEnabled ? qsTr("Ready to start sorting.") : qsTr("Camera streaming — sorting is not ready.")) : qsTr("Start Camera to check sorting readiness.")))))
                        color: root.unavailable || root.error ? Constants.warningColor : Constants.textColor
                        font: Constants.smallFont
                        elide: Text.ElideRight
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: Constants.spacing
                    }

                    Row {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: Constants.spacing
                        spacing: Constants.spacing

                        AppButton {
                            id: primaryActionButton
                            visible: root.active || root.completed || root.presentation === "ready"
                            width: secondaryActionButton.visible ? (parent.width - parent.spacing) / 2 : parent.width
                            height: Constants.appPrimaryButtonHeight
                            text: root.active ? (root.presentation === "paused" ? qsTr("Resume") : qsTr("Pause")) : (root.completed ? qsTr("Start New Run") : (root.cameraStreaming ? qsTr("Stop Camera") : qsTr("Start Camera")))
                            enabled: !root.unavailable && !root.error
                            visualRole: "primary"
                        }
                        AppButton {
                            id: secondaryActionButton
                            visible: root.active || root.completed || root.presentation === "ready"
                            width: primaryActionButton.visible ? (parent.width - parent.spacing) / 2 : parent.width
                            height: Constants.appPrimaryButtonHeight
                            text: root.active ? qsTr("Stop") : (root.completed ? qsTr("Open Run Summary") : qsTr("Start Sorting"))
                            enabled: root.active || root.completed || (root.presentation === "ready" && root.cameraStreaming && root.startSortingEnabled)
                            visualRole: root.active ? "destructive" : "primary"
                        }
                    }
                }
            }
        }
    }
}
