/****************************************************************************
 *
 *   (c) 2009-2016 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtPositioning
import QtLocation
import QGroundControl
import QGroundControl.Controls
import QGroundControl.CustomControls

MapQuickItem {
    coordinate:     customMapObject.rotationCenter
    anchorPoint.x:  mapRect.width / 2
    anchorPoint.y:  mapRect.height / 2

    property var customMapObject

    property var    _activeVehicle:     QGroundControl.multiVehicleManager.activeVehicle
    property var    _flightMap:         parent
    property var    _corePlugin:        QGroundControl.corePlugin
    property var    _vhfSettings:       QGroundControl.settingsManager.customSettings
    property int    _rotationIndex:     customMapObject.rotationIndex
    property var    _rotationInfo:      _corePlugin.rotationInfoList.get(_rotationIndex)
    property bool   _isLastRotation:    _rotationIndex == _corePlugin.rotationInfoList.count - 1
    property real   _ratio:             _isLastRotation ? _largeRatio : _smallRatio

    readonly property real _largeRatio: 0.5
    readonly property real _smallRatio: 0.3

    sourceItem: RotationRose {
        id:             mapRect
        width:          _flightMap.height * _ratio
        rotationInfo:   _rotationInfo
        borderColor:    mapPal.text
    }

    QGCMapPalette { id: mapPal; lightColors: _flightMap.isSatelliteMap }
}
