/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick

import QGroundControl

import TagTracker

// Custom builds can override this resource to add additional custom actions
Item {
    visible: false

    property var guidedController

    property bool anyActionAvailable: true

    property var    _customController:      guidedController._customController
    property var    _controllerStatus:      QGroundControl.corePlugin.controllerStatus
    property bool   _startDetectionEnabled: false
    property bool   _stopDetectionEnabled:  false

    on_ControllerStatusChanged: _updateStartStop(_controllerStatus)

    Component.onCompleted: _updateStartStop(_controllerStatus)

    function _updateStartStop(controllerStatus) {
        switch (controllerStatus) {
        case CustomPlugin.ControllerStatusIdle:
        case CustomPlugin.ControllerStatusHasTags:
            _startDetectionEnabled = true
            _stopDetectionEnabled = false
            break
        case CustomPlugin.ControllerStatusReceivingTags:
        case CustomPlugin.ControllerStatusCapture:
            _startDetectionEnabled = false
            _stopDetectionEnabled = false
            break
        case CustomPlugin.ControllerStatusDetecting:
            _startDetectionEnabled = false
            _stopDetectionEnabled = true
            break
        default:
            console.warn("Internal error: Unhandled controller status: " + controllerStatus)
            _startDetectionEnabled = false
            _stopDetectionEnabled = false
            break
        }
    }

    property var model: [ 
        {
            title:      _customController.startDetectionTitle,
            visible:    true,
            enabled:    _startDetectionEnabled,
            action:     _customController.actionStartDetection,
        },
        {
            title:      _customController.stopDetectionTitle,
            visible:    true,
            enabled:    _stopDetectionEnabled,
            action:     _customController.actionStopDetection,
        },
        {
            title:      _customController.rawCaptureTitle,
            visible:    true,
            enabled:    QGroundControl.multiVehicleManager.activeVehicle && (QGroundControl.corePlugin.controllerStatus == CustomPlugin.ControllerStatusHasTags || QGroundControl.corePlugin.controllerStatus == CustomPlugin.ControllerStatusIdle),
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
