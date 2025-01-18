import QtQuick
import QtQuick.Controls
import QtLocation

import QGroundControl               1.0
import QGroundControl.ScreenTools   1.0
import QGroundControl.Palette       1.0
import QGroundControl.Controls      1.0

MapQuickItem {
    coordinate:     customMapObject.coordinate
    anchorPoint.x:  rootItem.width / 2
    anchorPoint.y:  rootItem.height / 2
    z:              QGroundControl.zOrderWidgets - 1

    property var customMapObject

    property real   _indicatorRadius:       Math.ceil(ScreenTools.defaultFontPixelHeight / 2)
    property real   _labelMargin:           ScreenTools.defaultFontPixelWidth / 2
    property real   _labelRadius:           _indicatorRadius + _labelMargin
    property real   _antennaIndicatorLength: ScreenTools.defaultFontPixelWidth * 3

    function gradientColorBetween(fromColor, toColor, pctInBand) {
        let fromRed = fromColor.r
        let fromGreen = fromColor.g
        let fromBlue = fromColor.b
        let toRed = toColor.r
        let toGreen = toColor.g
        let toBlue = toColor.b
        let red = fromRed + ((toRed - fromRed) * pctInBand)
        let green = fromGreen + ((toGreen - fromGreen) * pctInBand)
        let blue = fromBlue + ((toBlue - fromBlue) * pctInBand)
        return Qt.rgba(red, green, blue, 1)
    }

    function snrColor(snr) {
        let minSNR = QGroundControl.corePlugin.minSNR
        let maxSNR = QGroundControl.corePlugin.maxSNR
        let pctSNR = 1.0
        if (minSNR != maxSNR) {
            pctSNR = (snr - minSNR) / (maxSNR - minSNR)
        }
        if (pctSNR < 0.25) {
            let fromColor = Qt.color("navy")
            let toColor = Qt.color("blue")
            let pctInBand = (pctSNR - 0.0) / 0.25
            return gradientColorBetween(fromColor, toColor, pctInBand)
        } else if (pctSNR < 0.5) {
            let fromColor = Qt.color("blue")
            let toColor = Qt.color("green")
            let pctInBand = (pctSNR - 0.25) / 0.25
            return gradientColorBetween(fromColor, toColor, pctInBand)
        } else if (pctSNR < 0.75) {
            let fromColor = Qt.color("green")
            let toColor = Qt.color("yellow")
            let pctInBand = (pctSNR - 0.5) / 0.25
            return gradientColorBetween(fromColor, toColor, pctInBand)
        } else {
            let fromColor = Qt.color("yellow")
            let toColor = Qt.color("red")
            let pctInBand = (pctSNR - 0.75) / 0.25
            return gradientColorBetween(fromColor, toColor, pctInBand)
        }
    }

    sourceItem: Item {
        id:     rootItem
        width:  labelControl.width
        height: labelControl.height

        // Antenna heading indicator
        Canvas {
            id:     antennaIndicatorCanvas
            x:      parent.width / 2
            y:      0
            width:  _antennaIndicatorLength
            height: rootItem.height
            visible: customMapObject.antennaDegrees != -1

            transform: Rotation {
                origin.x:   0
                origin.y:   antennaIndicatorCanvas.height / 2
                angle:      customMapObject.antennaDegrees - 90
            }

            onPaint: {
                var context = getContext("2d")
                context.beginPath()
                context.moveTo(0, 0)
                context.lineTo(_antennaIndicatorLength, height / 2)
                context.lineTo(0, height)
                context.closePath()
                context.fillStyle = "white"
                context.fill()
            }
        }

        // Tag ID label pluse SNR coloring
        Rectangle {
            id:                     labelControl
            width:                  tagIdLabel.contentWidth + (_labelMargin * 2)
            height:                 width
            radius:                 width / 2
            color:                  snrColor(customMapObject.snr)
            border.color:           "black"

            Connections {
                target: QGroundControl.corePlugin

                onMinSNRChanged: labelControl.color = snrColor(customMapObject.snr)
                onMaxSNRChanged: labelControl.color = snrColor(customMapObject.snr)
            }

            QGCLabel {
                id:                     tagIdLabel
                anchors.centerIn:       parent
                horizontalAlignment:    Text.AlignHCenter
                verticalAlignment:      Text.AlignVCenter
                text:                   customMapObject.tagId
                color:                  "white"
            }
        }
    }
}
