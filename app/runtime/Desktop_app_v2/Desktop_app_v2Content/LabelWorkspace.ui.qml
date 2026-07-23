/* This is a UI file (.ui.qml) intended for Qt Design Studio editing. */
import QtQuick
import QtQuick.Controls
import Desktop_app_v2

Rectangle {
    id: root
    width: Constants.width - Constants.navigationWidth
    height: Constants.height - Constants.shellHeaderHeight
    color: Constants.backgroundColor

    property string presentation: "empty"
    property int classCount: 0
    property string datasetName: ""
    property string datasetPath: ""
    property int selectedCount: 0
    property string lockText: ""
    property string saveState: ""
    property bool selectedCropExpanded: true
    property bool classesFilterExpanded: true
    property bool threeClassMode: classCount === 3

    property alias openDatasetButton: openDatasetButton
    property alias twoClassChoice: twoClassChoice
    property alias threeClassChoice: threeClassChoice
    property alias useInTrainButton: useInTrainButton
    property alias selectedCropHeadingButton: selectedCropSection.headingButton
    property alias classesFilterHeadingButton: classesFilterSection.headingButton

    Column {
        anchors.fill: parent
        anchors.margins: Constants.workspaceMargin
        spacing: Constants.spacing

        Text { text: qsTr("Label"); font: Constants.largeFont; color: Constants.textColor; width: parent.width; height: Constants.controlHeight; verticalAlignment: Text.AlignVCenter }

        Row {
            width: parent.width
            spacing: Constants.spacing
            Text {
                text: root.presentation === "empty" ? qsTr("No Dataset selected") : qsTr("Dataset: ") + root.datasetName
                font: Constants.headingFont
                width: parent.width - openDatasetButton.width - Constants.spacing
                elide: Text.ElideRight
            }
            Button { id: openDatasetButton; text: qsTr("Open Dataset"); height: Constants.controlHeight }
        }

        Text {
            visible: root.presentation !== "empty" && root.datasetPath !== ""
            text: root.datasetPath
            color: Constants.mutedTextColor
            font: Constants.smallFont
            elide: Text.ElideMiddle
            width: parent.width
        }

        Rectangle {
            visible: root.presentation === "classDefinition"
            width: parent.width
            height: 100
            color: Constants.surfaceColor
            border.color: Constants.borderColor
            Row {
                anchors.fill: parent
                anchors.margins: Constants.spacing
                spacing: Constants.spacing * 2
                Text { text: qsTr("Number of Classes"); font: Constants.headingFont; anchors.verticalCenter: parent.verticalCenter }
                RadioButton { id: twoClassChoice; text: qsTr("2 Classes"); checked: root.classCount === 2; anchors.verticalCenter: parent.verticalCenter }
                RadioButton { id: threeClassChoice; text: qsTr("3 Classes"); checked: root.classCount === 3; anchors.verticalCenter: parent.verticalCenter }
                Text { text: qsTr("Class IDs become fixed after labeling begins."); color: Constants.mutedTextColor; font: Constants.smallFont; anchors.verticalCenter: parent.verticalCenter }
            }
        }

        Rectangle {
            visible: root.presentation === "empty"
            width: parent.width
            height: parent.height - y
            color: Constants.surfaceColor
            border.color: Constants.borderColor
            Text { text: qsTr("Open a Dataset to label Droplet Crops"); color: Constants.mutedTextColor; font: Constants.largeFont; anchors.centerIn: parent }
        }

        Item {
            visible: root.presentation !== "empty" && root.presentation !== "classDefinition"
            width: parent.width
            height: parent.height - y

            Row {
                anchors.fill: parent
                spacing: Constants.spacing

                Column {
                    id: cropColumn
                    width: parent.width * 0.65
                    height: parent.height
                    spacing: Constants.spacing

                    Rectangle {
                        width: parent.width
                        height: parent.height - imageCounts.height - Constants.spacing
                        color: Constants.viewerColor
                        border.color: Constants.borderColor
                        Text { text: qsTr("DROPLET CROP GRID"); color: Constants.surfaceColor; font: Constants.headingFont; anchors.left: parent.left; anchors.top: parent.top; anchors.margins: Constants.spacing }
                        Grid {
                            id: cropGridFocus
                            columns: 4
                            spacing: Constants.spacing
                            anchors.centerIn: parent
                            focus: true
                            Rectangle { width: 116; height: 86; color: "#7ea9d8"; border.color: Constants.surfaceColor; Text { text: qsTr("Crop 01"); anchors.centerIn: parent } }
                            Rectangle { width: 116; height: 86; color: "#e7a766"; border.color: Constants.surfaceColor; Text { text: qsTr("Crop 02"); anchors.centerIn: parent } }
                            Rectangle { width: 116; height: 86; color: "#a98bd4"; border.color: Constants.surfaceColor; visible: root.classCount === 3; Text { text: qsTr("Crop 03"); anchors.centerIn: parent } }
                            Rectangle { width: 116; height: 86; color: "#d4d7db"; border.color: Constants.surfaceColor; Text { text: qsTr("Unlabeled"); anchors.centerIn: parent } }
                            Rectangle { width: 116; height: 86; color: "#9ca3af"; border.color: Constants.surfaceColor; Text { text: qsTr("Skipped"); anchors.centerIn: parent } }
                            Rectangle { width: 116; height: 86; color: "#707782"; border.color: Constants.surfaceColor; Text { text: qsTr("Removed"); color: Constants.surfaceColor; anchors.centerIn: parent } }
                        }
                    }

                    Rectangle {
                        id: imageCounts
                        width: parent.width
                        height: 98
                        color: Constants.surfaceColor
                        border.color: Constants.borderColor
                        Column {
                            anchors.fill: parent
                            anchors.margins: Constants.spacing
                            Text { text: qsTr("IMAGE COUNTS"); font: Constants.headingFont }
                            Row { spacing: Constants.spacing * 2
                                Text { text: qsTr("Class 0: 24") }
                                Text { text: qsTr("Class 1: 18") }
                                Text { visible: root.classCount === 3; text: qsTr("Class 2: 12") }
                                Text { text: qsTr("Unlabeled: 8  Skipped: 2  Removed: 1"); color: Constants.mutedTextColor }
                            }
                        }
                    }
                }

                ScrollView {
                    id: rightPanel
                    width: parent.width - cropColumn.width - Constants.spacing
                    height: parent.height
                    contentWidth: availableWidth
                    contentHeight: rightSections.height

                    Column {
                        id: rightSections
                        width: rightPanel.availableWidth
                        height: implicitHeight
                        spacing: 2

                        CollapsibleSection {
                            id: selectedCropSection
                            width: parent.width
                            sectionTitle: qsTr("Selected Crop")
                            expanded: root.selectedCropExpanded
                            useIntrinsicBodyHeight: true

                            Column {
                                width: parent.width
                                height: implicitHeight
                                spacing: Constants.spacing

                                Rectangle {
                                    width: parent.width
                                    height: width
                                    color: Constants.viewerColor
                                    border.color: Constants.borderColor

                                    Image {
                                        id: selectedCropPreview
                                        anchors.fill: parent
                                        fillMode: Image.PreserveAspectFit
                                        sourceSize.width: 64
                                        sourceSize.height: 64
                                    }

                                    Rectangle {
                                        anchors.fill: parent
                                        visible: selectedCropPreview.status !== Image.Ready
                                        color: Constants.viewerColor
                                        Text {
                                            anchors.centerIn: parent
                                            text: qsTr("Selected Crop\n64 × 64\nSelected: %1").arg(root.selectedCount)
                                            horizontalAlignment: Text.AlignHCenter
                                            color: Constants.surfaceColor
                                            font: Constants.headingFont
                                        }
                                    }
                                }

                                Grid {
                                    width: parent.width
                                    height: 48
                                    columns: 3
                                    spacing: 6

                                    Button {
                                        id: class0Button
                                        width: (parent.width - parent.spacing * 2) / 3
                                        height: parent.height
                                        text: qsTr("Class 0")
                                        enabled: root.presentation === "ready"
                                        background: Rectangle { color: "#7ea9d8"; border.color: Constants.borderColor }
                                    }
                                    Button {
                                        id: class1Button
                                        width: (parent.width - parent.spacing * 2) / 3
                                        height: parent.height
                                        text: qsTr("Class 1")
                                        enabled: root.presentation === "ready"
                                        background: Rectangle { color: "#e7a766"; border.color: Constants.borderColor }
                                    }
                                    Button {
                                        id: class2Button
                                        width: (parent.width - parent.spacing * 2) / 3
                                        height: parent.height
                                        text: qsTr("Class 2")
                                        enabled: root.presentation === "ready" && root.threeClassMode
                                        background: Rectangle { color: "#a98bd4"; border.color: Constants.borderColor }
                                    }
                                }

                                Grid {
                                    width: parent.width
                                    height: Constants.controlHeight * 2 + spacing
                                    columns: 2
                                    spacing: 6

                                    Button { id: skipButton; width: (parent.width - parent.spacing) / 2; height: Constants.controlHeight; text: qsTr("Skip"); enabled: root.presentation === "ready" }
                                    Button { id: removeFromDatasetButton; width: (parent.width - parent.spacing) / 2; height: Constants.controlHeight; text: qsTr("Remove from Dataset"); enabled: root.presentation === "ready" }
                                    Button { id: restoreButton; width: (parent.width - parent.spacing) / 2; height: Constants.controlHeight; text: qsTr("Restore"); enabled: root.presentation === "ready" }
                                    Button { id: undoButton; width: (parent.width - parent.spacing) / 2; height: Constants.controlHeight; text: qsTr("Undo"); enabled: root.presentation === "ready" }
                                }
                            }
                        }

                        CollapsibleSection {
                            id: classesFilterSection
                            width: parent.width
                            sectionTitle: qsTr("Classes & Filter")
                            expanded: root.classesFilterExpanded
                            useIntrinsicBodyHeight: true

                            Column {
                                width: parent.width
                                height: implicitHeight
                                spacing: 6
                                Text { text: qsTr("CLASS NAMES & FILTERS"); font: Constants.headingFont }
                                Row { spacing: 6
                                    Button { id: allFilterButton; text: qsTr("All") }
                                    Button { id: class0FilterButton; text: qsTr("Class 0") }
                                    Button { id: class1FilterButton; text: qsTr("Class 1") }
                                    Button { id: class2FilterButton; visible: root.threeClassMode; text: qsTr("Class 2") }
                                }
                                Row { spacing: 6
                                    Button { id: unlabeledFilterButton; text: qsTr("Unlabeled") }
                                    Button { id: skippedFilterButton; text: qsTr("Skipped") }
                                    Button { id: removedFilterButton; text: qsTr("Removed") }
                                }
                                Row {
                                    spacing: 6
                                    Text { text: qsTr("0") }
                                    TextField { id: class0NameField; text: qsTr("Class 0"); width: 150 }
                                }
                                Row {
                                    spacing: 6
                                    Text { text: qsTr("1") }
                                    TextField { id: class1NameField; text: qsTr("Class 1"); width: 150 }
                                }
                                Row {
                                    visible: root.threeClassMode
                                    spacing: 6
                                    Text { text: qsTr("2") }
                                    TextField { id: class2NameField; text: qsTr("Class 2"); width: 150 }
                                }
                                Button { id: useInTrainButton; text: qsTr("Use in Train"); enabled: root.presentation === "ready"; height: Constants.controlHeight }
                            }
                        }
                    }
                }
            }

            Rectangle {
                visible: root.presentation === "locked" || root.presentation === "error"
                width: parent.width
                height: 48
                color: root.presentation === "error" ? Constants.errorSurfaceColor : Constants.surfaceColor
                border.color: root.presentation === "error" ? Constants.faultColor : Constants.warningColor
                anchors.top: parent.top
                Text { text: root.presentation === "error" ? qsTr("Error") : root.lockText; color: root.presentation === "error" ? Constants.faultColor : Constants.warningColor; anchors.centerIn: parent }
            }
        }

        Text {
            visible: root.presentation === "ready" && root.saveState !== ""
            text: root.saveState
            color: Constants.mutedTextColor
            font: Constants.smallFont
        }
    }

    states: [
        State { name: "empty"; PropertyChanges { root.presentation: "empty"; root.classCount: 0; root.datasetName: ""; root.datasetPath: ""; root.selectedCount: 0; root.lockText: ""; root.saveState: "" } },
        State { name: "classDefinition"; PropertyChanges { root.presentation: "classDefinition"; root.classCount: 2; root.datasetName: qsTr("Droplet Dataset"); root.datasetPath: qsTr("C:/OpenDSS/Datasets/droplets/dataset.json") } },
        State { name: "ready"; PropertyChanges { root.presentation: "ready"; root.classCount: 3; root.datasetName: qsTr("Droplet Dataset"); root.datasetPath: qsTr("C:/OpenDSS/Datasets/droplets/dataset.json"); root.selectedCount: 3; root.saveState: qsTr("Saved"); root.selectedCropExpanded: true; root.classesFilterExpanded: true } },
        State { name: "locked"; PropertyChanges { root.presentation: "locked"; root.classCount: 3; root.datasetName: qsTr("Droplet Dataset"); root.datasetPath: qsTr("C:/OpenDSS/Datasets/droplets/dataset.json"); root.selectedCount: 3; root.lockText: qsTr("Dataset is in use by Training") } },
        State { name: "error"; PropertyChanges { root.presentation: "error"; root.classCount: 3; root.datasetName: qsTr("Droplet Dataset"); root.datasetPath: qsTr("C:/OpenDSS/Datasets/droplets/dataset.json") } },
        State { name: "rightSectionsExpanded"; PropertyChanges { root.presentation: "ready"; root.classCount: 3; root.datasetName: qsTr("Droplet Dataset"); root.datasetPath: qsTr("C:/OpenDSS/Datasets/droplets/dataset.json"); root.selectedCount: 3; root.saveState: qsTr("Saved"); root.selectedCropExpanded: true; root.classesFilterExpanded: true } },
        State { name: "selectedCropCollapsed"; PropertyChanges { root.presentation: "ready"; root.classCount: 3; root.datasetName: qsTr("Droplet Dataset"); root.datasetPath: qsTr("C:/OpenDSS/Datasets/droplets/dataset.json"); root.selectedCount: 3; root.saveState: qsTr("Saved"); root.selectedCropExpanded: false; root.classesFilterExpanded: true } },
        State { name: "classesFilterCollapsed"; PropertyChanges { root.presentation: "ready"; root.classCount: 3; root.datasetName: qsTr("Droplet Dataset"); root.datasetPath: qsTr("C:/OpenDSS/Datasets/droplets/dataset.json"); root.selectedCount: 3; root.saveState: qsTr("Saved"); root.selectedCropExpanded: true; root.classesFilterExpanded: false } },
        State { name: "rightSectionsCollapsed"; PropertyChanges { root.presentation: "ready"; root.classCount: 3; root.datasetName: qsTr("Droplet Dataset"); root.datasetPath: qsTr("C:/OpenDSS/Datasets/droplets/dataset.json"); root.selectedCount: 3; root.saveState: qsTr("Saved"); root.selectedCropExpanded: false; root.classesFilterExpanded: false } }
    ]
}
