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
import QGroundControl.Controllers
import QGroundControl.Controls
import QGroundControl.FactSystem
import QGroundControl.FlightDisplay
import QGroundControl.FlightMap
import QGroundControl.Palette
import QGroundControl.ScreenTools
import QGroundControl.Vehicle

// To implement a custom overlay copy this code to your own control in your custom code source. Then override the
// FlyViewCustomLayer.qml resource with your own qml. See the custom example and documentation for details.
Item {
    id: _root

    property var parentToolInsets               // These insets tell you what screen real estate is available for positioning the controls in your overlay
    property var totalToolInsets: _toolInsets   // These are the insets for your custom overlay additions
    property var mapControl

    property var _customPlugin:     QGroundControl.corePlugin
    property var _customSettings:   _customPlugin.customSettings

    property real _minSNR:          _customPlugin.minSNR
    property real _maxSNR:          _customPlugin.maxSNR
    property real _snrRange:        _maxSNR - _minSNR
    property real _pixelsPerSNR:    snrGradient.height / _snrRange

    property int    _tickSNRIncrement:      _snrRange / 5 > 3 ? 5 : 1
    property int    _maxTickSNR:            Math.floor(_maxSNR / _tickSNRIncrement) * _tickSNRIncrement
    property int    _tickCount:             Math.floor(_snrRange / _tickSNRIncrement)
    property real   _tickFirstPixelY:       (_maxSNR - _maxTickSNR) * _pixelsPerSNR
    property real   _tickPixelIncrement:    _tickSNRIncrement * _pixelsPerSNR


    // since this file is a placeholder for the custom layer in a standard build, we will just pass through the parent insets
    QGCToolInsets {
        id:                     _toolInsets
        leftEdgeTopInset:       parentToolInsets.leftEdgeTopInset
        leftEdgeCenterInset:    parentToolInsets.leftEdgeCenterInset
        leftEdgeBottomInset:    parentToolInsets.leftEdgeBottomInset
        rightEdgeTopInset:      parentToolInsets.rightEdgeTopInset
        rightEdgeCenterInset:   parentToolInsets.rightEdgeCenterInset
        rightEdgeBottomInset:   parentToolInsets.rightEdgeBottomInset
        topEdgeLeftInset:       parentToolInsets.topEdgeLeftInset
        topEdgeCenterInset:     parentToolInsets.topEdgeCenterInset
        topEdgeRightInset:      parentToolInsets.topEdgeRightInset
        bottomEdgeLeftInset:    parentToolInsets.bottomEdgeLeftInset
        bottomEdgeCenterInset:  parentToolInsets.bottomEdgeCenterInset
        bottomEdgeRightInset:   parentToolInsets.bottomEdgeRightInset
    }

    ColumnLayout {
        anchors.margins:    ScreenTools.defaultFontPixelWidth
        anchors.top:        parent.top
        anchors.right:      parent.right
        height:             parent.height - (anchors.margins * 2) - parentToolInsets.bottomEdgeRightInset
        spacing:            ScreenTools.defaultFontPixelHeight / 4

        QGCButton {
            Layout.alignment:   Qt.AlignHCenter
            text:               qsTr("Clear Map")
            onClicked:          _customPlugin.clearMap()
            enabled:            !_customPlugin.activeRotation
            visible:           _customPlugin.customMapItems.count
        }

        Rectangle {
            id:                 snrGradient
            Layout.alignment:   Qt.AlignRight
            width:              ScreenTools.defaultFontPixelWidth * 5
            Layout.fillHeight:  true
            visible:            _customSettings.showPulseOnMap.rawValue

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
    }
}
