import QtQml.Models

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView
import QGroundControl.CustomControls

ToolStrip {
    id: _root

    signal displayPreFlightChecklist

    ToolStripActionList {
        id: fullActionList

        signal displayPreFlightChecklist

        property var customController:  _guidedController._customController
        property var activeVehicle:     QGroundControl.multiVehicleManager.activeVehicle
        property bool controllerIdle:   QGroundControl.corePlugin.controllerStatus == CustomPlugin.ControllerStatusIdle || QGroundControl.corePlugin.controllerStatus == CustomPlugin.ControllerStatusHasLogs

        onDisplayPreFlightChecklist: _root.displayPreFlightChecklist()

        model: [
            GuidedActionTakeoff { },
            GuidedActionLand { },
            GuidedActionRTL { },
            GuidedActionPause { },
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
            CustomGuidedActionAutoDetection { },
            GuidedActionRTL { },
            GuidedActionPause { },
            FlyViewAdditionalActionsButton { }
        ]
    }

    model: QGroundControl.settingsManager.customSettings.detectionFlightMode.rawValue === CustomSettings.Auto ? autoActionList.model : fullActionList.model
}
