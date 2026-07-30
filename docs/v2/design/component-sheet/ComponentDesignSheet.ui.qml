/*
This is a static design-reference sheet intended for Qt Design Studio 2D view.
*/
import QtQuick

Rectangle {
    id: root

    width: 1800
    height: sheetColumn.height + 64
    color: "#F3F6F9"

    property alias panelToggleProofItem: structureTile
    property alias textSizeProofItem: textSizeProofTile
    readonly property color primaryText: "#17202A"
    readonly property color secondaryText: "#5D6978"
    readonly property color borderSubtle: "#D7DEE7"
    readonly property color borderDefault: "#AAB7C5"
    readonly property color brand: "#276DA3"
    readonly property color brandHover: "#236A9D"
    readonly property color brandPressed: "#1E5D8F"
    readonly property color canvas: "#15283F"
    readonly property color surface: "#FFFFFF"
    readonly property color subtle: "#F8FAFC"
    readonly property color disabled: "#EEF3F7"
    property font bodyFont
    property font labelFont
    property font majorSectionFont
    property font sectionFont
    property font titleFont
    property font buttonFont
    property font captionFont
    property font metricFont
    bodyFont.family: "Segoe UI Variable"
    bodyFont.pixelSize: 16
    labelFont.family: "Segoe UI Variable"
    labelFont.pixelSize: 15
    majorSectionFont.family: "Segoe UI Variable"
    majorSectionFont.pixelSize: 18
    majorSectionFont.weight: Font.DemiBold
    sectionFont.family: "Segoe UI Variable"
    sectionFont.pixelSize: 16
    sectionFont.weight: Font.DemiBold
    titleFont.family: "Segoe UI Variable"
    titleFont.pixelSize: 24
    buttonFont.family: "Segoe UI Variable"
    buttonFont.pixelSize: 16
    buttonFont.weight: Font.DemiBold
    captionFont.family: "Segoe UI Variable"
    captionFont.pixelSize: 13
    metricFont.family: "Segoe UI Variable"
    metricFont.pixelSize: 28
    metricFont.weight: Font.DemiBold

    Column {
        id: sheetColumn
        x: 32
        y: 32
        width: root.width - 64
        spacing: 24

        Text {
            width: parent.width
            text: qsTr("OpenDSS Component Design Sheet")
            color: root.primaryText
            font: root.titleFont
        }

        Text {
            width: parent.width
            text: qsTr("Qt Design Studio review artifact · 16/20 body and controls · 15/18 ordinary labels · Hardware Configuration remains beneath left navigation")
            color: root.secondaryText
            font: root.bodyFont
            wrapMode: Text.WordWrap
        }

        DesignTile {
            width: parent.width
            componentName: qsTr("Foundations")
            purpose: qsTr("Approved color, typography, spacing, radius, border, and focus tokens.")
            anatomy: qsTr("Semantic palette, type ramp, measured spacing, control radius, panel radius, and visible focus.")
            variants: qsTr("Single light application theme; dark surfaces are viewer canvases only.")
            stateList: qsTr("Rest, selected, focus, success, warning, error, and disabled.")
            dimensions: qsTr("16/20 body and controls; 15/18 ordinary labels; 13/16 captions; 24/32 page title; 18/26 major title; 16/22 section title.")
            accessibilityNote: qsTr("Status never relies on color alone; focus uses a visible 2 px ring.")
            contentHeight: 530

            Column {
                width: parent.width
                spacing: 18

                Row {
                    spacing: 12

                    DesignState { sampleText: qsTr("Surface"); label: qsTr("neutral.0"); fillColor: "#FFFFFF" }
                    DesignState { sampleText: qsTr("Subtle"); label: qsTr("neutral.25"); fillColor: "#F8FAFC" }
                    DesignState { sampleText: qsTr("App"); label: qsTr("neutral.50"); fillColor: "#F3F6F9" }
                    DesignState { sampleText: qsTr("Selected"); label: qsTr("brand.50"); fillColor: "#E7F0F8"; showMarker: true }
                    DesignState { sampleText: qsTr("Primary"); label: qsTr("brand.600"); fillColor: "#276DA3"; textColor: "#FFFFFF"; outlineColor: "#276DA3" }
                    DesignState { sampleText: qsTr("Viewer canvas"); label: qsTr("canvas.900"); fillColor: "#15283F"; textColor: "#FFFFFF"; outlineColor: "#15283F" }
                    DesignState { sampleText: qsTr("Disabled"); label: qsTr("neutral.100"); fillColor: "#EEF3F7"; textColor: "#6B7785" }
                }

                Row {
                    spacing: 12

                    DesignState { sampleText: qsTr("Ready ✓"); label: qsTr("Success"); fillColor: "#EAF6F0"; textColor: "#1F7A55"; outlineColor: "#1F7A55" }
                    DesignState { sampleText: qsTr("Warning !"); label: qsTr("Warning"); fillColor: "#FFF4D8"; textColor: "#8A5A00"; outlineColor: "#8A5A00" }
                    DesignState { sampleText: qsTr("Failed ×"); label: qsTr("Error"); fillColor: "#FDECEC"; textColor: "#B42318"; outlineColor: "#B42318" }
                    DesignState { sampleText: qsTr("Info i"); label: qsTr("Information"); fillColor: "#E7F0F8"; textColor: "#2563A6"; outlineColor: "#2563A6" }
                    DesignState { sampleText: qsTr("Class 0"); label: qsTr("Blue · 0"); fillColor: "#E0F2FE"; textColor: "#075985"; outlineColor: "#075985" }
                    DesignState { sampleText: qsTr("Class 1"); label: qsTr("Orange · 1"); fillColor: "#FFEDD5"; textColor: "#9A3412"; outlineColor: "#9A3412" }
                    DesignState { sampleText: qsTr("Class 2"); label: qsTr("Purple · 2"); fillColor: "#F3E8FF"; textColor: "#6B21A8"; outlineColor: "#6B21A8" }
                }

                Row {
                    spacing: 28

                    Text { text: qsTr("Page title 24 / 32"); color: root.primaryText; font: root.titleFont }
                    Text { text: qsTr("Major section 18 / 26"); color: root.primaryText; font: root.majorSectionFont }
                    Text { text: qsTr("Section 16 / 22"); color: root.primaryText; font: root.sectionFont }
                    Text { text: qsTr("Body and controls 16 / 20"); color: root.primaryText; font: root.bodyFont }
                }

                Row {
                    spacing: 28

                    Text { text: qsTr("Ordinary labels 15 / 18"); color: root.primaryText; font: root.labelFont }
                    Text { text: qsTr("Button 16 / 20"); color: root.primaryText; font: root.buttonFont }
                    Text { text: qsTr("Caption and status 13 / 16"); color: root.secondaryText; font: root.captionFont }
                    Text { text: qsTr("Metric 28"); color: root.primaryText; font: root.metricFont }
                }

                Row {
                    spacing: 18

                    Text {
                        text: qsTr("Text Size")
                        color: root.primaryText
                        font: root.labelFont
                        width: 120
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Rectangle {
                        width: 350
                        height: 44
                        radius: 4
                        color: root.surface
                        border.color: root.brand
                        border.width: 2

                        Text { text: qsTr("Medium (100%) — Default"); color: root.primaryText; font: root.bodyFont; x: 12; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: qsTr("⌄"); color: root.brand; font: root.bodyFont; anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter }
                    }

                    DesignState { width: 210; sampleText: qsTr("Small (80%)"); label: qsTr("Selectable") }
                    DesignState { width: 230; sampleText: qsTr("Medium (100%)"); label: qsTr("Default"); fillColor: "#E7F0F8"; showMarker: true }
                    DesignState { width: 210; sampleText: qsTr("Large (125%)"); label: qsTr("Selectable") }

                    Text {
                        text: qsTr("200% is validation-only")
                        color: root.secondaryText
                        font: root.captionFont
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }

        DesignTile {
            width: parent.width
            componentName: qsTr("Actions · AppButton and AppIconButton")
            purpose: qsTr("One coherent action hierarchy with stable geometry across states.")
            anatomy: qsTr("Label or icon, content padding, semantic surface, focus ring, and optional loading cue.")
            variants: qsTr("Primary, secondary, tertiary, destructive, standard icon, and dark-canvas icon.")
            stateList: qsTr("Rest, hover, pressed, focus, disabled, and loading.")
            dimensions: qsTr("32 px standard; 36 px primary workflow action; 32 × 32 px icon.")
            accessibilityNote: qsTr("Icon-only actions require accessible names and tooltips.")
            contentHeight: 230

            Column {
                width: parent.width
                spacing: 18

                Row {
                    spacing: 12
                    DesignState { sampleText: qsTr("Start Training"); label: qsTr("Primary · rest"); fillColor: root.brand; textColor: "#FFFFFF"; outlineColor: root.brand }
                    DesignState { sampleText: qsTr("Start Training"); label: qsTr("Hover"); fillColor: root.brandHover; textColor: "#FFFFFF"; outlineColor: root.brandHover }
                    DesignState { sampleText: qsTr("Start Training"); label: qsTr("Pressed"); fillColor: root.brandPressed; textColor: "#FFFFFF"; outlineColor: root.brandPressed }
                    DesignState { sampleText: qsTr("Start Training"); label: qsTr("Focus"); fillColor: root.brand; textColor: "#FFFFFF"; outlineColor: "#FFFFFF"; outlineWidth: 3 }
                    DesignState { sampleText: qsTr("Start Training"); label: qsTr("Disabled"); fillColor: root.disabled; textColor: "#6B7785"; outlineColor: root.borderSubtle }
                    DesignState { sampleText: qsTr("Working…"); label: qsTr("Loading"); fillColor: root.brand; textColor: "#FFFFFF"; outlineColor: root.brand }
                }

                Row {
                    spacing: 12
                    DesignState { sampleText: qsTr("Open Dataset"); label: qsTr("Secondary"); fillColor: root.surface; outlineColor: root.borderDefault }
                    DesignState { sampleText: qsTr("Undo"); label: qsTr("Tertiary"); fillColor: root.subtle; outlineColor: root.subtle }
                    DesignState { sampleText: qsTr("Delete Model"); label: qsTr("Destructive"); fillColor: "#B42318"; textColor: "#FFFFFF"; outlineColor: "#B42318" }
                    DesignState { sampleText: qsTr("＋"); label: qsTr("Zoom in"); fillColor: root.surface }
                    DesignState { sampleText: qsTr("−"); label: qsTr("Zoom out"); fillColor: root.canvas; textColor: "#FFFFFF"; outlineColor: "#FFFFFF"; outlineWidth: 2 }
                }
            }
        }

        DesignTile {
            width: parent.width
            componentName: qsTr("Inputs · fields, selectors, numeric controls, check boxes, and switches")
            purpose: qsTr("Shared field geometry and a clear semantic distinction between inclusion and persistent operational state.")
            anatomy: qsTr("Label above, control surface, value, trailing affordance, focus ring, and supporting or validation line.")
            variants: qsTr("Text field, text area, combo box, spin box, check box, AppSwitch, and radio-group anatomy.")
            stateList: qsTr("Rest, focus, open, disabled, read-only, error, checked, and on.")
            dimensions: qsTr("32 px single-line controls; 72 px text area; content-driven growth at enlarged text.")
            accessibilityNote: qsTr("Labels remain visible; error uses border, icon, and actionable text.")
            contentHeight: 480

            Column {
                width: parent.width
                spacing: 18

                Row {
                    spacing: 24

                    Column {
                        width: 330
                        spacing: 6
                        Text { text: qsTr("Model Name"); color: root.primaryText; font: root.labelFont }
                        Rectangle { width: parent.width; height: 44; radius: 4; color: root.surface; border.color: root.borderDefault; Text { text: qsTr("DropletNet-04"); color: root.primaryText; font: root.bodyFont; anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter } }
                    }

                    Column {
                        width: 330
                        spacing: 6
                        Text { text: qsTr("Architecture"); color: root.primaryText; font: root.labelFont }
                        Rectangle { width: parent.width; height: 44; radius: 4; color: root.surface; border.color: root.brand; border.width: 2; Text { text: qsTr("MobileNet  ·  Faster"); color: root.primaryText; font: root.bodyFont; anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter } Text { text: qsTr("⌄"); color: root.brand; font: root.bodyFont; anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter } }
                    }

                    Column {
                        width: 330
                        spacing: 6
                        Text { text: qsTr("Frequency (kHz)"); color: root.primaryText; font: root.labelFont }
                        Rectangle { width: parent.width; height: 44; radius: 4; color: root.surface; border.color: root.borderDefault; Text { text: qsTr("−"); font: root.bodyFont; anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter } Text { text: qsTr("10"); font: root.bodyFont; anchors.centerIn: parent } Text { text: qsTr("+"); font: root.bodyFont; anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter } }
                    }

                    Column {
                        width: 330
                        spacing: 6
                        Text { text: qsTr("Save Location"); color: root.primaryText; font: root.labelFont }
                        Rectangle { width: parent.width; height: 44; radius: 4; color: root.disabled; border.color: root.borderSubtle; Text { text: qsTr("C:/OpenDSS/Runs"); color: root.secondaryText; font: root.bodyFont; anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter } }
                    }
                }

                Row {
                    spacing: 48

                    Row {
                        spacing: 10
                        Rectangle { width: 26; height: 26; radius: 3; color: root.surface; border.color: root.brand; border.width: 2; Text { text: qsTr("✓"); color: root.brand; font: root.bodyFont; anchors.centerIn: parent } }
                        Text { text: qsTr("Physical DAQ Output"); color: root.primaryText; font: root.bodyFont; anchors.verticalCenter: parent.verticalCenter }
                    }

                    Row {
                        spacing: 10
                        Rectangle { width: 52; height: 28; radius: 14; color: root.brand; Rectangle { width: 22; height: 22; radius: 11; color: root.surface; anchors.right: parent.right; anchors.rightMargin: 3; anchors.verticalCenter: parent.verticalCenter } }
                        Text { text: qsTr("Trigger Every Droplet"); color: root.primaryText; font: root.bodyFont; anchors.verticalCenter: parent.verticalCenter }
                    }

                    Row {
                        spacing: 10
                        Rectangle { width: 52; height: 28; radius: 14; color: root.brand; Rectangle { width: 22; height: 22; radius: 11; color: root.surface; anchors.right: parent.right; anchors.rightMargin: 3; anchors.verticalCenter: parent.verticalCenter } }
                        Text { text: qsTr("DAQ Output"); color: root.primaryText; font: root.bodyFont; anchors.verticalCenter: parent.verticalCenter }
                    }

                    Row {
                        spacing: 10
                        Rectangle { width: 52; height: 28; radius: 14; color: root.disabled; border.color: root.borderSubtle; Rectangle { width: 22; height: 22; radius: 11; color: "#FFFFFF"; anchors.left: parent.left; anchors.leftMargin: 3; anchors.verticalCenter: parent.verticalCenter } }
                        Text { text: qsTr("Disabled switch"); color: "#6B7785"; font: root.bodyFont; anchors.verticalCenter: parent.verticalCenter }
                    }
                }

                Row {
                    spacing: 24
                    DesignState { sampleText: qsTr("Ready"); label: qsTr("Rest"); fillColor: root.surface }
                    DesignState { sampleText: qsTr("Focused"); label: qsTr("2 px focus"); fillColor: root.surface; outlineColor: root.brand; outlineWidth: 2 }
                    DesignState { sampleText: qsTr("Read-only"); label: qsTr("Information"); fillColor: root.disabled; textColor: root.secondaryText }
                    DesignState { sampleText: qsTr("Required"); label: qsTr("Error ×"); fillColor: "#FDECEC"; textColor: "#B42318"; outlineColor: "#B42318"; outlineWidth: 2 }
                    DesignState { sampleText: qsTr("Unavailable"); label: qsTr("Disabled"); fillColor: root.disabled; textColor: "#6B7785" }
                }
            }
        }

        DesignTile {
            id: structureTile

            width: parent.width
            componentName: qsTr("Structure · card, accordion, tabs, navigation, workspace inspector, and Hardware panel")
            purpose: qsTr("Shared structural hierarchy without changing the approved shell.")
            anatomy: qsTr("Title strip, disclosure indicator, body surface, scrolling region, stationary rail, and optional action footer.")
            variants: qsTr("Expanded, collapsed, selected, disabled, constrained width, and overflow.")
            stateList: qsTr("Rest, hover, focus, selected, expanded, collapsed, and unavailable.")
            dimensions: qsTr("36 px disclosure header; fixed 28 × 36 px outer toggle with 14 px chevron; approximately 536 px workspace-panel default at 100%.")
            accessibilityNote: qsTr("The outer toggle stays vertically centered, fixed-size, focusable, and clear of panel content.")
            contentHeight: 620

            Row {
                width: parent.width
                spacing: 20

                Rectangle {
                    width: 300
                    height: 580
                    color: root.subtle
                    border.color: root.borderSubtle

                    Column {
                        x: 12
                        y: 12
                        width: parent.width - 24
                        spacing: 6
                        Text { text: qsTr("Data"); color: root.primaryText; font: root.sectionFont }
                        DesignState { width: parent.width; sampleText: qsTr("Capture"); label: qsTr("Selected"); fillColor: "#E7F0F8"; showMarker: true }
                        DesignState { width: parent.width; sampleText: qsTr("Label"); label: qsTr("Rest") }
                        DesignState { width: parent.width; sampleText: qsTr("Sequence Viewer"); label: qsTr("Focus"); outlineColor: root.brand; outlineWidth: 2 }
                        Text { text: qsTr("Models"); color: root.primaryText; font: root.sectionFont }
                        DesignState { width: parent.width; sampleText: qsTr("Train"); label: qsTr("Rest") }
                        DesignState { width: parent.width; sampleText: qsTr("Model Test"); label: qsTr("Rest") }
                    }

                    Rectangle {
                        width: parent.width
                        height: 142
                        color: root.surface
                        border.color: root.borderDefault
                        anchors.bottom: parent.bottom

                        Text { text: qsTr("Hardware Configuration"); color: root.primaryText; font: root.sectionFont; x: 12; y: 10 }
                        Text { text: qsTr("›"); color: root.primaryText; font: root.bodyFont; anchors.right: parent.right; anchors.rightMargin: 12; y: 12 }
                        Text { text: qsTr("⌄  Camera"); color: root.primaryText; font: root.bodyFont; x: 12; y: 50 }
                        Text { text: qsTr("⌄  DAQ"); color: root.primaryText; font: root.bodyFont; x: 12; y: 86 }
                    }
                }

                Rectangle {
                    width: 1020
                    height: 580
                    color: root.surface
                    border.color: root.borderSubtle

                    Text { text: qsTr("Train"); color: root.primaryText; font: root.titleFont; x: 24; y: 20 }
                    Rectangle { x: 24; y: 76; width: 610; height: 450; radius: 6; color: root.subtle; border.color: root.borderSubtle; Text { text: qsTr("Workspace"); color: root.secondaryText; font: root.sectionFont; anchors.centerIn: parent } }

                    Rectangle {
                        x: 652
                        y: 20
                        width: 338
                        height: 536
                        color: root.surface
                        border.color: root.borderDefault

                        Rectangle { width: parent.width; height: 50; color: root.subtle; border.color: root.borderSubtle; Text { text: qsTr("Train"); color: root.primaryText; font: root.sectionFont; x: 14; anchors.verticalCenter: parent.verticalCenter } }
                        Text { text: qsTr("⌄  Training Setup"); color: root.primaryText; font: root.sectionFont; x: 14; y: 74 }
                        Text { text: qsTr("⌄  Readiness"); color: root.primaryText; font: root.sectionFont; x: 14; y: 126 }
                        Rectangle { x: 14; y: 190; width: parent.width - 28; height: 44; radius: 4; color: root.brand; Text { text: qsTr("Start Training"); color: "#FFFFFF"; font: root.bodyFont; anchors.centerIn: parent } }
                    }

                    Rectangle {
                        x: 962
                        y: 270
                        width: 28
                        height: 36
                        color: root.subtle
                        border.color: root.borderDefault
                        Text { text: qsTr("›"); color: root.brand; font.family: "Segoe UI Variable"; font.pixelSize: 14; font.weight: Font.DemiBold; anchors.centerIn: parent }
                    }
                }
            }
        }

        DesignTile {
            width: parent.width
            componentName: qsTr("Feedback · status, inline message, empty state, loading, dialog, and tooltip")
            purpose: qsTr("Consistent technical feedback with direct wording and visible recovery.")
            anatomy: qsTr("Semantic icon, concise label, direct reason, optional recovery action, and stable progress region.")
            variants: qsTr("Information, success, warning, error, neutral, confirmation, determinate, and indeterminate.")
            stateList: qsTr("Static, loading, completed, paused, failed, and unavailable.")
            dimensions: qsTr("Content-driven; messages and dialogs grow with wrapped text.")
            accessibilityNote: qsTr("Initial dialog focus is explicit; every status has a non-color cue.")
            contentHeight: 360

            Column {
                width: parent.width
                spacing: 18

                Row {
                    spacing: 12
                    DesignState { sampleText: qsTr("✓ DAQ Ready"); label: qsTr("AppStatusBadge"); fillColor: "#EAF6F0"; textColor: "#1F7A55"; outlineColor: "#1F7A55" }
                    DesignState { sampleText: qsTr("! Camera unavailable"); label: qsTr("Warning"); fillColor: "#FFF4D8"; textColor: "#8A5A00"; outlineColor: "#8A5A00" }
                    DesignState { sampleText: qsTr("× Save failed"); label: qsTr("Error"); fillColor: "#FDECEC"; textColor: "#B42318"; outlineColor: "#B42318" }
                    DesignState { sampleText: qsTr("i No Active Model"); label: qsTr("Information"); fillColor: "#E7F0F8"; textColor: "#2563A6"; outlineColor: "#2563A6" }
                    DesignState { sampleText: qsTr("Processing…"); label: qsTr("Loading"); fillColor: root.subtle; outlineColor: root.brand; outlineWidth: 2 }
                }

                Row {
                    spacing: 24

                    Rectangle {
                        width: 520
                        height: 170
                        radius: 8
                        color: root.surface
                        border.color: root.borderDefault
                        Text { text: qsTr("Camera unavailable. Continue?"); color: root.primaryText; font: root.sectionFont; x: 18; y: 18 }
                        Text { text: qsTr("You can continue without live capture."); color: root.secondaryText; font: root.bodyFont; x: 18; y: 58 }
                        Rectangle { x: 292; y: 108; width: 96; height: 42; radius: 4; color: root.surface; border.color: root.borderDefault; Text { text: qsTr("Cancel"); color: root.primaryText; font: root.bodyFont; anchors.centerIn: parent } }
                        Rectangle { x: 402; y: 108; width: 96; height: 42; radius: 4; color: root.brand; Text { text: qsTr("Continue"); color: "#FFFFFF"; font: root.bodyFont; anchors.centerIn: parent } }
                    }

                    Rectangle {
                        width: 520
                        height: 170
                        radius: 6
                        color: root.subtle
                        border.color: root.borderSubtle
                        Text { text: qsTr("No Dataset selected"); color: root.primaryText; font: root.sectionFont; anchors.horizontalCenter: parent.horizontalCenter; y: 42 }
                        Text { text: qsTr("Open a Dataset to review its summary."); color: root.secondaryText; font: root.bodyFont; anchors.horizontalCenter: parent.horizontalCenter; y: 82 }
                    }

                    Rectangle {
                        width: 520
                        height: 170
                        radius: 6
                        color: root.surface
                        border.color: root.borderSubtle
                        Text { text: qsTr("Processing 360 of 1,200"); color: root.primaryText; font: root.bodyFont; x: 18; y: 26 }
                        Rectangle { x: 18; y: 74; width: 484; height: 14; radius: 7; color: root.disabled; Rectangle { width: parent.width * 0.3; height: parent.height; radius: 7; color: root.brand } }
                        Text { text: qsTr("30%"); color: root.secondaryText; font: root.captionFont; x: 18; y: 104 }
                    }
                }
            }
        }

        DesignTile {
            width: parent.width
            componentName: qsTr("OpenDSS compositions · property grid, readiness, sticky footer, transport, chart, and Class selector")
            purpose: qsTr("Product-specific compositions built from the same visual language.")
            anatomy: qsTr("Shared controls arranged without redefining their tokens or interaction states.")
            variants: qsTr("Ready, disabled, empty, loaded, two-class, three-class, and dark-canvas.")
            stateList: qsTr("Representative accepted visual states only; no application behavior is implemented here.")
            dimensions: qsTr("Workspace-specific compositions grow and reflow with content.")
            accessibilityNote: qsTr("Reading order matches visual order; direct frame entry follows the transport row.")
            contentHeight: 620

            Column {
                width: parent.width
                spacing: 22

                Row {
                    spacing: 24

                    Rectangle {
                        width: 520
                        height: 246
                        radius: 6
                        color: root.surface
                        border.color: root.borderSubtle
                        Text { text: qsTr("Output Configuration"); color: root.primaryText; font: root.sectionFont; x: 16; y: 14 }
                        Text { text: qsTr("Output Channel"); color: root.primaryText; font: root.bodyFont; x: 16; y: 58 }
                        Text { text: qsTr("ao0"); color: root.secondaryText; font: root.bodyFont; x: 310; y: 58 }
                        Text { text: qsTr("Amplitude (Vpp)"); color: root.primaryText; font: root.bodyFont; x: 16; y: 98 }
                        Text { text: qsTr("5"); color: root.secondaryText; font: root.bodyFont; x: 310; y: 98 }
                        Text { text: qsTr("Event Duration (ms)"); color: root.primaryText; font: root.bodyFont; x: 16; y: 138 }
                        Text { text: qsTr("5"); color: root.secondaryText; font: root.bodyFont; x: 310; y: 138 }
                        Rectangle { x: 16; y: 184; width: parent.width - 32; height: 44; radius: 4; color: root.brand; Text { text: qsTr("Start Sine Wave"); color: "#FFFFFF"; font: root.bodyFont; anchors.centerIn: parent } }
                    }

                    Rectangle {
                        width: 520
                        height: 246
                        radius: 6
                        color: root.surface
                        border.color: root.borderSubtle
                        Text { text: qsTr("Readiness"); color: root.primaryText; font: root.sectionFont; x: 16; y: 14 }
                        Text { text: qsTr("✓  Dataset — Ready"); color: "#1F7A55"; font: root.bodyFont; x: 16; y: 62 }
                        Text { text: qsTr("✓  Active Model — Ready"); color: "#1F7A55"; font: root.bodyFont; x: 16; y: 102 }
                        Text { text: qsTr("!  DAQ — Unavailable"); color: "#8A5A00"; font: root.bodyFont; x: 16; y: 142 }
                        Text { text: qsTr("Physical output is disabled."); color: root.secondaryText; font: root.captionFont; x: 44; y: 176 }
                    }

                    Rectangle {
                        width: 520
                        height: 246
                        radius: 6
                        color: root.subtle
                        border.color: root.borderSubtle
                        Text { text: qsTr("Class selector"); color: root.primaryText; font: root.sectionFont; x: 16; y: 14 }
                        Row {
                            x: 16
                            y: 62
                            spacing: 12
                            DesignState { width: 146; sampleText: qsTr("Class 0"); label: qsTr("Blue · 0"); fillColor: "#E0F2FE"; textColor: "#075985"; outlineColor: "#075985" }
                            DesignState { width: 146; sampleText: qsTr("Class 1"); label: qsTr("Orange · 1"); fillColor: "#FFEDD5"; textColor: "#9A3412"; outlineColor: "#9A3412" }
                            DesignState { width: 146; sampleText: qsTr("Class 2"); label: qsTr("Disabled"); fillColor: root.disabled; textColor: "#6B7785"; outlineColor: root.borderSubtle }
                        }
                        Text { text: qsTr("Exclude · Undo · Previous · Next"); color: root.secondaryText; font: root.bodyFont; x: 16; y: 174 }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 300
                    radius: 6
                    color: root.canvas

                    Text { text: qsTr("Sequence Viewer · viewer canvas dark surface"); color: "#FFFFFF"; font: root.sectionFont; x: 20; y: 18 }
                    Rectangle { x: 20; y: 62; width: parent.width - 40; height: 112; radius: 4; color: "#1C3552"; border.color: "#5CA9E6"; border.width: 2; Text { text: qsTr("Selected frame preview"); color: "#FFFFFF"; font: root.bodyFont; anchors.centerIn: parent } }

                    Row {
                        x: 20
                        y: 194
                        spacing: 10
                        DesignState { width: 104; height: 64; sampleText: qsTr("−50"); label: qsTr("Jump"); fillColor: "#1C3552"; textColor: "#FFFFFF"; outlineColor: "#5CA9E6" }
                        DesignState { width: 104; height: 64; sampleText: qsTr("−10"); label: qsTr("Jump"); fillColor: "#1C3552"; textColor: "#FFFFFF"; outlineColor: "#5CA9E6" }
                        DesignState { width: 130; height: 64; sampleText: qsTr("Previous"); label: qsTr("Transport"); fillColor: "#1C3552"; textColor: "#FFFFFF"; outlineColor: "#5CA9E6" }
                        DesignState { width: 130; height: 64; sampleText: qsTr("Next"); label: qsTr("Focus"); fillColor: "#1C3552"; textColor: "#FFFFFF"; outlineColor: "#FFFFFF"; outlineWidth: 3 }
                        DesignState { width: 104; height: 64; sampleText: qsTr("+10"); label: qsTr("Jump"); fillColor: "#1C3552"; textColor: "#FFFFFF"; outlineColor: "#5CA9E6" }
                        DesignState { width: 104; height: 64; sampleText: qsTr("+50"); label: qsTr("Jump"); fillColor: "#1C3552"; textColor: "#FFFFFF"; outlineColor: "#5CA9E6" }
                        Rectangle { width: 390; height: 14; radius: 7; color: "#334A63"; anchors.verticalCenter: parent.verticalCenter; Rectangle { width: 140; height: parent.height; radius: 7; color: "#5CA9E6" } }
                    }

                    Text { text: qsTr("Go to frame"); color: "#FFFFFF"; font: root.bodyFont; x: 20; y: 270 }
                    Rectangle { x: 150; y: 264; width: 150; height: 30; radius: 4; color: "#FFFFFF"; border.color: "#5CA9E6"; Text { text: qsTr("360"); color: root.primaryText; font: root.bodyFont; anchors.centerIn: parent } }
                    Text { text: qsTr("of 1,200"); color: "#D7E5F2"; font: root.bodyFont; x: 318; y: 270 }
                }
            }
        }

        DesignTile {
            id: textSizeProofTile

            width: parent.width
            componentName: qsTr("Scale and reflow proof")
            purpose: qsTr("Text-bearing components grow with content instead of clipping.")
            anatomy: qsTr("Label, control, supporting line, wrap point, and independent scrolling region.")
            variants: qsTr("Small (80%), Medium (100%), Large (125%), and separate 200% validation-only proof.")
            stateList: qsTr("Ready and focused.")
            dimensions: qsTr("No fixed text-bearing height at enlarged Text Size.")
            accessibilityNote: qsTr("Focused controls remain visible and are not covered by sticky actions.")
            contentHeight: 650

            Column {
                width: parent.width
                spacing: 24

                Row {
                    spacing: 20

                    Rectangle {
                        width: 520
                        height: 250
                        radius: 6
                        color: root.surface
                        border.color: root.borderSubtle
                        Text { text: qsTr("Small (80%)"); color: root.primaryText; font.family: "Segoe UI Variable"; font.pixelSize: 13; font.weight: Font.DemiBold; x: 16; y: 16 }
                        Text { text: qsTr("Decision-to-trigger Delay (ms)"); color: root.primaryText; font.family: "Segoe UI Variable"; font.pixelSize: 12; x: 16; y: 64; width: 488; wrapMode: Text.WordWrap }
                        Rectangle { x: 16; y: 102; width: 488; height: 42; radius: 4; color: root.surface; border.color: root.brand; border.width: 2; Text { text: qsTr("0"); color: root.primaryText; font.family: "Segoe UI Variable"; font.pixelSize: 13; anchors.centerIn: parent } }
                        Text { text: qsTr("Camera unavailable — restore Hardware Configuration to continue."); color: "#8A5A00"; font.family: "Segoe UI Variable"; font.pixelSize: 10; x: 16; y: 166; width: 488; wrapMode: Text.WordWrap }
                    }

                    Rectangle {
                        width: 520
                        height: 250
                        radius: 6
                        color: root.surface
                        border.color: root.brand
                        border.width: 2
                        Text { text: qsTr("Medium (100%) · Default"); color: root.primaryText; font: root.sectionFont; x: 16; y: 16 }
                        Text { text: qsTr("Decision-to-trigger Delay (ms)"); color: root.primaryText; font: root.labelFont; x: 16; y: 64; width: 488; wrapMode: Text.WordWrap }
                        Rectangle { x: 16; y: 102; width: 488; height: 44; radius: 4; color: root.surface; border.color: root.brand; border.width: 2; Text { text: qsTr("0"); color: root.primaryText; font: root.bodyFont; anchors.centerIn: parent } }
                        Text { text: qsTr("Camera unavailable — restore Hardware Configuration to continue."); color: "#8A5A00"; font: root.captionFont; x: 16; y: 166; width: 488; wrapMode: Text.WordWrap }
                    }

                    Rectangle {
                        width: 520
                        height: 250
                        radius: 6
                        color: root.surface
                        border.color: root.borderSubtle
                        Text { text: qsTr("Large (125%)"); color: root.primaryText; font.family: "Segoe UI Variable"; font.pixelSize: 20; font.weight: Font.DemiBold; x: 16; y: 14 }
                        Text { text: qsTr("Decision-to-trigger Delay (ms)"); color: root.primaryText; font.family: "Segoe UI Variable"; font.pixelSize: 19; x: 16; y: 62; width: 488; wrapMode: Text.WordWrap }
                        Rectangle { x: 16; y: 110; width: 488; height: 54; radius: 4; color: root.surface; border.color: root.brand; border.width: 2; Text { text: qsTr("0"); color: root.primaryText; font.family: "Segoe UI Variable"; font.pixelSize: 20; anchors.centerIn: parent } }
                        Text { text: qsTr("Camera unavailable — restore Hardware Configuration to continue."); color: "#8A5A00"; font.family: "Segoe UI Variable"; font.pixelSize: 16; x: 16; y: 184; width: 488; wrapMode: Text.WordWrap }
                    }
                }

                Rectangle {
                    width: 1600
                    height: 340
                    radius: 6
                    color: root.surface
                    border.color: root.borderDefault

                    Text { text: qsTr("200% validation-only reflow proof"); color: root.primaryText; font.family: "Segoe UI Variable"; font.pixelSize: 32; font.weight: Font.DemiBold; x: 20; y: 18 }
                    Text { text: qsTr("Not shown in the application Text Size dropdown"); color: root.secondaryText; font.family: "Segoe UI Variable"; font.pixelSize: 26; x: 20; y: 68 }
                    Text { text: qsTr("Decision-to-trigger Delay (ms)"); color: root.primaryText; font.family: "Segoe UI Variable"; font.pixelSize: 30; x: 20; y: 124; width: 760; wrapMode: Text.WordWrap }
                    Rectangle { x: 20; y: 188; width: 760; height: 76; radius: 4; color: root.surface; border.color: root.brand; border.width: 3; Text { text: qsTr("0"); color: root.primaryText; font.family: "Segoe UI Variable"; font.pixelSize: 32; anchors.centerIn: parent } }
                    Text { text: qsTr("Camera unavailable — restore Hardware Configuration to continue."); color: "#8A5A00"; font.family: "Segoe UI Variable"; font.pixelSize: 26; x: 820; y: 124; width: 740; wrapMode: Text.WordWrap }
                }
            }
        }
    }
}
