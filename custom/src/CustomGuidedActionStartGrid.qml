/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView
import QGroundControl.CustomControls

// Opens the Grid editor panel over the Fly View (FlyViewCustomLayer); its Start grid opens the confirm slider
ToolStripAction {
    objectName: "flyViewToolStrip_gridButton"
    text:       customController.startGridTitle
    iconSource: "/res/action.svg"
    visible:    QGroundControl.settingsManager.customSettings.detectionFlightMode.rawValue === CustomSettings.SurveyDetection
    enabled:    _activeVehicle && _activeVehicle.airborne && _activeVehicle.coordinate.isValid
                && !QGroundControl.corePlugin.rotationInProgress && !QGroundControl.corePlugin.gridInProgress

    property var _activeVehicle: QGroundControl.multiVehicleManager.activeVehicle
    property var _guidedController: globals.guidedControllerFlyView
    property var customController: _guidedController._customController

    onTriggered: {
        _guidedController.closeAll()
        QGroundControl.corePlugin.startGridPreview()
    }
}
