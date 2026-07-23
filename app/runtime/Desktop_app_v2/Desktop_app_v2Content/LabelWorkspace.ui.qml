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

        Row {
            width: parent.width
            height: parent.height - y
            spacing: Constants.spacing

            Rectangle {
                id: cropArea
                width: parent.width - rightPanel.width - parent.spacing
                height: parent.height
                color: Constants.surfaceColor
                border.color: Constants.borderColor

                Text { text: qsTr("Droplet Crop Grid"); font: Constants.headingFont; color: Constants.textColor; anchors.top: parent.top; anchors.left: parent.left; anchors.margins: Constants.spacing }
                Grid {
                    columns: 5
                    spacing: Constants.spacing
                    anchors.centerIn: parent
                    Repeater {
                        model: 15
                        Rectangle {
                            required property int index
                            width: 106
                            height: 82
                            color: Constants.viewerColor
                            border.width: 4
                            border.color: index % 3 === 0 ? "#2b6cb0" : index % 3 === 1 ? "#e07a24" : "#7652b8"
                            Text { anchors.centerIn: parent; color: Constants.surfaceColor; text: qsTr("Crop %1").arg(parent.index + 1); font: Constants.smallFont }
                        }
                    }
                }
            }

            Rectangle {
                id: rightPanel
                width: root.rightPanelExpanded ? Constants.operationPanelWidth : Constants.collapsedOperationPanelWidth
                height: parent.height
                color: Constants.surfaceColor
                border.color: Constants.borderColor

                Button {
                    id: rightPanelToggleButton
                    text: root.rightPanelExpanded ? qsTr("‹ Label panel") : qsTr("›")
                    width: parent.width - Constants.spacing * 2
                    height: Constants.controlHeight
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.topMargin: Constants.spacing
                }

                ScrollView {
                    id: rightPanelScroll
                    visible: root.rightPanelExpanded
                    anchors.top: rightPanelToggleButton.bottom
                    anchors.topMargin: Constants.spacing
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: saveAsFooter.top
                    anchors.bottomMargin: Constants.spacing
                    anchors.margins: Constants.spacing
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
                                Button { id: openDatasetButton; text: qsTr("Open Dataset"); height: Constants.controlHeight }
                            }
                        }
                    }

                    CollapsibleSection {
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
                                Text { text: qsTr("Class setup"); verticalAlignment: Text.AlignVCenter }
                                RadioButton { id: twoClassChoice; text: qsTr("2 classes"); checked: root.classCount === 2 }
                                RadioButton { id: threeClassChoice; text: qsTr("3 classes"); checked: root.classCount === 3 }
                            }
                            Text { text: qsTr("Configured schema: %1 classes").arg(root.classCount); color: Constants.mutedTextColor; wrapMode: Text.WordWrap; width: parent.width }
                        }
                    }

                    CollapsibleSection {
                        id: labelSection
                        width: parent.width
                        sectionTitle: qsTr("Label")
                        expanded: root.labelExpanded
                        useIntrinsicBodyHeight: true
                        Column {
                            width: parent.width
                            height: implicitHeight
                            spacing: Constants.spacing
                            Rectangle { width: parent.width; height: 152; color: Constants.viewerColor; border.color: Constants.borderColor; Text { anchors.centerIn: parent; text: qsTr("Selected Crop\n64 × 64"); horizontalAlignment: Text.AlignHCenter; color: Constants.surfaceColor; font: Constants.headingFont } }
                            Grid { columns: 3; width: parent.width; spacing: 6
                                Button { id: class0Button; width: (parent.width - 12) / 3; height: Constants.controlHeight; text: qsTr("Class 0"); background: Rectangle { color: "#2b6cb0" } }
                                Button { id: class1Button; width: (parent.width - 12) / 3; height: Constants.controlHeight; text: qsTr("Class 1"); background: Rectangle { color: "#e07a24" } }
                                Button { id: class2Button; width: (parent.width - 12) / 3; height: Constants.controlHeight; text: qsTr("Class 2"); enabled: root.classCount === 3; background: Rectangle { color: "#7652b8" } }
                            }
                            Button { id: excludeButton; width: parent.width; height: Constants.controlHeight; text: qsTr("Exclude") }
                            Row { width: parent.width; spacing: 6
                                Button { id: undoButton; width: (parent.width - 12) / 3; height: Constants.controlHeight; text: qsTr("Undo") }
                                Button { id: previousButton; width: (parent.width - 12) / 3; height: Constants.controlHeight; text: qsTr("Previous") }
                                Button { id: nextButton; width: (parent.width - 12) / 3; height: Constants.controlHeight; text: qsTr("Next") }
                            }
                        }
                    }

                    CollapsibleSection {
                        id: filterSection
                        width: parent.width
                        sectionTitle: qsTr("Filter")
                        expanded: root.filterExpanded
                        useIntrinsicBodyHeight: true
                        Column {
                            width: parent.width
                            height: implicitHeight
                            spacing: 4
                            Button { id: allFilterButton; width: parent.width; text: qsTr("All (%1)").arg(root.totalCount) }
                            Button { id: class0FilterButton; width: parent.width; text: qsTr("Class 0 (12000)") }
                            Button { id: class1FilterButton; width: parent.width; text: qsTr("Class 1 (6072)") }
                            Button { id: class2FilterButton; width: parent.width; text: root.classCount === 3 ? qsTr("Class 2 (0)") : qsTr("Class 2 (unavailable)"); enabled: root.classCount === 3 }
                            Button { id: excludedFilterButton; width: parent.width; text: qsTr("Excluded (0)") }
                            Button { id: unreviewedFilterButton; width: parent.width; text: qsTr("Unreviewed (%1)").arg(root.totalCount - root.labeledCount) }
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

                    Button { id: saveAsButton; width: 120; height: Constants.controlHeight; text: qsTr("Save As"); anchors.right: parent.right; anchors.rightMargin: Constants.spacing; anchors.verticalCenter: parent.verticalCenter }
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
