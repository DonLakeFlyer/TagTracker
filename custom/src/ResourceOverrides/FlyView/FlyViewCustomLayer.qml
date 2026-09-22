/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import QtLocation
import QtPositioning
import QtQuick.Window
import QtQml.Models

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView
import QGroundControl.FlightMap
// To implement a custom overlay copy this code to your own control in your custom code source. Then override the
// FlyViewCustomLayer.qml resource with your own qml. See the custom example and documentation for details.
Item {
    id: _root

    property var parentToolInsets               // These insets tell you what screen real estate is available for positioning the controls in your overlay
    property var totalToolInsets: _toolInsets   // These are the insets for your custom overlay additions
    property var mapControl

    property var _customPlugin:     QGroundControl.corePlugin
    property var _customSettings:   QGroundControl.settingsManager.customSettings
    property var _guidedController: globals.guidedControllerFlyView

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    property real _minSNR:          _customPlugin.minSNR
    property real _maxSNR:          _customPlugin.maxSNR
    property real _snrRange:        _maxSNR - _minSNR
    property real _pixelsPerSNR:    snrGradient.height / _snrRange

    property int    _tickSNRIncrement:      _snrRange / 5 > 3 ? 5 : 1
    property int    _maxTickSNR:            Math.floor(_maxSNR / _tickSNRIncrement) * _tickSNRIncrement
    property int    _tickCount:             Math.floor(_snrRange / _tickSNRIncrement)
    property real   _tickFirstPixelY:       (_maxSNR - _maxTickSNR) * _pixelsPerSNR
    property real   _tickPixelIncrement:    _tickSNRIncrement * _pixelsPerSNR


    QGCToolInsets {
        id:                     _toolInsets
        leftEdgeTopInset:       parentToolInsets.leftEdgeTopInset
        leftEdgeCenterInset:    parentToolInsets.leftEdgeCenterInset
        leftEdgeBottomInset:    emergencyStopButton.visible ? Math.max(parentToolInsets.leftEdgeBottomInset, emergencyStopButton.width + emergencyStopButton.anchors.leftMargin) : parentToolInsets.leftEdgeBottomInset
        rightEdgeTopInset:      parentToolInsets.rightEdgeTopInset
        rightEdgeCenterInset:   parentToolInsets.rightEdgeCenterInset
        rightEdgeBottomInset:   parentToolInsets.rightEdgeBottomInset
        topEdgeLeftInset:       parentToolInsets.topEdgeLeftInset
        topEdgeCenterInset:     parentToolInsets.topEdgeCenterInset
        topEdgeRightInset:      parentToolInsets.topEdgeRightInset
        bottomEdgeLeftInset:    emergencyStopButton.visible ? emergencyStopButton.anchors.bottomMargin + emergencyStopButton.height + ScreenTools.defaultFontPixelWidth : parentToolInsets.bottomEdgeLeftInset
        bottomEdgeCenterInset:  parentToolInsets.bottomEdgeCenterInset
        bottomEdgeRightInset:   parentToolInsets.bottomEdgeRightInset
    }

    // TagTracker shows emergency stop whenever the vehicle is armed, not just while flying.
    QGCButton {
        id:                     emergencyStopButton
        anchors.left:           parent.left
        anchors.bottom:         parent.bottom
        anchors.leftMargin:     ScreenTools.defaultFontPixelWidth
        anchors.bottomMargin:   parentToolInsets.bottomEdgeLeftInset + ScreenTools.defaultFontPixelWidth
        text:                   qsTr("EMERGENCY STOP")
        backgroundColor:        "red"
        textColor:              "white"
        fontWeight:             Font.Bold
        visible:                _guidedController.showEmergenyStop ||
                                (_guidedController.showDisarm && QGroundControl.corePlugin.options.flyView.guidedBarShowEmergencyStop)
        onClicked:              _guidedController.confirmAction(_guidedController.actionEmergencyStop)
    }

    ColumnLayout {
        anchors.top:            parent.top
        anchors.right:          parent.right
        anchors.bottom:         parent.bottom
        anchors.margins:        ScreenTools.defaultFontPixelWidth
        anchors.bottomMargin:   parentToolInsets.bottomEdgeRightInset + ScreenTools.defaultFontPixelWidth
        spacing:                ScreenTools.defaultFontPixelHeight / 4

        // Progress card shared by controller operations and the WiFi log download.
        component OperationCard: Rectangle {
            id:                     operationCard
            Layout.alignment:       Qt.AlignRight
            Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 26
            Layout.preferredHeight: operationColumn.height + ScreenTools.defaultFontPixelWidth * 2
            color:                  Qt.rgba(qgcPal.window.r, qgcPal.window.g, qgcPal.window.b, 0.75)
            radius:                 ScreenTools.defaultFontPixelWidth / 2

            property string title
            property string message
            property real   fraction:   -1      // 0..1, or -1 when indeterminate
            property bool   running:    true
            property bool   failed:     false

            ColumnLayout {
                id:                 operationColumn
                anchors.left:       parent.left
                anchors.right:      parent.right
                anchors.margins:    ScreenTools.defaultFontPixelWidth
                anchors.verticalCenter: parent.verticalCenter
                spacing:            ScreenTools.defaultFontPixelHeight / 4

                QGCLabel {
                    Layout.fillWidth:   true
                    text:               operationCard.title
                    font.bold:          true
                    elide:              Text.ElideRight
                }

                Rectangle {
                    id:                     operationBar
                    Layout.fillWidth:       true
                    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 0.75
                    color:                  qgcPal.windowShade
                    radius:                 height / 4
                    clip:                   true

                    property bool indeterminate: operationCard.fraction < 0 && operationCard.running

                    readonly property int  _sweepMSecs:    900
                    readonly property real _sweepDistance: width - operationFill.width

                    Rectangle {
                        id:             operationFill
                        anchors.top:    parent.top
                        anchors.bottom: parent.bottom
                        radius:         parent.radius
                        color:          operationCard.failed ? "red" : qgcPal.colorGreen
                        width:          operationBar.indeterminate ? parent.width / 4 : parent.width * Math.max(0, Math.min(1, operationCard.running ? operationCard.fraction : 1))
                        x:              0

                        SequentialAnimation on x {
                            running:    operationBar.indeterminate && operationCard.visible
                            loops:      Animation.Infinite
                            onStopped:  operationFill.x = 0
                            NumberAnimation { from: 0; to: operationBar._sweepDistance; duration: operationBar._sweepMSecs; easing.type: Easing.InOutQuad }
                            NumberAnimation { from: operationBar._sweepDistance; to: 0; duration: operationBar._sweepMSecs; easing.type: Easing.InOutQuad }
                        }
                    }

                    QGCLabel {
                        anchors.centerIn:   parent
                        text:               operationCard.failed ? qsTr("Failed")
                                            : (!operationCard.running ? qsTr("Done")
                                            : (operationCard.fraction < 0 ? "" : Math.round(operationCard.fraction * 100) + "%"))
                        font.pointSize:     ScreenTools.smallFontPointSize
                        font.bold:          true
                    }
                }

                QGCLabel {
                    Layout.fillWidth:   true
                    text:               operationCard.message
                    font.pointSize:     ScreenTools.smallFontPointSize
                    elide:              Text.ElideMiddle
                    visible:            text !== "" && text !== operationCard.title
                }
            }
        }

        // Controller long-running operation (raw capture, log save/delete, detection start/stop)
        OperationCard {
            property var _operation: _customPlugin.operationProgress

            visible:    _operation.active
            title:      _operation.title
            message:    _operation.message
            fraction:   _operation.fraction
            running:    _operation.running
            failed:     _operation.failed
        }

        // scp gives no machine-readable progress, so the WiFi download is always indeterminate.
        OperationCard {
            visible:    _customPlugin.companionLogDownloader.downloading
            title:      _guidedController._customController.downloadLogsTitle
            message:    qsTr("Copying companion logs over WiFi...")
        }

        Rectangle {
            id:                     pulseOverlayBackground
            Layout.alignment:       Qt.AlignRight
            Layout.preferredWidth:  pulseOverlay.width + ScreenTools.defaultFontPixelWidth * 2
            Layout.preferredHeight: pulseOverlay.height + ScreenTools.defaultFontPixelWidth * 2
            color:                  Qt.rgba(qgcPal.window.r, qgcPal.window.g, qgcPal.window.b, 0.75)
            radius:                 ScreenTools.defaultFontPixelWidth / 2
            visible:                _customPlugin.detectorList.count > 0 && !_customPlugin.controllerLostHeartbeat

            ColumnLayout {
                id:                 pulseOverlay
                anchors.centerIn:   parent
                spacing:            2

                Repeater {
                    model: _customPlugin.detectorList

                    RowLayout {
                        property real maxStrength:  _customSettings.maxPulseStrength.rawValue

                        Rectangle {
                            id:                     pulseRect
                            Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.5
                            Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 20
                            color:                  object.heartbeatLost ? "red" : (object.lastPulseNoPulse ? "gray" : "transparent")

                            Rectangle {
                                property real filteredSNR: Math.max(0, Math.min(object.lastPulseStrength, maxStrength))

                                anchors.rightMargin:    maxStrength <= 0 ? parent.width : ((maxStrength - filteredSNR) / maxStrength) * parent.width
                                anchors.fill:           parent
                                color:                  object.lastPulseLowConfidence ? "orange" : "green"
                                visible:                !object.heartbeatLost && !object.lastPulseNoPulse && !object.waitingForFirstPulse
                            }

                            QGCLabel {
                                anchors.fill:           parent
                                text:                   object.waitingForFirstPulse ? qsTr("Waiting...") : (object.lastPulseNoPulse ? qsTr("No Pulse ×%1").arg(object.noPulseCount) : Math.max(0, Math.min(object.lastPulseStrength, maxStrength)).toFixed(1))
                                font.bold:              true
                                color:                  (object.lastPulseNoPulse || object.heartbeatLost) ? "white" : "black"
                                horizontalAlignment:    Text.AlignHCenter
                                verticalAlignment:      Text.AlignVCenter
                            }
                        }

                        QGCLabel {
                            text:               object.rateLabel !== "" ? object.rateLabel : object.tagLabel[0]
                            visible:            !object.lastPulseNoPulse && !object.waitingForFirstPulse
                            color:              qgcPal.text
                            verticalAlignment:  Text.AlignVCenter
                        }
                    }
                }
            }
        }

        Rectangle {
            id:                 snrGradient
            Layout.alignment:   Qt.AlignRight
            width:              ScreenTools.defaultFontPixelWidth * 5
            Layout.fillHeight:  true
            visible:            _customSettings.detectionFlightMode.rawValue === CustomSettings.SurveyDetection

            gradient: Gradient {
                GradientStop { position: 0; color: "red" }
                GradientStop { position: 0.25; color: "yellow" }
                GradientStop { position: 0.5; color: "green" }
                GradientStop { position: 0.75; color: "blue" }
                GradientStop { position: 1; color: "navy" }
            }

            Repeater {
                model: _tickCount

                Rectangle {
                    anchors.horizontalCenter:   parent.horizontalCenter
                    width:                      tickLabel.contentWidth + (2 * _labelMargins)
                    height:                     tickLabel.contentHeight + (2 * _labelMargins)
                    color:                      "white"
                    y:                          _tickFirstPixelY + (index * _tickPixelIncrement)
                    radius:                     ScreenTools.defaultFontPixelWidth / 4

                    property real _labelMargins: ScreenTools.defaultFontPixelWidth / 4

                    QGCLabel {
                        id:     tickLabel
                        x:      _labelMargins
                        y:      _labelMargins
                        text:   _maxTickSNR - (index * _tickSNRIncrement)
                        color:  "black"
                    }
                }
            }
        }

        // Keeps the cards pinned to the top when the gradient is hidden
        Item {
            Layout.fillHeight:  true
            visible:            !snrGradient.visible
        }
    }
}
