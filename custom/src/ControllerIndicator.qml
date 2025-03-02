/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls
import QGroundControl.FactSystem
import QGroundControl.MultiVehicleManager
import QGroundControl.ScreenTools
import QGroundControl.Palette
import MAVLink

import QGroundControl.CustomControls

Item {
    id:                 control
    anchors.margins:    -ScreenTools.defaultFontPixelHeight / 2
    anchors.top:        parent.top
    anchors.bottom:     parent.bottom
    width:              rowLayout.width

    property bool showIndicator: true

    property var    activeVehicle:  QGroundControl.multiVehicleManager.activeVehicle
    property real   maxStrength:    QGroundControl.corePlugin.customSettings.maxPulseStrength.rawValue

    Row {
        id:             rowLayout
        anchors.top:    parent.top
        anchors.bottom: parent.bottom
        spacing:        ScreenTools.defaultFontPixelWidth / 2

        Rectangle {
            height: parent.height
            width:  cpuTemp.contentWidth + ScreenTools.defaultFontPixelWidth / 4
            color:  QGroundControl.corePlugin.controllerLostHeartbeat ? "red" : "green"

            QGCLabel {
                id:                     cpuTemp
                anchors.fill:           parent
                text:                   qsTr("Temp") + " " + (isNaN(cpuTemp) ? qsTr("N/A") : cpuTemp.toFixed(0))
                color:                  "black"
                horizontalAlignment:    Text.AlignHCenter
                verticalAlignment:      Text.AlignVCenter

                property real cpuTemp: QGroundControl.corePlugin.controllerCPUTemp
            }
        }

        ColumnLayout {
            height:     parent.height
            spacing:    2

            Repeater {
                model: QGroundControl.corePlugin.detectorInfoList

                RowLayout {
                    property real filteredSNR: Math.max(0, Math.min(object.lastPulseStrength, maxStrength))

                    Rectangle {
                        id:                     pulseRect
                        Layout.fillHeight:      true
                        Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 20
                        color:                  object.heartbeatLost ? 
                                                    "red" :
                                                    object.lastPulseStale ? "yellow" : "transparent" 

                        Rectangle {
                            anchors.topMargin:      2
                            anchors.bottomMargin:   2
                            anchors.leftMargin:     2
                            anchors.rightMargin:    ((maxStrength - filteredSNR) / maxStrength) *  (parent.width - 4)
                            anchors.fill:           parent
                            color:                  object.lastPulseStale ? "yellow" : "green"
                            visible:                !object.heartbeatLost

                        }

                        QGCLabel {
                            anchors.fill:           parent
                            text:                   filteredSNR.toFixed(1)
                            font.bold:              true
                            color:                  "black"
                            horizontalAlignment:    Text.AlignHCenter
                            verticalAlignment:      Text.AlignVCenter
                        }
                    }

                    QGCLabel {
                        Layout.preferredHeight: pulseRect.height
                        text:                   object.tagId + object.tagLabel[0]
                        fontSizeMode:           Text.VerticalFit
                        verticalAlignment:      Text.AlignVCenter
                    }
                }
            }
        }
    }

    MouseArea {
        anchors.fill:   rowLayout
        onClicked:      mainWindow.showIndicatorDrawer(indicatorPopup, control)
    }

    Component {
        id: indicatorPopup

        ToolIndicatorPage {
            showExpand:         true
            waitForParameters:  false
            contentComponent:   indicatorContentComponent
            expandedComponent:  indicatorExpandedComponent
        }
    }

    Component {
        id: manufacturersDialogComponent
        TagManufacturersDialog { }
    }

    Component {
        id: tagInfoDialogComponent
        TagInfoDialog { }
    }

    Component {
        id: indicatorContentComponent

        ColumnLayout {
            spacing: ScreenTools.defaultFontPixelHeight

            property var tagDatabase: QGroundControl.corePlugin.tagDatabase

            RowLayout {
                spacing: ScreenTools.defaultFontPixelWidth

                QGCButton {
                    text:       qsTr("Manufacturers")
                    onClicked:  manufacturersDialogComponent.createObject(mainWindow).open()
                }

                QGCButton {
                    text:       qsTr("New Tag")
                    onClicked: { 
                        if (tagDatabase.tagManufacturerList.count == 0) {
                            mainWindow.showMessageDialog(qsTr("New Tag"), qsTr("You must add a Manufacturer first."))
                        } else {
                            tagInfoDialogComponent.createObject(mainWindow, { tagInfo: tagDatabase.newTagInfo() }).open()
                        }
                    }
                }
            }

            GridLayout {
                rows:       tagDatabase.tagInfoList.count + 1
                columns:    4
                flow:       GridLayout.TopToBottom

                QGCLabel { }
                Repeater {
                    model: tagDatabase.tagInfoList

                    FactCheckBox { 
                        fact:       object.selected 
                        onClicked:  QGroundControl.corePlugin.tagDatabase.save()
                    }
                }

                QGCLabel { text: qsTr("Name") }
                Repeater {
                    model: tagDatabase.tagInfoList

                    QGCLabel { text: object.name.valueString }
                }

                QGCLabel { text: qsTr("Frequency") }
                Repeater {
                    model: tagDatabase.tagInfoList

                    QGCLabel { text: object.frequencyHz.valueString }
                }

                QGCLabel { }
                Repeater {
                    model: tagDatabase.tagInfoList

                    QGCButton {
                        text:       qsTr("Edit")
                        onClicked:  tagInfoDialogComponent.createObject(mainWindow, { tagInfo: object }).open()
                    }
                }

                QGCLabel { }
                Repeater {
                    model: tagDatabase.tagInfoList

                    QGCButton {
                        text:       qsTr("Del")
                        onClicked: {
                            tagDatabase.deleteTagInfoListItem(object)
                            QGroundControl.corePlugin.tagDatabase.save()
                        }
                    }
                }
            }
        }
    }

    Component {
        id: indicatorExpandedComponent

        SettingsGroupLayout {
            property var _customSettings:               QGroundControl.corePlugin.customSettings
            property Fact _antennaTypeFact:             _customSettings.antennaType
            property Fact _autoTakeoffRotateRTLFact:    _customSettings.autoTakeoffRotateRTL
            property int _antennaTypeDirectional:       1

            LabelledFactComboBox {
                Layout.fillWidth:   true
                label:              fact.shortDescription
                fact:               _customSettings.useSNRForPulseStrength
            }

            FactCheckBoxSlider {
                Layout.fillWidth:   true
                text:               fact.shortDescription
                fact:               _customSettings.showPulseOnMap
            }

            LabelledFactTextField {
                Layout.fillWidth:   true
                label:              fact.shortDescription
                fact:               _customSettings.takeoffAltitude
            }

            LabelledFactTextField {
                Layout.fillWidth:   true
                label:              fact.shortDescription
                fact:               _customSettings.divisions
            }

            LabelledFactTextField {
                Layout.fillWidth:   true
                label:              fact.shortDescription
                fact:               _customSettings.k
            }

            LabelledFactTextField {
                label:              fact.shortDescription
                fact:               _customSettings.falseAlarmProbability
            }

            LabelledFactTextField {
                Layout.fillWidth:   true
                label:              fact.shortDescription
                fact:               _customSettings.maxPulseStrength
            }

            LabelledFactTextField {
                Layout.fillWidth:   true
                label:              fact.shortDescription
                fact:               _customSettings.antennaOffset
            }

            LabelledFactComboBox {
                Layout.fillWidth:   true
                label:              fact.shortDescription
                fact:               _customSettings.gain
            }

            LabelledFactComboBox {
                Layout.fillWidth:   true
                label:              fact.shortDescription
                fact:               _antennaTypeFact
            }

            FactCheckBoxSlider {
                Layout.fillWidth:   true
                text:               fact.shortDescription
                fact:               _autoTakeoffRotateRTLFact
                enabled:            _antennaTypeFact.rawValue === _antennaTypeDirectional

                Connections {
                    target: _antennaTypeFact
                    onRawValueChanged: _autoTakeoffRotateRTLFact.value = _antennaTypeFact.rawValue === _antennaTypeDirectional
                }
            }
        }
    }
}
