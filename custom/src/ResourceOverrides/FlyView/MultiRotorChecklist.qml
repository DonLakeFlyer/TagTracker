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
            name: qsTr("Power")

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

        }
    }
}
