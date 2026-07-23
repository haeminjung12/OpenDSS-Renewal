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
    property alias activeModelRowButton: activeModelRowButton
    property alias candidateModelRowButton: candidateModelRowButton
    property alias setActiveButton: setActiveButton
    property alias openInModelTestButton: openInModelTestButton
    property alias selectedModelHeadingButton: selectedModelSection.headingButton

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

    Row {
        anchors.top: workspaceTitle.bottom
        anchors.topMargin: Constants.spacing
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Constants.workspaceMargin
        spacing: Constants.spacing
        Rectangle {
            width: parent.width * 0.4
            height: parent.height
            color: Constants.surfaceColor
            border.color: Constants.borderColor
            Column {
                anchors.fill: parent
                anchors.margins: Constants.spacing * 2
                spacing: Constants.spacing
                Text { text: qsTr("Models"); font: Constants.headingFont }
                Button { id: importButton; text: qsTr("Import Model"); height: Constants.controlHeight }
                Column {
                    id: modelList
                    visible: root.presentation !== "empty"
                    width: parent.width
                    spacing: 4
                    Button {
                        id: activeModelRowButton
                        width: parent.width
                        height: 58
                        padding: Constants.spacing
                        activeFocusOnTab: true
                        background: Rectangle {
                            color: root.selectedActive ? "#dcebdc" : Constants.backgroundColor
                            border.color: Constants.borderColor
                        }
                        contentItem: Row {
                            spacing: Constants.spacing
                            Text { text: qsTr("✓"); visible: true; color: Constants.readyColor; Accessible.name: qsTr("Active Model") }
                            Column { Text { text: qsTr("DropletNet-04"); font.bold: true } Text { text: qsTr("More Accurate"); color: Constants.mutedTextColor; font: Constants.smallFont } }
                        }
                    }
                    Button {
                        id: candidateModelRowButton
                        width: parent.width
                        height: 58
                        padding: Constants.spacing
                        activeFocusOnTab: true
                        background: Rectangle {
                            color: !root.selectedActive && root.hasSelection ? "#dfe8f4" : Constants.backgroundColor
                            border.color: Constants.borderColor
                        }
                        contentItem: Column {
                            Text { text: qsTr("DropletNet-03"); font.bold: true }
                            Text { text: qsTr("Faster"); color: Constants.mutedTextColor; font: Constants.smallFont }
                        }
                    }
                }
                Text { visible: root.presentation === "empty"; text: qsTr("No discovered v2 Model Packages"); color: Constants.mutedTextColor; wrapMode: Text.WordWrap; width: parent.width }
            }
        }
        Rectangle {
            width: parent.width * 0.6 - Constants.spacing
            height: parent.height
            color: root.showError ? Constants.errorSurfaceColor : Constants.surfaceColor
            border.color: root.showError ? Constants.faultColor : Constants.borderColor

            Column {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Constants.spacing

                CollapsibleSection {
                    id: selectedModelSection
                    width: parent.width
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
                            Text { visible: root.hasSelection && !root.showError; text: qsTr("DropletNet-03") ; font: Constants.headingFont }
                            Text { visible: root.hasSelection && !root.showError; text: qsTr("Active state: %1").arg(root.selectedActive ? qsTr("Active Model") : qsTr("Not Active")) }
                            Text { visible: root.hasSelection && !root.showError; text: qsTr("Trained: 2026-07-23\nDataset: Dataset-042\nModel Type: Faster\nClasses: 2\nTraining results: Accuracy 0.94\nPackage: C:/OpenDSS/Models/DropletNet-03.opendssmodel"); wrapMode: Text.WordWrap; width: parent.width }
                            Text { visible: root.modelLocked; text: qsTr("Model is in use by Model Test"); color: Constants.warningColor }
                            Button { id: setActiveButton; visible: root.hasSelection; text: qsTr("Set Active"); enabled: !root.selectedActive && !root.modelLocked; height: Constants.controlHeight }
                            Button { id: openInModelTestButton; visible: root.hasSelection; text: qsTr("Open in Model Test"); height: Constants.controlHeight }
                            Row { visible: root.hasSelection; spacing: Constants.spacing; Button { id: exportButton; text: qsTr("Export") } Button { id: duplicateButton; text: qsTr("Duplicate") } Button { id: renameButton; text: qsTr("Rename"); enabled: !root.selectedActive && !root.modelLocked } Button { id: deleteButton; text: qsTr("Delete"); enabled: !root.selectedActive && !root.modelLocked } }
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
