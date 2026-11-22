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
    text:       customController.startGridTitle
    iconSource: "/res/action.svg"
    visible:    QGroundControl.settingsManager.customSettings.detectionFlightMode.rawValue === CustomSettings.SurveyDetection
    enabled:    _activeVehicle && _activeVehicle.airborne && _activeVehicle.coordinate.isValid
                && !QGroundControl.corePlugin.rotationInProgress && !QGroundControl.corePlugin.gridInProgress
    actionID:   customController.actionStartGrid

    property var _activeVehicle: QGroundControl.multiVehicleManager.activeVehicle
    property var customController: _guidedController._customController
}
