/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQml.Models

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlightDisplay

import QGroundControl.CustomControls

ToolStrip {
    id:     _root
    title:  qsTr("Fly")

    signal displayPreFlightChecklist

    ToolStripActionList {
        id: fullActionList

        signal displayPreFlightChecklist

        property var customController:  _guidedController._customController
        property var activeVehicle:     QGroundControl.multiVehicleManager.activeVehicle
        property bool controllerIdle:   QGroundControl.corePlugin.controllerStatus == CustomPlugin.ControllerStatusIdle || QGroundControl.corePlugin.controllerStatus == CustomPlugin.ControllerStatusHasLogs

        onDisplayPreFlightChecklist: _root.displayPreFlightChecklist()

        model: [
            ToolStripAction {
                text:           qsTr("Plan")
                iconSource:     "/qmlimages/Plan.svg"
                onTriggered:{
                    mainWindow.showPlanView()
                    viewer3DWindow.close()
                }
            },
            GuidedActionTakeoff { },
            GuidedActionLand { },
            GuidedActionRTL { },
            GuidedActionPause { },
            CustomGuidedActionStartStopDetection { },
            CustomGuidedActionStartRotation { },
            FlyViewAdditionalActionsButton { }
        ]
    }

    ToolStripActionList {
        id: autoActionList

        signal displayPreFlightChecklist

        property var customController:  _guidedController._customController
        property var activeVehicle:     QGroundControl.multiVehicleManager.activeVehicle
        property bool controllerIdle:   QGroundControl.corePlugin.controllerStatus == CustomPlugin.ControllerStatusIdle || QGroundControl.corePlugin.controllerStatus == CustomPlugin.ControllerStatusHasLogs

        model: [
            GuidedActionRTL { },
            GuidedActionPause { },
            CustomGuidedActionAutoTakeoffRotateRTL { },
            FlyViewAdditionalActionsButton { }
        ]
    }

    model: QGroundControl.corePlugin.customSettings.autoTakeoffRotateRTL.rawValue ? autoActionList.model : fullActionList.model
}
