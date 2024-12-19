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
    text:       customController.sendTagsTitle
    iconSource: "/res/action.svg"
    visible:    true
    enabled:    QGroundControl.multiVehicleManager.activeVehicle && actionEnabled
    actionID:   customController.actionSendTags

    property bool actionEnabled: true
    property var controllerStatus: QGroundControl.corePlugin.controllerStatus
    property var customController: _guidedController._customController

    onControllerStatusChanged: _update(controllerStatus)

    function _update(controllerStatus) {
        switch (controllerStatus) {
        case CustomPlugin.ControllerStatusIdle:
        case CustomPlugin.ControllerStatusReceivingTags:
        case CustomPlugin.ControllerStatusHasTags:
            actionEnabled = true
            break
        default:
            actionEnabled = false
        }
    }
}
