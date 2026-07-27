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
    property bool hitBoundaryDefined: false
    property real hitBoundaryXRatio: 0.0
    property real hitBoundaryYRatio: 0.5
    property string hitBoundarySide: "top"
    property bool hitBoundaryEditable: true
    property bool hitBoundaryPlacementArmed: false
    property bool smallDropletSelectionArmed: false
    property bool smallDropletSelectionVisible: false
    property real smallDropletSelectionStartXRatio: 0.0
    property real smallDropletSelectionStartYRatio: 0.0
    property real smallDropletSelectionEndXRatio: 0.0
    property real smallDropletSelectionEndYRatio: 0.0
    property alias loadSequenceButton: loadSequenceButton
    property alias loadToMemoryButton: loadToMemoryButton
    property alias startStopButton: startStopButton
    property alias physicalDaqOutputControl: physicalDaqOutputControl
    property alias sequencePreviewHost: sequencePreviewHost
    property alias sequencePreviewImage: sequencePreviewViewer.image
    property alias sequencePreviewPlaceholder: sequencePreviewViewer.placeholder
    property alias hitBoundaryInputArea: hitBoundaryInputArea
    property alias smallDropletSelectionInputArea: smallDropletSelectionInputArea
    property alias setDecisionBoundaryButton: setDecisionBoundaryButton
    property alias resetDecisionBoundaryButton: resetDecisionBoundaryButton
    property alias topIsHitControl: topIsHitControl
    property alias bottomIsHitControl: bottomIsHitControl
    property alias sequenceNameField: sequenceNameField
    property alias sequencePathField: sequencePathField
    property alias frameCountText: frameCountText
    property alias recordedFpsText: recordedFpsText
    property alias sequenceValidationText: sequenceValidationText
    property alias processingFpsField: processingFpsField
    property alias achievedFpsText: achievedFpsText
    property alias outputStatusText: outputStatusText
    property alias availableMemoryText: availableMemoryText
    property alias bufferSizeText: bufferSizeText
    property alias loadReadinessText: loadReadinessText
    property alias loadStatusText: loadStatusText
    property alias triggerEveryDropletControl: triggerEveryDropletControl
    property alias hitClassControl: hitClassControl
    property alias saveLocationField: saveLocationField
    property alias browseSaveLocationButton: browseSaveLocationButton
    property alias sequenceTestHeadingButton: sequenceTestSection.headingButton
    property alias rightPanelToggleButton: rightPanelToggleButton
    property string runLocationText: ""
    property bool hasRunSummary: false
    property bool hasRunFolder: false
    property alias openRunSummaryButton: openRunSummaryButton
    property alias openRunFolderButton: openRunFolderButton
    property alias startAnotherTestButton: startAnotherTestButton
    readonly property bool loaded: presentation === "ready" || presentation === "running" || presentation === "completed" || presentation === "interrupted"
    readonly property bool running: presentation === "running"
    readonly property bool unavailable: presentation === "unavailable"
    readonly property bool error: presentation === "error"
    readonly property bool postOperation: presentation === "completed" || presentation === "interrupted" || presentation === "error"

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
                    id: sequencePreviewHost
                    SplitView.fillWidth: true
                    SplitView.preferredHeight: parent.height * 0.5
                    SplitView.minimumHeight: Math.round(180 * Constants.textScale)
                    color: Constants.viewerColor
                    border.color: Constants.borderColor

                    FullSizeImageViewer {
                        id: sequencePreviewViewer
                        anchors.fill: parent
                        placeholderText: root.loaded
                                         ? qsTr("First-frame preview")
                                         : qsTr("No Image Sequence selected\nLoad a Sequence to begin.")
                        image.visible: sequencePreviewViewer.image.source.toString() !== ""
                        placeholder.visible: !sequencePreviewViewer.image.visible

                        Item {
                            id: hitBoundaryOverlay
                            anchors.fill: parent
                            visible: sequencePreviewViewer.image.visible
                            readonly property real boundaryX: Math.max(0, Math.min(1, root.hitBoundaryXRatio)) * width
                            readonly property real boundaryY: Math.max(0, Math.min(1, root.hitBoundaryYRatio)) * height

                            MouseArea {
                                id: hitBoundaryInputArea
                                anchors.fill: parent
                                enabled: root.hitBoundaryEditable
                                         && root.hitBoundaryPlacementArmed
                                         && !root.smallDropletSelectionArmed
                                         && sequencePreviewViewer.image.visible
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
                            visible: sequencePreviewViewer.image.visible

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
                                         && sequencePreviewViewer.image.visible
                                cursorShape: Qt.CrossCursor
                            }
                        }
                    }
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
                                Text {
                                    text: root.hitBoundarySide === "top" ? qsTr("Hit: −Y ↑   Waste: +Y ↓") : qsTr("Hit: +Y ↓   Waste: −Y ↑")
                                    color: Constants.mutedTextColor
                                    font: Constants.smallFont
                                    wrapMode: Text.WordWrap
                                    width: parent.width
                                }
                                Text { text: qsTr("Image Sequence"); color: Constants.textColor; font: Constants.headingFont }
                                AppTextField {
                                    id: sequenceNameField
                                    width: parent.width
                                    height: Constants.appStandardControlHeight
                                    text: qsTr("No sequence selected")
                                    readOnly: true
                                    Accessible.name: qsTr("Selected sequence name")
                                }
                                AppTextField {
                                    id: sequencePathField
                                    width: parent.width
                                    height: Constants.appStandardControlHeight
                                    text: ""
                                    placeholderText: qsTr("No source path")
                                    readOnly: true
                                    Accessible.name: qsTr("Selected sequence path")
                                }
                                Row {
                                    width: parent.width
                                    spacing: Constants.spacing
                                    Text { id: frameCountText; text: qsTr("Frames: —"); color: Constants.mutedTextColor; font: Constants.smallFont; width: (parent.width - parent.spacing) / 2; wrapMode: Text.WordWrap }
                                    Text { id: recordedFpsText; text: qsTr("Recorded FPS: —"); color: Constants.mutedTextColor; font: Constants.smallFont; width: (parent.width - parent.spacing) / 2; wrapMode: Text.WordWrap }
                                }
                                Text { id: sequenceValidationText; text: qsTr("Status: Not selected"); color: Constants.mutedTextColor; font: Constants.smallFont; width: parent.width; wrapMode: Text.WordWrap }
                                Row {
                                    width: parent.width
                                    spacing: Constants.spacing
                                    AppButton { id: loadSequenceButton; text: qsTr("Load Sequence"); enabled: !root.running; width: (parent.width - parent.spacing) / 2; height: Constants.appStandardControlHeight }
                                    AppButton { id: loadToMemoryButton; text: qsTr("Load to Memory"); enabled: root.presentation === "selected"; width: (parent.width - parent.spacing) / 2; height: Constants.appStandardControlHeight }
                                }
                                Text { text: qsTr("Processing FPS"); color: Constants.textColor; font: Constants.font }
                                AppTextField {
                                    id: processingFpsField
                                    width: parent.width
                                    height: Constants.appStandardControlHeight
                                    text: ""
                                    placeholderText: qsTr("Defaults to recorded FPS")
                                    readOnly: root.running
                                    Accessible.name: qsTr("Processing FPS")
                                }
                                Row {
                                    width: parent.width
                                    spacing: Constants.spacing
                                    Text { id: achievedFpsText; text: qsTr("Achieved FPS: —"); color: Constants.mutedTextColor; font: Constants.smallFont; width: (parent.width - parent.spacing) / 2; wrapMode: Text.WordWrap }
                                    Text { id: outputStatusText; text: qsTr("Output status: Idle"); color: Constants.mutedTextColor; font: Constants.smallFont; width: (parent.width - parent.spacing) / 2; wrapMode: Text.WordWrap }
                                }
                                Text { id: availableMemoryText; text: qsTr("Available memory: —"); color: Constants.mutedTextColor; font: Constants.smallFont; width: parent.width; wrapMode: Text.WordWrap }
                                Text { id: bufferSizeText; text: qsTr("Buffer size: —"); color: Constants.mutedTextColor; font: Constants.smallFont; width: parent.width; wrapMode: Text.WordWrap }
                                Text { id: loadReadinessText; text: qsTr("Load readiness: Select a sequence"); color: Constants.mutedTextColor; font: Constants.smallFont; width: parent.width; wrapMode: Text.WordWrap }
                                Text { id: loadStatusText; text: root.running ? qsTr("Load status: Processing") : qsTr("Load status: Not loaded"); color: Constants.mutedTextColor; font: Constants.smallFont; width: parent.width; wrapMode: Text.WordWrap }
                                Text { text: qsTr("Routing"); color: Constants.textColor; font: Constants.headingFont }
                                AppSwitch {
                                    id: triggerEveryDropletControl
                                    text: qsTr("Trigger Every Droplet")
                                    checked: false
                                    enabled: !root.running
                                }
                                Text { text: qsTr("Class-Based / Hit Class"); color: Constants.textColor; font: Constants.font }
                                AppComboBox {
                                    id: hitClassControl
                                    width: parent.width
                                    height: Constants.appStandardControlHeight
                                    model: [qsTr("Select Hit Class")]
                                    enabled: !root.running && !triggerEveryDropletControl.checked
                                    Accessible.name: qsTr("Hit Class")
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
                                Text { text: qsTr("Save Location"); color: Constants.textColor; font: Constants.font }
                                Row {
                                    width: parent.width
                                    spacing: Constants.spacing
                                    AppTextField {
                                        id: saveLocationField
                                        width: parent.width - browseSaveLocationButton.width - parent.spacing
                                        height: Constants.appStandardControlHeight
                                        text: ""
                                        placeholderText: qsTr("Select save location")
                                        readOnly: true
                                        Accessible.name: qsTr("Save Location")
                                    }
                                    AppButton {
                                        id: browseSaveLocationButton
                                        text: qsTr("Browse")
                                        height: Constants.appStandardControlHeight
                                        enabled: !root.running
                                    }
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
                                Text {
                                    visible: root.postOperation && root.runLocationText !== ""
                                    text: qsTr("Location: %1").arg(root.runLocationText)
                                    color: Constants.mutedTextColor
                                    font: Constants.smallFont
                                    width: parent.width
                                    elide: Text.ElideMiddle
                                }
                                Row {
                                    visible: root.postOperation && (root.hasRunSummary || root.hasRunFolder)
                                    width: parent.width
                                    spacing: Constants.spacing
                                    AppButton { id: openRunSummaryButton; text: qsTr("Open Run Summary"); visible: root.hasRunSummary; enabled: visible; width: root.hasRunFolder ? (parent.width - parent.spacing) / 2 : parent.width; height: Constants.appStandardControlHeight }
                                    AppButton { id: openRunFolderButton; text: qsTr("Open Run Folder"); visible: root.hasRunFolder; enabled: visible; width: root.hasRunSummary ? (parent.width - parent.spacing) / 2 : parent.width; height: Constants.appStandardControlHeight }
                                }
                                AppButton {
                                    id: startAnotherTestButton
                                    visible: root.postOperation
                                    text: qsTr("Start Another Sequence Test")
                                    width: parent.width
                                    height: Constants.appStandardControlHeight
                                }
                            }
                        }
                        }
                    }
                }
            }
        }
    }
}
