pragma Singleton
import QtQuick

QtObject {
    readonly property int width: 1600
    readonly property int height: 900

    /* Edit this comment to add your custom font */
    readonly property font font: Qt.font({
                                             family: Qt.application.font.family,
                                             pixelSize: Qt.application.font.pixelSize
                                         })
    readonly property font largeFont: Qt.font({
                                                  family: Qt.application.font.family,
                                                  pixelSize: Qt.application.font.pixelSize * 1.6
                                              })

    readonly property int shellHeaderHeight: 54
    readonly property int navigationWidth: 208
    readonly property int operationPanelWidth: 400
    readonly property int drawerWidth: 440
    readonly property int controlHeight: 40
    readonly property int spacing: 10

    readonly property color backgroundColor: "#f4f5f7"
    readonly property color surfaceColor: "#ffffff"
    readonly property color borderColor: "#c9cdd3"
    readonly property color textColor: "#1f2328"
    readonly property color mutedTextColor: "#5d6773"
    readonly property color viewerColor: "#20252b"
    readonly property color accentColor: "#2b6cb0"
    readonly property color warningColor: "#9a6700"
    readonly property color faultColor: "#b42318"
}
