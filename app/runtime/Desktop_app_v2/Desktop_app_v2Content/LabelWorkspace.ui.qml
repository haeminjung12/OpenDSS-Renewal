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
    property int classCount: 3
    property string datasetName: qsTr("Droplet Dataset")
    property int totalCount: 18072
    property int labeledCount: 18072
    property bool rightPanelExpanded: true
    property bool datasetSummaryExpanded: true
    property bool labelExpanded: true
    property bool filterExpanded: true
    property alias rightPanelToggleButton: rightPanelToggleButton
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

    Column {
        anchors.fill: parent
        anchors.margins: Constants.workspaceMargin
        spacing: Constants.spacing

        Text { text: qsTr("Label"); font: Constants.largeFont; color: Constants.textColor; height: Constants.controlHeight }

        SplitView {
            font: Constants.font
            width: parent.width
            height: parent.height - y

            Rectangle {
                id: cropArea
                SplitView.fillWidth: true
                color: Constants.surfaceColor
                border.color: Constants.borderColor

                Text { id: cropGridTitle; text: qsTr("Droplet Crop Grid"); font: Constants.headingFont; color: Constants.textColor; anchors.top: parent.top; anchors.left: parent.left; anchors.margins: Constants.spacing }
                Row {
                    id: paginationControls
                    visible: root.presentation !== "empty"
                    height: Math.max(pageSpinBox.implicitHeight, imagesPerPageSelector.implicitHeight)
                    spacing: Constants.spacing
                    anchors.top: cropGridTitle.bottom
                    anchors.topMargin: Constants.spacing
                    anchors.left: parent.left
                    anchors.leftMargin: Constants.spacing
                    Text { text: qsTr("Page"); color: Constants.textColor; font: Constants.font; anchors.verticalCenter: parent.verticalCenter }
                    AppSpinBox { id: pageSpinBox; from: 1; to: 1; value: 1; height: Constants.appStandardControlHeight }
                    Text { text: qsTr("Images per page"); color: Constants.textColor; font: Constants.font; anchors.verticalCenter: parent.verticalCenter }
                    AppComboBox { id: imagesPerPageSelector; model: ["100", "200", "500"]; currentIndex: 2; width: Math.round(96 * Constants.textScale); height: Constants.appStandardControlHeight }
                }
                ScrollView {
                    id: cropGridScroll
                    visible: root.presentation !== "empty"
                    anchors.top: paginationControls.bottom
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: Constants.spacing
                    contentWidth: availableWidth
                    contentHeight: Math.max(availableHeight, cropGrid.implicitHeight)
                    clip: true

                Grid {
                    id: cropGrid
                    columns: Math.max(1, Math.floor((cropGridScroll.availableWidth + spacing) / (106 + spacing)))
                    spacing: Constants.spacing
                    width: columns * 106 + Math.max(0, columns - 1) * spacing
                    x: Math.max(0, (cropGridScroll.availableWidth - width) / 2)
                    y: Math.max(0, (parent.height - height) / 2)
                    Repeater {
                        model: 24
                        Rectangle {
                            required property int index
                            readonly property int globalIndex: index
                            readonly property int classIndex: (globalIndex * 37 + 11) % 3
                            width: 106
                            height: 82
                            color: classIndex === 0 ? "#dbeafe" : classIndex === 1 ? "#ffedd5" : "#ede9fe"
                            border.width: 4
                            border.color: classIndex === 0 ? "#2b6cb0" : classIndex === 1 ? "#e07a24" : "#7652b8"
                        }
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
                SplitView.maximumWidth: root.rightPanelExpanded ? parent.width * 0.75 : Constants.collapsedOperationPanelWidth
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
                            height: selectedCropPane.SplitView.maximumHeight + labelActionContent.implicitHeight + Constants.spacing
                            handle: Rectangle {
                                implicitHeight: Constants.spacing
                                color: Constants.borderColor
                            }

                            Item {
                                id: selectedCropPane
                                SplitView.preferredHeight: Math.min(Math.round(180 * Constants.textScale), labelSplitView.width)
                                SplitView.minimumHeight: Math.min(Math.round(120 * Constants.textScale), labelSplitView.width)
                                SplitView.maximumHeight: Math.max(labelSplitView.width, Math.min(Math.round(520 * Constants.textScale), labelSplitView.width * 2))
                                Rectangle {
                                    width: Math.min(parent.width, parent.height)
                                    height: width
                                    anchors.centerIn: parent
                                    color: Constants.viewerColor
                                    border.color: Constants.borderColor
                                    Text { anchors.centerIn: parent; text: qsTr("Selected Crop\n64 × 64"); horizontalAlignment: Text.AlignHCenter; color: Constants.surfaceColor; font: Constants.headingFont }
                                }
                            }

                            Item {
                                SplitView.minimumHeight: labelActionContent.implicitHeight
                                SplitView.preferredHeight: labelActionContent.implicitHeight
                                SplitView.fillHeight: true
                                Column {
                                    id: labelActionContent
                                    anchors.fill: parent
                                    spacing: Constants.spacing
                                    Grid { columns: 3; width: parent.width; spacing: 6
                                        AppButton { id: class0Button; width: (parent.width - 12) / 3; height: Constants.controlHeight; text: qsTr("Class 0"); visualRole: "identity"; identityColor: Constants.appClass0Color }
                                        AppButton { id: class1Button; width: (parent.width - 12) / 3; height: Constants.controlHeight; text: qsTr("Class 1"); visualRole: "identity"; identityColor: Constants.appClass1Color }
                                        AppButton { id: class2Button; width: (parent.width - 12) / 3; height: Constants.controlHeight; text: qsTr("Class 2"); enabled: root.classCount === 3; visualRole: "identity"; identityColor: Constants.appClass2Color }
                                    }
                                    AppButton { id: excludeButton; width: parent.width; height: Constants.appStandardControlHeight; text: qsTr("Exclude") }
                                    Row { width: parent.width; spacing: 6
                                        AppButton { id: undoButton; width: (parent.width - 12) / 3; height: Constants.appStandardControlHeight; text: qsTr("↶  Undo") }
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
                            AppButton { id: allFilterButton; width: parent.width; height: Constants.appStandardControlHeight; text: qsTr("All (%1)").arg(root.totalCount) }
                            AppButton { id: class0FilterButton; width: parent.width; height: Constants.appStandardControlHeight; text: qsTr("Class 0 (12000)") }
                            AppButton { id: class1FilterButton; width: parent.width; height: Constants.appStandardControlHeight; text: qsTr("Class 1 (6072)") }
                            AppButton { id: class2FilterButton; width: parent.width; height: Constants.appStandardControlHeight; text: root.classCount === 3 ? qsTr("Class 2 (0)") : qsTr("Class 2 (unavailable)"); enabled: root.classCount === 3 }
                            AppButton { id: excludedFilterButton; width: parent.width; height: Constants.appStandardControlHeight; text: qsTr("Excluded (0)") }
                            AppButton { id: unreviewedFilterButton; width: parent.width; height: Constants.appStandardControlHeight; text: qsTr("Unreviewed (%1)").arg(root.totalCount - root.labeledCount) }
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
    }

    states: [
        State { name: "empty"; PropertyChanges { root.presentation: "empty"; root.classCount: 0; root.datasetName: qsTr("No Dataset selected"); root.totalCount: 0; root.labeledCount: 0 } },
        State { name: "classDefinition"; PropertyChanges { root.presentation: "classDefinition"; root.classCount: 2; root.datasetName: qsTr("New Droplet Dataset"); root.totalCount: 0; root.labeledCount: 0 } },
        State { name: "ready"; PropertyChanges { root.presentation: "ready"; root.classCount: 3; root.datasetName: qsTr("Droplet Dataset"); root.totalCount: 18072; root.labeledCount: 18072 } },
        State { name: "rightPanelCollapsed"; PropertyChanges { root.rightPanelExpanded: false } }
    ]
}
