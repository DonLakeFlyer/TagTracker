/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QGroundControl
import QGroundControl.FlyView
import QGroundControl.CustomControls

GuidedToolStripAction {
    text:       customController.startRotationTitle
    iconSource: "/res/action.svg"
    visible:    QGroundControl.settingsManager.customSettings.detectionFlightMode.rawValue === CustomSettings.ManualRotation
    enabled:    _activeVehicle && _activeVehicle.flying && !QGroundControl.corePlugin.rotationInProgress
    actionID:   customController.actionStartRotation

    property var _activeVehicle: QGroundControl.multiVehicleManager.activeVehicle
    property var customController: _guidedController._customController
}
