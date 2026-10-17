import QtQuick

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView

Item {
    property var model: listModel

    property var    _corePlugin:            QGroundControl.corePlugin
    property var    _tagInfoList:           _corePlugin.tagDatabase.tagInfoList
    property int    _selectedTagCount: {
        let count = 0
        for (let i = 0; i < _tagInfoList.count; i++) {
            if (_tagInfoList.get(i).selected.rawValue) {
                count++
            }
        }
        return count
    }

    PreFlightCheckModel {
        id: listModel

        PreFlightCheckGroup {
            name: qsTr("Hardware")

            PreFlightCheckButton {
                name:           qsTr("Landing pad")
                manualText:     qsTr("Landing pad placed on firm, level ground?")
            }

            PreFlightCheckButton {
                name:           qsTr("Hardware")
                manualText:     qsTr("Arms unfolded and latched?")
            }

            PreFlightCheckButton {
                name:           qsTr("Props")
                manualText:     qsTr("Props unfolded?")
            }

            PreFlightCheckButton {
                name:           qsTr("Vehicle position")
                manualText:     qsTr("Vehicle on the landing pad with the green arms facing away?")
            }

            PreFlightCheckButton {
                name:           qsTr("Antenna")
                manualText:     qsTr("Antenna firmly clipped to the landing legs?")
            }

            PreFlightCheckButton {
                name:           qsTr("USB cable")
                manualText:     qsTr("USB cable firmly connected to the antenna BNC?")
            }

            PreFlightBatteryCheck {
                manualText:                     qsTr("Both batteries fully inserted and connected?")
                failurePercent:                 50
                allowFailurePercentOverride:    false
            }
        }

        PreFlightCheckGroup {
            name: qsTr("Tracker Checks")

            PreFlightCheckButton {
                name:                   qsTr("Controller")
                telemetryFailure:       !_corePlugin.protocolCompatible
                telemetryTextFailure:   qsTr("No compatible controller heartbeat. Check that the controller is running and its protocol version matches this app.")
            }

            PreFlightCheckButton {
                name:                   qsTr("Tags")
                telemetryFailure:       _selectedTagCount === 0
                telemetryTextFailure:   qsTr("No tag selected. Select a tag from the tag indicator in the toolbar.")
            }
        }

        PreFlightCheckGroup {
            name: qsTr("Last preparations before launch")

            PreFlightCheckButton {
                name:           qsTr("Wind & weather")
                manualText:     qsTr("OK for your platform?")
            }

            PreFlightCheckButton {
                name:           qsTr("Flight area")
                manualText:     qsTr("Launch area and path free of obstacles/people?")
            }
        }
    }
}
