import QtQuick

FullSizeImageViewerForm {
    id: root

    function zoomAt(position, wheelDelta) {
        if (wheelDelta === 0)
            return

        const oldWidth = image.width
        const oldHeight = image.height
        const imagePositionX = (viewport.contentX + position.x - image.x)
                / oldWidth
        const imagePositionY = (viewport.contentY + position.y - image.y)
                / oldHeight
        const nextScale = Math.max(0.3, Math.min(10,
                zoomScale * Math.pow(1.2, wheelDelta / 120)))

        if (nextScale === zoomScale)
            return

        zoomScale = nextScale

        const nextImageX = Math.max(0, (viewport.width - image.width) / 2)
        const nextImageY = Math.max(0, (viewport.height - image.height) / 2)
        viewport.contentX = Math.max(0, Math.min(
                viewport.contentWidth - viewport.width,
                nextImageX + imagePositionX * image.width - position.x))
        viewport.contentY = Math.max(0, Math.min(
                viewport.contentHeight - viewport.height,
                nextImageY + imagePositionY * image.height - position.y))
    }

    WheelHandler {
        acceptedModifiers: Qt.ControlModifier
        target: null
        onWheel: event => {
            root.zoomAt(point.position, event.angleDelta.y)
            event.accepted = true
        }
    }
}
