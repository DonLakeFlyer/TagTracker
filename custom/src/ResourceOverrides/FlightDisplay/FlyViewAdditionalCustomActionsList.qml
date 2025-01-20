/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQml

import QGroundControl

// Custom builds can override this resource to add additional custom actions
QtObject {
    property var guidedController

    property bool anyActionAvailable: true

    property var _customController:  guidedController._customController

    property var model: [ 
        {
            title:      _customController.rawCaptureTitle,
            visible:    true,
            enabled:    QGroundControl.multiVehicleManager.activeVehicle && QGroundControl.corePlugin.controllerStatus == CustomPlugin.ControllerStatusHasTags,
            action:     _customController.actionRawCapture,
        },

        {
            title:      _customController.saveLogsTitle,
            visible:    true,
            enabled:    true,
            action:     _customController.actionSaveLogs,
        },

        {
            title:      _customController.clearLogsTitle,
            visible:    true,
            enabled:    true,
            action:     _customController.actionClearLogs,
        }
    ]
}
