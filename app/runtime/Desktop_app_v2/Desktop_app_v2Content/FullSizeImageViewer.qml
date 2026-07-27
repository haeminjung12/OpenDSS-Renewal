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

    function scrollBy(wheelDelta, horizontal) {
        const extent = horizontal
                ? viewport.contentWidth - viewport.width
                : viewport.contentHeight - viewport.height
        if (wheelDelta === 0 || extent <= 0)
            return false

        if (horizontal)
            viewport.contentX = Math.max(0, Math.min(
                    extent, viewport.contentX - wheelDelta))
        else
            viewport.contentY = Math.max(0, Math.min(
                    extent, viewport.contentY - wheelDelta))
        return true
    }

    WheelHandler {
        target: null
        onWheel: event => {
            const modifiers = event.modifiers
            const wheelDelta = (event.pixelDelta.y !== 0
                                ? event.pixelDelta.y
                                : event.angleDelta.y)
                    * (event.inverted ? -1 : 1)
            if (modifiers === Qt.ControlModifier) {
                root.zoomAt(point.position, wheelDelta)
                event.accepted = wheelDelta !== 0
            } else if (modifiers === Qt.ShiftModifier) {
                event.accepted = root.scrollBy(wheelDelta, true)
            } else if (modifiers === Qt.NoModifier) {
                event.accepted = root.scrollBy(wheelDelta, false)
            } else {
                event.accepted = false
            }
        }
    }
}
