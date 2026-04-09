/****************************************************************************
 *
 *   (c) 2009-2016 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick          2.3
import QtQuick.Layouts  1.15
import QtQuick.Controls 2.15
import QtPositioning    5.15
import QtLocation       5.15

import QGroundControl               1.0
import QGroundControl.Palette       1.0
import QGroundControl.ScreenTools   1.0
import QGroundControl.Controls      1.0

MapQuickItem {
    coordinate:     customMapObject.rotationCenter
    anchorPoint.x:  mapRect.width / 2
    anchorPoint.y:  mapRect.height / 2

    property var customMapObject

    property var    _activeVehicle:     QGroundControl.multiVehicleManager.activeVehicle
    property var    _flightMap:         parent
    property var    _corePlugin:        QGroundControl.corePlugin
    property var    _vhfSettings:       _corePlugin.customSettings
    property int    _rotationIndex:     customMapObject.rotationIndex
    property var    _rotationInfo:      _corePlugin.rotationInfoList.get(_rotationIndex)
    property int    _sliceCount:        _rotationInfo.slices.count
    property bool   _isLastRotation:    _rotationIndex == _corePlugin.rotationInfoList.count - 1
    property real   _sliceSize:         360.0 / _sliceCount
    property real   _ratio:             _isLastRotation ? _largeRatio : _smallRatio

    readonly property real _largeRatio: 0.5
    readonly property real _smallRatio: 0.3

    sourceItem: Rectangle {
        id:             mapRect
        width:          _flightMap.height * _ratio
        height:         width
        radius:         width / 2
        color:          "transparent"
        border.color:   mapPal.text
        border.width:   1

        Repeater {
            model: _sliceCount

            Canvas {
                id:             arcCanvas
                anchors.fill:   parent
                visible:        !isNaN(strengthRatio)

                property real centerX:          width / 2
                property real centerY:          height / 2
                property real arcRadians:       (Math.PI * 2) / _sliceCount
                property var  sliceInfo:        _rotationInfo.slices.get(index)
                property real rawStrengthRatio: _rotationInfo.maxSNR > 0 ? sliceInfo.displaySNR / _rotationInfo.maxSNR : 0
                property bool noDetection:      rawStrengthRatio == 0
                property bool lowConfidenceOnly: sliceInfo.lowConfidenceOnly
                property real strengthRatio:    noDetection ? 1 : rawStrengthRatio

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();

                    var hasDetection = !noDetection;
                    var sliceColor = noDetection ? "white" : (lowConfidenceOnly ? "orange" : "green");

                    ctx.beginPath();
                    ctx.globalAlpha = 0.75;
                    ctx.fillStyle = sliceColor;
                    ctx.strokeStyle = "white";
                    ctx.lineWidth = 1;
                    ctx.moveTo(centerX, centerY);
                    ctx.arc(centerX, centerY, (width / 2) * arcCanvas.strengthRatio, 0, arcRadians, false);
                    ctx.lineTo(centerX, centerY);

                    if (hasDetection) {
                        ctx.fill();
                    }
                    ctx.stroke();
                }

                transform: Rotation {
                    origin.x:   arcCanvas.centerX
                    origin.y:   arcCanvas.centerY
                    angle:      -90 - (360 / _sliceCount / 2) + ((360 / _sliceCount) * index)
                }

                Connections {
                    target: arcCanvas.sliceInfo

                    function onMaxSNRChanged(maxSNR) { arcCanvas.requestPaint() }
                    function onDisplaySNRChanged(displaySNR) { arcCanvas.requestPaint() }
                    function onDisplaySourceChanged(displaySource) { arcCanvas.requestPaint() }
                    function onLowConfidenceOnlyChanged(lowConfidenceOnly) { arcCanvas.requestPaint() }
                }
                Connections {
                    target: _rotationInfo

                    function onMaxSNRChanged(maxSNR) { arcCanvas.requestPaint() }
                }
            }
        }

        Repeater {
            model: _sliceCount

            Text {
                property var  sliceInfo:        _rotationInfo.slices.get(index)
                property real rawStrengthRatio: _rotationInfo.maxSNR > 0 ? sliceInfo.displaySNR / _rotationInfo.maxSNR : 0
                property bool noDetection:      rawStrengthRatio == 0
                property bool lowConfidenceOnly: sliceInfo.lowConfidenceOnly
                property string displaySource:  sliceInfo.displaySource
                property real sliceAngleDeg:    -90 + (360 / _sliceCount) * index
                property real sliceAngleRad:    sliceAngleDeg * Math.PI / 180
                property real outerRadius:      mapRect.width / 2

                x:              mapRect.width / 2 + outerRadius * Math.cos(sliceAngleRad) - width / 2
                y:              mapRect.height / 2 + outerRadius * Math.sin(sliceAngleRad) - height / 2
                visible:        !noDetection && sliceInfo.displaySNR > 0
                text:           sliceInfo.displaySNR.toFixed(1) + " " + displaySource
                font.pointSize: ScreenTools.largeFontPointSize
                font.bold:      true
                color:          lowConfidenceOnly ? "orange" : "white"
                style:          Text.Outline
                styleColor:     "black"
            }
        }

        // Bearing arrow overlay
        Canvas {
            id:             bearingArrowCanvas
            anchors.fill:   parent
            visible:        _rotationInfo.bearingValid

            property real centerX:      width / 2
            property real centerY:      height / 2
            property real arrowRadius:  width / 2
            property real bearingDeg:   _rotationInfo.bearingDeg
            property real uncertainty:  _rotationInfo.bearingUncertainty
            property real rSquared:     _rotationInfo.bearingRSquared
            property bool ambiguous:    _rotationInfo.bearingAmbiguous

            property color arrowColor:  rSquared > 0.85 ? "#00e000" : (rSquared > 0.6 ? "#ffcc00" : "#ff3333")

            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();

                // Convert bearing to canvas angle (0° = north = up, clockwise positive)
                var bearingRad = (bearingDeg - 90) * Math.PI / 180;

                // Draw uncertainty arc
                if (!isNaN(uncertainty) && uncertainty > 0) {
                    var uncertRad = uncertainty * Math.PI / 180;
                    var arcStart = bearingRad - uncertRad;
                    var arcEnd   = bearingRad + uncertRad;

                    ctx.beginPath();
                    ctx.globalAlpha = 0.25;
                    ctx.fillStyle = arrowColor;
                    ctx.moveTo(centerX, centerY);
                    ctx.arc(centerX, centerY, arrowRadius * 0.95, arcStart, arcEnd, false);
                    ctx.lineTo(centerX, centerY);
                    ctx.fill();
                }

                // Draw bearing arrow line
                var tipX = centerX + arrowRadius * Math.cos(bearingRad);
                var tipY = centerY + arrowRadius * Math.sin(bearingRad);

                ctx.beginPath();
                ctx.globalAlpha = 1.0;
                ctx.strokeStyle = arrowColor;
                ctx.lineWidth = 3;
                ctx.moveTo(centerX, centerY);
                ctx.lineTo(tipX, tipY);
                ctx.stroke();

                // Draw arrowhead
                var headLen = arrowRadius * 0.12;
                var headAngle = 0.4;
                ctx.beginPath();
                ctx.fillStyle = arrowColor;
                ctx.moveTo(tipX, tipY);
                ctx.lineTo(tipX - headLen * Math.cos(bearingRad - headAngle),
                           tipY - headLen * Math.sin(bearingRad - headAngle));
                ctx.lineTo(tipX - headLen * Math.cos(bearingRad + headAngle),
                           tipY - headLen * Math.sin(bearingRad + headAngle));
                ctx.closePath();
                ctx.fill();
            }

            Connections {
                target: _rotationInfo
                function onBearingChanged() { bearingArrowCanvas.requestPaint() }
            }
        }

        // Bearing text label
        Text {
            anchors.horizontalCenter:   parent.horizontalCenter
            anchors.bottom:             parent.bottom
            anchors.bottomMargin:       -ScreenTools.defaultFontPixelHeight * 2
            visible:                    _rotationInfo.bearingValid
            text: {
                var label = "BRG " + _rotationInfo.bearingDeg.toFixed(0) + "°";
                label += "  R² " + _rotationInfo.bearingRSquared.toFixed(2);
                if (_rotationInfo.bearingAmbiguous) {
                    label += "  AMB";
                }
                return label;
            }
            font.pointSize: ScreenTools.largeFontPointSize
            font.bold:      true
            color:          _rotationInfo.bearingRSquared > 0.85 ? "#00e000" : (_rotationInfo.bearingRSquared > 0.6 ? "#ffcc00" : "#ff3333")
            style:          Text.Outline
            styleColor:     "black"
        }
    }


    QGCMapPalette { id: mapPal; lightColors: _flightMap.isSatelliteMap }
}
