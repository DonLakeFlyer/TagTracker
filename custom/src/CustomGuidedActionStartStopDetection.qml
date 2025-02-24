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

// The action handles:
//  Send Tags
//  Start/Stop Detection
GuidedToolStripAction {
    text:       customController.startDetectionTitle
    iconSource: "/res/action.svg"
    visible:    true
    enabled:    QGroundControl.multiVehicleManager.activeVehicle && actionEnabled
    actionID:   customController.actionStartDetection

    property bool actionEnabled: false
    property var controllerStatus: QGroundControl.corePlugin.controllerStatus
    property var customController: _guidedController._customController

    onControllerStatusChanged: _update(controllerStatus)

    function _update(controllerStatus) {
        switch (controllerStatus) {
        case CustomPlugin.ControllerStatusIdle:
        case CustomPlugin.ControllerStatusHasTags:
            text = customController.startDetectionTitle
            actionEnabled = true
            actionID = customController.actionStartDetection
            break
        case CustomPlugin.ControllerStatusReceivingTags:
        case CustomPlugin.ControllerStatusCapture:
            text = customController.startDetectionTitle
            actionEnabled = false
            break
        case CustomPlugin.ControllerStatusDetecting:
            text = customController.stopDetectionTitle
            actionEnabled = true
            actionID = customController.actionStopDetection
            break
        default:
            actionEnabled = false
            breal
        }
    }
}
