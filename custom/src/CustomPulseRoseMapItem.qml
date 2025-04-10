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
        border.width:   2

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
                property real rawStrengthRatio: sliceInfo.maxSNR / _rotationInfo.maxSNR
                property bool noDetection:      rawStrengthRatio == 0
                property real strengthRatio:    noDetection ? 1 : rawStrengthRatio

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();

                    ctx.beginPath();
                    ctx.globalAlpha = 0.5;
                    ctx.fillStyle = noDetection ? "white" : "red";
                    ctx.strokeStyle = noDetection ? "white" : "red";
                    ctx.lineWidth = noDetection ? 1 : 3;
                    ctx.moveTo(centerX, centerY);
                    ctx.arc(centerX, centerY, (width / 2) * arcCanvas.strengthRatio, 0, arcRadians, false);
                    ctx.lineTo(centerX, centerY);
                    ctx.fill();
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
                }
                Connections {
                    target: _rotationInfo

                    function onMaxSNRChanged(maxSNR) { arcCanvas.requestPaint() }
                }
            }
        }
    }

    
    QGCMapPalette { id: mapPal; lightColors: _flightMap.isSatelliteMap }
}
