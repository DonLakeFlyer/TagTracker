/****************************************************************************
 *
 * (c) 2009-2025 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick

import QGroundControl
import QGroundControl.Controls
// TagTracker shows emergency stop whenever the vehicle is armed, not just while flying.
Item {
    id:             control
    anchors.top:    parent.top
    anchors.bottom: parent.bottom
    width:          emergencyStopButton.width

    property bool showIndicator: _guidedController.showEmergenyStop ||
                                 (_guidedController.showDisarm && _emergencyStopAllowed)

    readonly property var  _guidedController:       globals.guidedControllerFlyView
    readonly property bool _emergencyStopAllowed:   QGroundControl.corePlugin.options.flyView.guidedBarShowEmergencyStop

    QGCButton {
        id:                     emergencyStopButton
        anchors.verticalCenter: parent.verticalCenter
        text:                   qsTr("EMERGENCY STOP")
        backgroundColor:        "red"
        textColor:              "white"
        fontWeight:             Font.Bold
        onClicked:              control._guidedController.confirmAction(control._guidedController.actionEmergencyStop)
    }
}
