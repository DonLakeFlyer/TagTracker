import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView

// Full-screen replacement for the Fly View: Start, Pause and Return buttons, an altitude bar and the rotation compass rose.
Rectangle {
    id:     root
    color:  qgcPal.window

    property var    guidedController                // GuidedActionsController
    property var    _customPlugin:      QGroundControl.corePlugin
    property var    _customSettings:    QGroundControl.settingsManager.customSettings
    property var    _activeVehicle:     QGroundControl.multiVehicleManager.activeVehicle
    property var    _rotationInfoList:  _customPlugin.rotationInfoList
    property var    _rotationInfo:      _rotationInfoList.count > 0 ? _rotationInfoList.get(_rotationInfoList.count - 1) : null
    property bool   _running:           _customPlugin.rotationInProgress
    property bool   _roseVisible:       _customPlugin.activeRotation
    property bool   _airborne:          !!_activeVehicle && _activeVehicle.airborne
    property bool   _canStart:          !!_activeVehicle && !_airborne && !_customPlugin.controllerLostHeartbeat && !_running
    property real   _roseSize:          Math.min(width, height) * 0.55

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    // Keeps clicks, hovers and scrolls from reaching the map underneath
    MouseArea {
        anchors.fill:   parent
        hoverEnabled:   true
        onWheel:        (wheel) => wheel.accepted = true
    }

    Connections {
        target: root._customPlugin

        // The previous result stays up until the next run starts
        function onRotationInProgressChanged(inProgress) {
            if (inProgress) {
                root._roseVisible = false
            }
        }

        function onActiveRotationChanged(active) {
            if (active) {
                root._roseVisible = true
            }
        }
    }

    QGCLabel {
        anchors.top:                parent.top
        anchors.topMargin:          ScreenTools.defaultFontPixelHeight
        anchors.horizontalCenter:   parent.horizontalCenter
        font.pointSize:             ScreenTools.largeFontPointSize
        text: {
            if (!root._activeVehicle) {
                return qsTr("No vehicle connected")
            }
            if (root._customPlugin.controllerLostHeartbeat) {
                return qsTr("Waiting for controller")
            }
            if (root._running) {
                return qsTr("Rotation in progress")
            }
            return root._activeVehicle.airborne ? qsTr("Vehicle is flying") : qsTr("Ready")
        }
    }

    // Altitude above takeoff, from the ground up to the rotation altitude
    Item {
        id:                     altitudeIndicator
        anchors.left:           parent.left
        anchors.leftMargin:     ScreenTools.defaultFontPixelWidth * 12
        anchors.verticalCenter: parent.verticalCenter
        width:                  ScreenTools.defaultFontPixelWidth * 16
        height:                 root.height * 0.55
        visible:                !!root._activeVehicle

        property real _altitude:        root._activeVehicle && !isNaN(root._activeVehicle.altitudeRelative.rawValue) ? Math.max(0, root._activeVehicle.altitudeRelative.rawValue) : 0
        property real _target:          root._customSettings.takeoffAltitude.rawValue
        property real _scale:           Math.max(_target, _altitude, 1)
        property real _fraction:        _altitude / _scale
        property real _targetFraction:  _target / _scale
        property real _shownFraction:   _fraction

        Behavior on _shownFraction { NumberAnimation { duration: 300 } }

        Rectangle {
            id:             altitudeTrack
            width:          ScreenTools.defaultFontPixelWidth * 1.5
            anchors.left:   parent.left
            anchors.top:    parent.top
            anchors.bottom: parent.bottom
            radius:         width / 2
            color:          qgcPal.borderSubtle

            Rectangle {
                anchors.left:   parent.left
                anchors.right:  parent.right
                anchors.bottom: parent.bottom
                height:         parent.height * altitudeIndicator._shownFraction
                radius:         parent.radius
                color:          qgcPal.colorGreen
            }
        }

        // Target sits left of the track so it can't collide with the current reading on the right
        Rectangle {
            x:      -ScreenTools.defaultFontPixelWidth * 1.5
            y:      altitudeTrack.height * (1 - altitudeIndicator._targetFraction)
            width:  ScreenTools.defaultFontPixelWidth
            height: 2
            color:  qgcPal.text

            QGCLabel {
                anchors.right:          parent.left
                anchors.rightMargin:    ScreenTools.defaultFontPixelWidth * 0.5
                anchors.verticalCenter: parent.verticalCenter
                text:                   root._customSettings.takeoffAltitude.valueString + " " + root._customSettings.takeoffAltitude.units
                font.pointSize:         ScreenTools.smallFontPointSize
            }
        }

        // Current altitude marker, rides the top of the fill
        Rectangle {
            x:      -ScreenTools.defaultFontPixelWidth * 0.5
            y:      altitudeTrack.height * (1 - altitudeIndicator._shownFraction) - height / 2
            width:  altitudeTrack.width + ScreenTools.defaultFontPixelWidth
            height: 4
            color:  qgcPal.text

            QGCLabel {
                anchors.left:           parent.right
                anchors.leftMargin:     ScreenTools.defaultFontPixelWidth * 0.5
                anchors.verticalCenter: parent.verticalCenter
                text:                   root._activeVehicle ? root._activeVehicle.altitudeRelative.valueString + " " + root._activeVehicle.altitudeRelative.units : ""
                font.pointSize:         ScreenTools.largeFontPointSize
                font.bold:              true
            }
        }
    }

    ColumnLayout {
        anchors.centerIn:   parent
        spacing:            0

        Item {
            id:                     roseHolder
            Layout.alignment:       Qt.AlignHCenter
            Layout.preferredWidth:  root._roseSize
            Layout.preferredHeight: root._roseSize
            visible:                root._roseVisible && root._rotationInfo !== null

            QGCLabel {
                anchors.bottom:             parent.top
                anchors.bottomMargin:       ScreenTools.defaultFontPixelHeight
                anchors.horizontalCenter:   parent.horizontalCenter
                text:                       qsTr("N")
                font.pointSize:             ScreenTools.largeFontPointSize
                font.bold:                  true
            }

            Loader {
                anchors.fill:   parent
                active:         roseHolder.visible

                sourceComponent: RotationRose {
                    width:          roseHolder.width
                    rotationInfo:   root._rotationInfo
                    borderColor:    qgcPal.text
                }
            }
        }

        QGCDelayButton {
            Layout.alignment:       Qt.AlignHCenter
            // Clears the rose's outcome text, which hangs below the circle
            Layout.topMargin:       ScreenTools.defaultFontPixelHeight * 3
            text:                   qsTr("Start")
            enabled:                root._canStart
            visible:                !root._running && !root._airborne
            onActivated:            root._customPlugin.autoDetection()
        }
    }

    RowLayout {
        anchors.bottom:             parent.bottom
        anchors.bottomMargin:       ScreenTools.defaultFontPixelHeight * 2
        anchors.horizontalCenter:   parent.horizontalCenter
        spacing:                    ScreenTools.defaultFontPixelWidth * 4

        QGCDelayButton {
            text:                   qsTr("Pause")
            enabled:                !!root.guidedController && root.guidedController.showPause
            onActivated:            root._customPlugin.pauseRotation()
        }

        QGCDelayButton {
            text:                   qsTr("Return")
            enabled:                !!root.guidedController && root.guidedController.showRTL
            onActivated:            root._activeVehicle.guidedModeRTL(false)
        }
    }

    // The widget layer's own VehicleWarnings is under this screen
    VehicleWarnings {
        anchors.centerIn:   parent
    }
}
