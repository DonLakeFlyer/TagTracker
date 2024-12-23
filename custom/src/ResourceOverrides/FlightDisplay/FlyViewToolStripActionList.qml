/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQml.Models 2.12

import QGroundControl               1.0
import QGroundControl.Controls      1.0
import QGroundControl.FlightDisplay 1.0

ToolStripActionList {
    id: _root

    signal displayPreFlightChecklist

    property var customController:  _guidedController._customController
    property var activeVehicle:     QGroundControl.multiVehicleManager.activeVehicle
    property bool controllerIdle:   QGroundControl.corePlugin.controllerStatus === CustomPlugin.ControllerStatusIdle || QGroundControl.corePlugin.controllerStatus === CustomPlugin.ControllerStatusHasLogs

    model: [
        GuidedActionTakeoff { },
        GuidedActionLand { },
        GuidedActionRTL { },
        CustomGuidedActionSendTags { },
        CustomGuidedActionStartStopDetection { },
        CustomGuidedActionStartRotation { },
        CustomGuidedActionRawCapture { },

        GuidedToolStripAction {
            text:       customController.saveLogsTitle
            iconSource: "/res/action.svg"
            visible:    true
            enabled:    activeVehicle && controllerIdle
            actionID:   customController.actionSaveLogs
        },

        GuidedToolStripAction {
            text:       customController.clearLogsTitle
            iconSource: "/res/action.svg"
            visible:    true
            enabled:    activeVehicle && controllerIdle
            actionID:   customController.actionClearLogs
        }
    ]
}
