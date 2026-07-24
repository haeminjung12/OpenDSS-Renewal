pragma Singleton
import QtQuick

QtObject {
    readonly property int width: 1600
    readonly property int height: 900
    property int textSizePercent: 100
    readonly property real textScale: 1.375 * Math.max(80, Math.min(200, textSizePercent)) / 100
    readonly property font font: Qt.font({ pixelSize: 16 * textScale })
    readonly property font smallFont: Qt.font({ pixelSize: 14 * textScale })
    readonly property font headingFont: Qt.font({ pixelSize: 19 * textScale, bold: true })
    readonly property font largeFont: Qt.font({ pixelSize: 26 * textScale })

    readonly property int shellHeaderHeight: Math.round(44 * textScale)
    readonly property int navigationWidth: Math.round(208 * textScale)
    readonly property int operationPanelWidth: Math.round(390 * textScale)
    readonly property int collapsedOperationPanelWidth: Math.round(46 * textScale)
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
}
