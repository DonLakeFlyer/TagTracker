/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QGroundControl               1.0
import QGroundControl.FlightDisplay 1.0

GuidedToolStripAction {
    text:       customController.rawCaptureTitle
    iconSource: "/res/action.svg"
    visible:    true
    enabled:    QGroundControl.multiVehicleManager.activeVehicle && QGroundControl.corePlugin.controllerStatus == CustomPlugin.ControllerStatusHasTags
    actionID:   customController.actionRawCapture

    property var customController: _guidedController._customController
}
