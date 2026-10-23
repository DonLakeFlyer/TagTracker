import QtQuick

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView

Item {
    property var model: listModel

    PreFlightCheckModel {
        id: listModel

        PreFlightCheckGroup {
            name: qsTr("Launch Area")

            PreFlightCheckButton {
                name:           qsTr("Takeoff")
                manualText:     qsTr("Takeoff path free of obstacles?")
            }

            PreFlightCheckButton {
                name:           qsTr("Landing pad")
                manualText:     qsTr("Landing pad placed on firm, level ground?")
            }
        }

        PreFlightCheckGroup {
            name: qsTr("Drone")

            PreFlightCheckButton {
                name:           qsTr("Drone")
                manualText:     qsTr("Landing legs firmly screwed on (match I-I, II-II)?")
            }

            PreFlightCheckButton {
                name:           qsTr("Drone")
                manualText:     qsTr("Landing legs parallel?")
            }

            PreFlightCheckButton {
                name:           qsTr("Drone")
                manualText:     qsTr("Place drone centered on the landing pad with the green arms facing away?")
            }

            PreFlightCheckButton {
                name:           qsTr("Drone")
                manualText:     qsTr("Arms unfolded and latched?")
            }

            PreFlightCheckButton {
                name:           qsTr("Antenna")
                manualText:     qsTr("Antenna firmly clipped to the landing legs?")
            }

            PreFlightCheckButton {
                name:           qsTr("USB cable")
                manualText:     qsTr("USB cable firmly connected to the antenna BNC?")
            }

            PreFlightCheckButton {
                name:           qsTr("Drone")
                manualText:     qsTr("Props unfolded?")
            }
        }
    }
}
