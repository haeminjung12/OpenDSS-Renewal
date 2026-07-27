pragma ComponentBehavior: Bound
/* This is a UI file (.ui.qml) intended for Qt Design Studio editing. */
import QtQuick
import QtQuick.Controls.Basic
import Desktop_app_v2

Rectangle {
    id: root
    width: Constants.width - Constants.navigationWidth
    height: Constants.height - Constants.shellHeaderHeight
    color: Constants.backgroundColor

    property string presentation: "ready"
    property string currentFilter: "all"
    property int classCount: 3
    property string datasetName: qsTr("Droplet Dataset")
    property int totalCount: 18072
    property int labeledCount: 18072
    property int class0Count: 12000
    property int class1Count: 6072
    property int class2Count: 0
    property int excludedCount: 0
    property int unreviewedCount: 0
    property var classNames: [qsTr("Class 0"), qsTr("Class 1"), qsTr("Class 2")]
    readonly property string class0DisplayName: classNames.length > 0 && classNames[0] !== ""
                                                       ? classNames[0] : qsTr("Class 0")
    readonly property string class1DisplayName: classNames.length > 1 && classNames[1] !== ""
                                                       ? classNames[1] : qsTr("Class 1")
    readonly property string class2DisplayName: classNames.length > 2 && classNames[2] !== ""
                                                       ? classNames[2] : qsTr("Class 2")
    property var filteredCropRecords: []
    property string selectedCropId: ""
    property int selectedCropIndex: -1
    property bool canUndo: false
    property string errorMessage: ""
    property bool rightPanelExpanded: true
    property bool datasetSummaryExpanded: true
    property bool labelExpanded: true
    property bool filterExpanded: true
    property bool smallDropletSelectionArmed: false
    property bool smallDropletSelectionVisible: false
    property real smallDropletSelectionStartXRatio: 0.0
    property real smallDropletSelectionStartYRatio: 0.0
    property real smallDropletSelectionEndXRatio: 0.0
    property real smallDropletSelectionEndYRatio: 0.0
    property alias rightPanelToggleButton: rightPanelToggleButton
    property alias smallDropletSelectionInputArea: smallDropletSelectionInputArea
    property alias openDatasetButton: openDatasetButton
    property alias twoClassChoice: twoClassChoice
    property alias threeClassChoice: threeClassChoice
    property alias class0Button: class0Button
    property alias class1Button: class1Button
    property alias class2Button: class2Button
    property alias excludeButton: excludeButton
    property alias undoButton: undoButton
    property alias previousButton: previousButton
    property alias nextButton: nextButton
    property alias saveAsButton: saveAsButton
    property alias datasetSummaryHeadingButton: datasetSummarySection.headingButton
    property alias labelHeadingButton: labelSection.headingButton
    property alias filterHeadingButton: filterSection.headingButton
    property alias allFilterButton: allFilterButton
    property alias class0FilterButton: class0FilterButton
    property alias class1FilterButton: class1FilterButton
    property alias class2FilterButton: class2FilterButton
    property alias excludedFilterButton: excludedFilterButton
    property alias unreviewedFilterButton: unreviewedFilterButton
    property alias cropGridHost: cropGridHost
    property alias class0NameField: class0NameField
    property alias class1NameField: class1NameField
    property alias class2NameField: class2NameField
    property alias errorMessageText: errorMessageText.text
    property alias selectedCropSource: selectedCropImage.source

    Text {
        id: workspaceTitle
        text: qsTr("Label")
        font: Constants.largeFont
        color: Constants.textColor
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Constants.workspaceMargin
    }

    SplitView {
            font: Constants.font
            anchors.top: workspaceTitle.bottom
            anchors.topMargin: Constants.spacing
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Constants.workspaceMargin
            anchors.rightMargin: Constants.workspaceMargin
            anchors.bottomMargin: Constants.workspaceMargin

            Rectangle {
                id: cropArea
                SplitView.fillWidth: true
                color: Constants.surfaceColor
                border.color: Constants.borderColor

                Text { id: cropGridTitle; text: qsTr("Droplet Crop Grid"); font: Constants.headingFont; color: Constants.textColor; anchors.top: parent.top; anchors.left: parent.left; anchors.margins: Constants.spacing }
                Text {
                    id: errorMessageText
                    visible: root.errorMessage !== ""
                    text: root.errorMessage
                    color: Constants.faultColor
                    font: Constants.font
                    wrapMode: Text.WordWrap
                    anchors.top: cropGridTitle.bottom
                    anchors.topMargin: Constants.spacing
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: Constants.spacing
                    anchors.rightMargin: Constants.spacing
                }
                GridView {
                    id: cropGridHost
                    visible: root.presentation !== "empty"
                    activeFocusOnTab: true
                    Accessible.role: Accessible.List
                    Accessible.name: qsTr("Droplet crop grid")
                    anchors.top: errorMessageText.visible ? errorMessageText.bottom : cropGridTitle.bottom
                    anchors.topMargin: Constants.spacing
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: Constants.spacing
                    clip: true
                    cellWidth: 185
                    cellHeight: 185
                    reuseItems: true
                    currentIndex: root.selectedCropIndex
                    highlightMoveDuration: 0
                    highlight: Item {
                        width: cropGridHost.cellWidth
                        height: cropGridHost.cellHeight
                        z: 2
                        Accessible.ignored: true

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 6
                            color: Constants.backgroundColor
                            opacity: 0.55
                            Accessible.ignored: true
                        }

                        Rectangle {
                            anchors.fill: parent
                            color: "transparent"
                            border.color: Constants.borderColor
                            border.width: 6
                            Accessible.ignored: true
                        }

                        Rectangle {
                            width: 28
                            height: 28
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.margins: 10
                            color: Constants.backgroundColor
                            border.color: Constants.textColor
                            border.width: 2
                            Accessible.ignored: true

                            Text {
                                text: qsTr("✓")
                                anchors.centerIn: parent
                                color: Constants.textColor
                                font.bold: true
                                Accessible.ignored: true
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 1
                            visible: cropGridHost.activeFocus
                            color: "transparent"
                            border.color: Constants.accentColor
                            border.width: 2
                            Accessible.ignored: true
                        }
                    }
                }
                Text {
                    visible: root.presentation === "empty"
                    text: qsTr("No Dataset selected\nOpen a Dataset to begin labeling.")
                    color: Constants.mutedTextColor
                    font: Constants.headingFont
                    horizontalAlignment: Text.AlignHCenter
                    anchors.centerIn: parent
                }
            }

            Rectangle {
                id: rightPanel
                SplitView.preferredWidth: Constants.operationPanelWidth
                SplitView.minimumWidth: Constants.collapsedOperationPanelWidth
                SplitView.maximumWidth: root.rightPanelExpanded
                                        ? Math.max(Constants.collapsedOperationPanelWidth,
                                                   parent.width * 0.75)
                                        : Constants.collapsedOperationPanelWidth
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
                        text: qsTr("Label")
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
                }

                ScrollView {
                    id: rightPanelScroll
                    visible: root.rightPanelExpanded
                    anchors.top: panelTopStrip.bottom
                    anchors.topMargin: Constants.spacing
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: saveAsFooter.top
                    anchors.bottomMargin: Constants.spacing
                    anchors.margins: Constants.spacing
                    anchors.leftMargin: Constants.spacing
                    contentWidth: availableWidth
                    contentHeight: rightSections.height
                    clip: true

                    Column {
                        id: rightSections
                        width: rightPanelScroll.availableWidth
                        height: implicitHeight
                        spacing: 2

                    Rectangle {
                        width: parent.width
                        height: loadDatasetContent.implicitHeight + Constants.spacing * 2
                        color: Constants.backgroundColor
                        border.color: Constants.borderColor
                        Column {
                            id: loadDatasetContent
                            anchors.fill: parent
                            anchors.margins: Constants.spacing
                            spacing: 6
                            Text { text: qsTr("Load Dataset"); font: Constants.headingFont; color: Constants.textColor }
                            Row { width: parent.width; spacing: 6
                                Text { text: root.presentation === "empty" ? qsTr("No Dataset selected") : root.datasetName; width: parent.width - openDatasetButton.width - 6; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                AppButton { id: openDatasetButton; text: qsTr("Open Dataset"); height: Constants.appStandardControlHeight }
                            }
                        }
                    }

                    AppAccordion {
                        id: datasetSummarySection
                        width: parent.width
                        sectionTitle: qsTr("Dataset Summary")
                        expanded: root.datasetSummaryExpanded
                        useIntrinsicBodyHeight: true
                        Column {
                            width: parent.width
                            height: implicitHeight
                            spacing: 6
                            Text { text: qsTr("Total: %1").arg(root.totalCount) }
                            Text { text: qsTr("Labeled: %1").arg(root.labeledCount) }
                            Row {
                                spacing: Constants.spacing
                                Text { text: qsTr("Class setup"); font: Constants.font; verticalAlignment: Text.AlignVCenter }
                                AppRadioButton { id: twoClassChoice; text: qsTr("2 classes"); checked: root.classCount === 2; height: Constants.appStandardControlHeight }
                                AppRadioButton { id: threeClassChoice; text: qsTr("3 classes"); checked: root.classCount === 3; height: Constants.appStandardControlHeight }
                            }
                            Text { text: qsTr("Configured schema: %1 classes").arg(root.classCount); color: Constants.mutedTextColor; wrapMode: Text.WordWrap; width: parent.width }
                            Text { text: qsTr("Class names"); font: Constants.font; color: Constants.textColor }
                            AppTextField { id: class0NameField; width: parent.width; height: Constants.appStandardControlHeight; enabled: root.classCount > 0; text: root.classNames.length > 0 ? root.classNames[0] : ""; Accessible.name: qsTr("Class 0 name") }
                            AppTextField { id: class1NameField; width: parent.width; height: Constants.appStandardControlHeight; enabled: root.classCount > 0; text: root.classNames.length > 1 ? root.classNames[1] : ""; Accessible.name: qsTr("Class 1 name") }
                            AppTextField { id: class2NameField; width: parent.width; height: Constants.appStandardControlHeight; enabled: root.classCount === 3; text: root.classNames.length > 2 ? root.classNames[2] : ""; Accessible.name: qsTr("Class 2 name") }
                        }
                    }

                    AppAccordion {
                        id: labelSection
                        width: parent.width
                        sectionTitle: qsTr("Label")
                        expanded: root.labelExpanded
                        useIntrinsicBodyHeight: true
                        SplitView {
                            id: labelSplitView
                            orientation: Qt.Vertical
                            width: parent.width
                            height: selectedCropPane.SplitView.preferredHeight + labelActionContent.implicitHeight + Constants.spacing
                            handle: Rectangle {
                                implicitHeight: Constants.spacing
                                color: Constants.borderColor
                            }

                            Item {
                                id: selectedCropPane
                                SplitView.preferredHeight: Math.min(Math.round(180 * Constants.textScale), labelSplitView.width)
                                SplitView.minimumHeight: Math.min(Math.round(120 * Constants.textScale), labelSplitView.width)
                                SplitView.maximumHeight: Math.max(labelSplitView.width, Math.min(Math.round(520 * Constants.textScale), labelSplitView.width * 2))
                                SplitView.fillHeight: true
                                FullSizeImageViewer {
                                    id: selectedCropImage
                                    width: Math.min(parent.width, parent.height)
                                    height: width
                                    anchors.centerIn: parent
                                    placeholderText: qsTr("Selected Crop\n64 × 64")

                                    Item {
                                        id: smallDropletSelectionOverlay
                                        anchors.fill: parent
                                        visible: selectedCropImage.image.visible

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
                                                     && selectedCropImage.image.visible
                                            cursorShape: Qt.CrossCursor
                                        }
                                    }
                                }
                            }

                            Item {
                                SplitView.minimumHeight: labelActionContent.implicitHeight
                                SplitView.preferredHeight: labelActionContent.implicitHeight
                                Column {
                                    id: labelActionContent
                                    anchors.fill: parent
                                    spacing: Constants.spacing
                                    Grid { columns: 3; width: parent.width; spacing: 6
                                        AppButton { id: class0Button; width: (parent.width - 12) / 3; height: Constants.controlHeight; text: root.class0DisplayName; visualRole: "identity"; identityColor: Constants.appClass0Color }
                                        AppButton { id: class1Button; width: (parent.width - 12) / 3; height: Constants.controlHeight; text: root.class1DisplayName; visualRole: "identity"; identityColor: Constants.appClass1Color }
                                        AppButton { id: class2Button; width: (parent.width - 12) / 3; height: Constants.controlHeight; text: root.class2DisplayName; enabled: root.classCount === 3; visualRole: "identity"; identityColor: Constants.appClass2Color }
                                    }
                                    AppButton { id: excludeButton; width: parent.width; height: Constants.appStandardControlHeight; text: qsTr("Exclude") }
                                    Row { width: parent.width; spacing: 6
                                        AppButton { id: undoButton; width: (parent.width - 12) / 3; height: Constants.appStandardControlHeight; text: qsTr("↶  Undo"); enabled: root.canUndo }
                                        AppButton { id: previousButton; width: (parent.width - 12) / 3; height: Constants.appStandardControlHeight; text: qsTr("←  Previous") }
                                        AppButton { id: nextButton; width: (parent.width - 12) / 3; height: Constants.appStandardControlHeight; text: qsTr("Next  →") }
                                    }
                                }
                            }
                        }
                    }

                    AppAccordion {
                        id: filterSection
                        width: parent.width
                        sectionTitle: qsTr("Filter")
                        expanded: root.filterExpanded
                        useIntrinsicBodyHeight: true
                        Column {
                            width: parent.width
                            height: implicitHeight
                            spacing: 4
                            AppButton { id: allFilterButton; width: parent.width; height: Constants.appStandardControlHeight; text: qsTr("All (%1)").arg(root.totalCount); checked: root.currentFilter === "all"; visualRole: checked ? "primary" : "secondary" }
                            AppButton { id: class0FilterButton; width: parent.width; height: Constants.appStandardControlHeight; text: qsTr("%1 (%2)").arg(root.class0DisplayName).arg(root.class0Count); checked: root.currentFilter === "class0"; visualRole: checked ? "primary" : "secondary" }
                            AppButton { id: class1FilterButton; width: parent.width; height: Constants.appStandardControlHeight; text: qsTr("%1 (%2)").arg(root.class1DisplayName).arg(root.class1Count); checked: root.currentFilter === "class1"; visualRole: checked ? "primary" : "secondary" }
                            AppButton { id: class2FilterButton; width: parent.width; height: Constants.appStandardControlHeight; text: qsTr("%1 (%2)").arg(root.class2DisplayName).arg(root.class2Count); enabled: root.classCount === 3; checked: root.currentFilter === "class2"; visualRole: checked ? "primary" : "secondary" }
                            AppButton { id: excludedFilterButton; width: parent.width; height: Constants.appStandardControlHeight; text: qsTr("Excluded (%1)").arg(root.excludedCount); checked: root.currentFilter === "excluded"; visualRole: checked ? "primary" : "secondary" }
                            AppButton { id: unreviewedFilterButton; width: parent.width; height: Constants.appStandardControlHeight; text: qsTr("Unreviewed (%1)").arg(root.unreviewedCount); checked: root.currentFilter === "unreviewed"; visualRole: checked ? "primary" : "secondary" }
                        }
                    }

                    }
                }
                Rectangle {
                    id: saveAsFooter
                    visible: root.rightPanelExpanded
                    height: Constants.controlHeight + Constants.spacing * 2
                    color: Constants.backgroundColor
                    border.color: Constants.borderColor
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: Constants.spacing

                    AppButton { id: saveAsButton; width: 120; height: Constants.appStandardControlHeight; text: qsTr("Save As"); anchors.right: parent.right; anchors.rightMargin: Constants.spacing; anchors.verticalCenter: parent.verticalCenter }
                }
            }
    }

    states: [
        State { name: "empty"; PropertyChanges { root.presentation: "empty"; root.classCount: 0; root.datasetName: qsTr("No Dataset selected"); root.totalCount: 0; root.labeledCount: 0 } },
        State { name: "classDefinition"; PropertyChanges { root.presentation: "classDefinition"; root.classCount: 2; root.datasetName: qsTr("New Droplet Dataset"); root.totalCount: 0; root.labeledCount: 0 } },
        State { name: "ready"; PropertyChanges { root.presentation: "ready"; root.classCount: 3; root.datasetName: qsTr("Droplet Dataset"); root.totalCount: 18072; root.labeledCount: 18072 } },
        State { name: "rightPanelCollapsed"; PropertyChanges { root.rightPanelExpanded: false } }
    ]
}
