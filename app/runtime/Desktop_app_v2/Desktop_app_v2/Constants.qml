pragma Singleton
import QtQuick

QtObject {
    readonly property int width: 1600
    readonly property int height: 900
    property int textSizePercent: 100
    readonly property real textScale: Math.max(80, Math.min(200, textSizePercent)) / 100
    readonly property font font: Qt.font({ family: Qt.application.font.family, pixelSize: Qt.application.font.pixelSize * textScale })
    readonly property font smallFont: Qt.font({ family: Qt.application.font.family, pixelSize: Qt.application.font.pixelSize * 0.88 * textScale })
    readonly property font headingFont: Qt.font({ family: Qt.application.font.family, pixelSize: Qt.application.font.pixelSize * 1.22 * textScale, bold: true })
    readonly property font largeFont: Qt.font({ family: Qt.application.font.family, pixelSize: Qt.application.font.pixelSize * 1.6 * textScale })

    readonly property int shellHeaderHeight: 54
    readonly property int navigationWidth: 208
    readonly property int operationPanelWidth: 390
    readonly property int collapsedOperationPanelWidth: 46
    readonly property int hardwarePanelWidth: 380
    readonly property int hardwarePanelHeight: 600
    readonly property int controlHeight: 38
    readonly property int navigationItemHeight: 32
    readonly property int workspaceMargin: 18
    readonly property int singleImageBodyHeight: 290
    readonly property int compactBodyHeight: 58
    readonly property int spacing: 10
    readonly property int headerItemSpacing: 22

    readonly property color backgroundColor: "#f4f5f7"
    readonly property color surfaceColor: "#ffffff"
    readonly property color borderColor: "#c9cdd3"
    readonly property color textColor: "#1f2328"
    readonly property color mutedTextColor: "#5d6773"
    readonly property color viewerColor: "#20252b"
    readonly property color accentColor: "#2b6cb0"
    readonly property color readyColor: "#238636"
    readonly property color warningColor: "#9a6700"
    readonly property color faultColor: "#b42318"
    readonly property color errorSurfaceColor: "#fff2f0"
}
