/* This is a UI file (.ui.qml) intended for Qt Design Studio editing. */
import QtQuick
import QtQuick.Controls.Basic
import Desktop_app_v2

Rectangle {
    id: root
    width: 1200
    height: 680
    color: Constants.backgroundColor
    property string presentation: "empty"
    property bool hasSelection: false
    property bool selectedActive: false
    property bool modelLocked: false
    property bool showError: false
    property bool selectedModelExpanded: true
    property bool rightPanelExpanded: true
    property var modelRows: []
    property bool hasDynamicModelRows: root.modelRows !== null && root.modelRows.length > 0
    property int selectedModelIndex: -1
    property string selectedModelName: ""
    property string selectedModelArchitecture: ""
    property string selectedModelPerformanceLabel: ""
    property string selectedModelClassSummary: ""
    property string selectedModelSourceDataset: ""
    property string selectedModelCreationDate: ""
    property string selectedModelPackageLocation: ""
    property string selectedModelTrainingMetrics: ""
    property alias modelListView: modelListView
    property alias modelRowButtonGroup: modelRowButtonGroup
    property alias productionModelRowButton: productionModelRowButton
    property alias activeModelRowButton: productionModelRowButton
    property alias candidateModelRowButton: candidateModelRowButton
    property alias importButton: importButton
    property alias setActiveButton: setActiveButton
    property alias openInModelTestButton: openInModelTestButton
    property alias exportButton: exportButton
    property alias duplicateButton: duplicateButton
    property alias renameButton: renameButton
    property alias deleteButton: deleteButton
    property alias selectedModelHeadingButton: selectedModelSection.headingButton
    property alias rightPanelToggleButton: rightPanelToggleButton

    ButtonGroup {
        id: modelRowButtonGroup
    }

    Text {
        id: workspaceTitle
        text: qsTr("Library")
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
            color: Constants.surfaceColor
            border.color: Constants.borderColor
            ScrollView {
                id: modelListScroll
                anchors.fill: parent
                anchors.margins: Constants.spacing * 2
                contentWidth: availableWidth
                clip: true
            Column {
                width: modelListScroll.availableWidth
                height: implicitHeight
                spacing: Constants.spacing
                Text { text: qsTr("Models"); font: Constants.headingFont }
                AppButton { id: importButton; text: qsTr("Import Model"); visualRole: "primary"; height: Constants.appPrimaryButtonHeight }
                Column {
                    id: modelList
                    visible: root.presentation !== "empty"
                    width: parent.width
                    spacing: 4
                    ListView {
                        id: modelListView
                        visible: root.hasDynamicModelRows
                        width: parent.width
                        height: contentHeight
                        model: root.modelRows
                        delegate: Button {
                            required property int index
                            required property var modelData
                            readonly property int rowIndex: index
                            readonly property var row: modelData
                            width: ListView.view.width
                            height: Math.round(82 * Constants.textScale)
                            padding: Constants.spacing
                            activeFocusOnTab: true
                            checkable: true
                            checked: index === root.selectedModelIndex
                            ButtonGroup.group: modelRowButtonGroup
                            background: Rectangle {
                                color: row.active ? "#dcebdc" : (parent.checked ? "#dfe8f4" : Constants.backgroundColor)
                                border.color: Constants.borderColor
                            }
                            contentItem: Row {
                                spacing: Constants.spacing
                                Text { text: qsTr("✓"); visible: row.active; color: Constants.readyColor; Accessible.name: qsTr("Active Model") }
                                Column {
                                    width: parent.width - (row.active ? 48 : 0)
                                    Text { text: row.name; font.bold: true; elide: Text.ElideRight; width: parent.width }
                                    Text { text: qsTr("Architecture: %1").arg(row.architecture); color: Constants.textColor; font: Constants.font; elide: Text.ElideRight; width: parent.width }
                                    Text { text: qsTr("%1  •  %2").arg(row.performanceLabel).arg(row.classSummary); color: Constants.mutedTextColor; font: Constants.smallFont; elide: Text.ElideRight; width: parent.width }
                                }
                            }
                        }
                    }
                    Button {
                        id: productionModelRowButton
                        visible: !root.hasDynamicModelRows
                        width: parent.width
                        height: Math.round(82 * Constants.textScale)
                        padding: Constants.spacing
                        activeFocusOnTab: true
                        background: Rectangle {
                            color: root.selectedActive ? "#dcebdc" : Constants.backgroundColor
                            border.color: Constants.borderColor
                        }
                        contentItem: Row {
                            spacing: Constants.spacing
                            Text { text: qsTr("✓"); visible: true; color: Constants.readyColor; Accessible.name: qsTr("Active Model") }
                            Column {
                                width: productionModelRowButton.width - 48
                                Text { text: qsTr("DropletNet-04"); font.bold: true }
                                Text { text: qsTr("Architecture: EfficientNet-B0"); color: Constants.textColor; font: Constants.font; elide: Text.ElideRight; width: parent.width }
                                Text { text: qsTr("More Accurate  •  Labels: Class 0, Class 1"); color: Constants.mutedTextColor; font: Constants.smallFont; elide: Text.ElideRight; width: parent.width }
                            }
                        }
                    }
                    Button {
                        id: candidateModelRowButton
                        visible: !root.hasDynamicModelRows
                        width: parent.width
                        height: Math.round(82 * Constants.textScale)
                        padding: Constants.spacing
                        activeFocusOnTab: true
                        background: Rectangle {
                            color: !root.selectedActive && root.hasSelection ? "#dfe8f4" : Constants.backgroundColor
                            border.color: Constants.borderColor
                        }
                        contentItem: Column {
                            Text { text: qsTr("DropletNet-03"); font.bold: true }
                            Text { text: qsTr("Architecture: MobileNetV3-Small"); color: Constants.textColor; font: Constants.font; elide: Text.ElideRight; width: candidateModelRowButton.width - Constants.spacing * 2 }
                            Text { text: qsTr("Faster  •  Labels: Class 0, Class 1"); color: Constants.mutedTextColor; font: Constants.smallFont; elide: Text.ElideRight; width: candidateModelRowButton.width - Constants.spacing * 2 }
                        }
                    }
                }
                Text { visible: root.presentation === "empty"; text: qsTr("No discovered v2 Model Packages"); color: Constants.mutedTextColor; wrapMode: Text.WordWrap; width: parent.width }
            }
            }
        }
        Rectangle {
            id: rightPanel
            SplitView.preferredWidth: Constants.operationPanelWidth
            SplitView.minimumWidth: Constants.collapsedOperationPanelWidth
            SplitView.maximumWidth: root.rightPanelExpanded ? Math.max(Constants.collapsedOperationPanelWidth, parent.width * 0.75) : Constants.collapsedOperationPanelWidth
            color: root.showError ? Constants.errorSurfaceColor : Constants.surfaceColor
            border.color: root.showError ? Constants.faultColor : Constants.borderColor

            Rectangle {
                id: panelTopStrip
                height: Constants.controlHeight
                color: Constants.backgroundColor
                border.color: Constants.borderColor
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                Text {
                    text: qsTr("Library")
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
                Accessible.name: root.rightPanelExpanded ? qsTr("Collapse Library panel") : qsTr("Expand Library panel")
            }

            ScrollView {
                id: selectedModelScroll
                visible: root.rightPanelExpanded
                anchors.top: panelTopStrip.bottom
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Constants.spacing
                anchors.leftMargin: Constants.spacing
                contentWidth: availableWidth
                clip: true

                AppAccordion {
                    id: selectedModelSection
                    width: selectedModelScroll.availableWidth
                    sectionTitle: qsTr("Selected Model")
                    expanded: root.selectedModelExpanded
                    useIntrinsicBodyHeight: true

                    Item {
                        width: parent.width
                        height: selectedModelContent.implicitHeight + Constants.spacing * 2
                        Column {
                            id: selectedModelContent
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.margins: Constants.spacing
                            spacing: Constants.spacing
                            Text { visible: root.showError; text: qsTr("Error"); font: Constants.largeFont; color: Constants.faultColor }
                            Text { visible: root.hasSelection && !root.showError; text: root.selectedModelName; font: Constants.headingFont }
                            Text { visible: root.hasSelection && !root.showError; text: qsTr("Active state: %1").arg(root.selectedActive ? qsTr("Active Model") : qsTr("Not Active")) }
                            Text { visible: root.hasSelection && !root.showError; text: qsTr("Trained: %1\nDataset: %2\nArchitecture: %3\n%4\nLabels: %5\nTraining results: %6\nPackage: %7").arg(root.selectedModelCreationDate).arg(root.selectedModelSourceDataset).arg(root.selectedModelArchitecture).arg(root.selectedModelPerformanceLabel).arg(root.selectedModelClassSummary).arg(root.selectedModelTrainingMetrics).arg(root.selectedModelPackageLocation); wrapMode: Text.WordWrap; width: parent.width }
                            Text { visible: root.modelLocked; text: qsTr("Model is in use by Model Test"); color: Constants.warningColor; wrapMode: Text.WordWrap; width: parent.width }
                            AppButton { id: setActiveButton; visible: root.hasSelection; text: qsTr("Set Active"); visualRole: "primary"; enabled: !root.selectedActive && !root.modelLocked; height: Constants.appPrimaryButtonHeight }
                            AppButton { id: openInModelTestButton; visible: root.hasSelection; text: qsTr("Open in Model Test"); height: Constants.appStandardControlHeight }
                            Flow { visible: root.hasSelection; width: parent.width; height: implicitHeight; spacing: Constants.spacing; AppButton { id: exportButton; text: qsTr("Export"); height: Constants.appStandardControlHeight } AppButton { id: duplicateButton; text: qsTr("Duplicate"); height: Constants.appStandardControlHeight } AppButton { id: renameButton; text: qsTr("Rename"); enabled: !root.selectedActive && !root.modelLocked; height: Constants.appStandardControlHeight } AppButton { id: deleteButton; text: qsTr("Delete"); visualRole: "destructive"; enabled: !root.selectedActive && !root.modelLocked; height: Constants.appStandardControlHeight } }
                        }
                    }
                }
            }
        }
    }

    states: [
        State { name: "empty"; PropertyChanges { root.presentation: "empty"; root.hasSelection: false; root.selectedActive: false; root.modelLocked: false; root.showError: false } },
        State { name: "readySelected"; PropertyChanges { root.presentation: "readySelected"; root.hasSelection: true; root.selectedActive: false; root.modelLocked: false; root.showError: false } },
        State { name: "readyActive"; PropertyChanges { root.presentation: "readyActive"; root.hasSelection: true; root.selectedActive: true; root.modelLocked: false; root.showError: false } },
        State { name: "locked"; PropertyChanges { root.presentation: "locked"; root.hasSelection: true; root.selectedActive: true; root.modelLocked: true; root.showError: false } },
        State { name: "error"; PropertyChanges { root.presentation: "error"; root.hasSelection: false; root.showError: true } }
    ]
}
