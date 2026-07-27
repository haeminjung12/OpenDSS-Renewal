pragma Singleton
import QtQuick

QtObject {
    readonly property int width: 1600
    readonly property int height: 900
    property int textSizePercent: 100
    readonly property real textScale: 1.375 * Math.max(80, Math.min(200, textSizePercent)) / 100
    // Remove the legacy visual tokens after every workspace consumes the approved App* component tokens.
    readonly property real designTextScale: textSizePercent === 80 ? 0.8 : (textSizePercent === 125 ? 1.25 : 1.0)
    readonly property font font: Qt.font({ pointSize: 12 * textScale })
    readonly property font smallFont: Qt.font({ pointSize: 10.5 * textScale })
    readonly property font headingFont: Qt.font({ pointSize: 14.25 * textScale, bold: true })
    readonly property font largeFont: Qt.font({ pointSize: 19.5 * textScale })

    readonly property int shellHeaderHeight: Math.round(44 * textScale)
    readonly property int navigationWidth: Math.round(208 * textScale)
    readonly property int operationPanelWidth: Math.round(390 * textScale)
    readonly property int collapsedOperationPanelWidth: 28
    readonly property int hardwarePanelWidth: Math.round(380 * textScale)
    readonly property int hardwarePanelHeight: 600
    readonly property int controlHeight: Math.round(40 * textScale)
    readonly property int navigationItemHeight: Math.round(36 * textScale)
    readonly property int workspaceMargin: 18
    readonly property int singleImageBodyHeight: 290
    readonly property int compactBodyHeight: 58
    readonly property int spacing: 10
    readonly property int headerItemSpacing: Math.round(22 * textScale)

    readonly property color backgroundColor: "#f4f7fa"
    readonly property color surfaceColor: "#ffffff"
    readonly property color borderColor: "#cbd5e1"
    readonly property color textColor: "#172033"
    readonly property color mutedTextColor: "#5f6b7a"
    readonly property color viewerColor: "#17263a"
    readonly property color accentColor: "#245b91"
    readonly property color readyColor: "#267247"
    readonly property color warningColor: "#956300"
    readonly property color faultColor: "#b42318"
    readonly property color errorSurfaceColor: "#fff1f0"

    readonly property font appBodyFont: Qt.font({ family: "Segoe UI Variable", pointSize: Math.max(10, 12 * designTextScale) })
    readonly property font appLabelFont: Qt.font({ family: "Segoe UI Variable", pointSize: Math.max(10, 11.25 * designTextScale) })
    readonly property font appButtonFont: Qt.font({ family: "Segoe UI Variable", pointSize: Math.max(10, 12 * designTextScale), weight: Font.DemiBold })
    readonly property font appSectionFont: Qt.font({ family: "Segoe UI Variable", pointSize: Math.max(10, 12 * designTextScale), weight: Font.DemiBold })
    readonly property font appCaptionFont: Qt.font({ family: "Segoe UI Variable", pointSize: Math.max(10, 10 * designTextScale) })
    readonly property font appInspectorRailGlyphFont: Qt.font({ family: "Segoe UI Variable", pointSize: Math.max(10, 10.5 * designTextScale), weight: Font.DemiBold })

    readonly property color appSurfaceColor: "#FFFFFF"
    readonly property color appSubtleSurfaceColor: "#F8FAFC"
    readonly property color appHoverColor: "#EEF3F7"
    readonly property color appDisabledColor: "#EEF3F7"
    readonly property color appPrimaryTextColor: "#17202A"
    readonly property color appSecondaryTextColor: "#5D6978"
    readonly property color appDisabledTextColor: "#6B7785"
    readonly property color appBorderSubtleColor: "#D7DEE7"
    readonly property color appBorderDefaultColor: "#AAB7C5"
    readonly property color appFocusColor: "#276DA3"
    readonly property color appPrimaryColor: "#276DA3"
    readonly property color appPrimaryHoverColor: "#236A9D"
    readonly property color appPrimaryPressedColor: "#1E5D8F"
    readonly property color appErrorColor: "#B42318"
    readonly property color appClass0Color: "#276DA3"
    readonly property color appClass1Color: "#D46A18"
    readonly property color appClass2Color: "#7652B8"
    readonly property color appInverseTextColor: "#FFFFFF"

    readonly property int appStandardControlHeight: Math.round(32 * designTextScale)
    readonly property int appPrimaryButtonHeight: Math.round(36 * designTextScale)
    readonly property int appAccordionHeaderHeight: Math.round(36 * designTextScale)
    readonly property int appControlHorizontalPadding: Math.round(12 * designTextScale)
    readonly property int appControlRadius: 4
}
