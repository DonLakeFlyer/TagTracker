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

    property var    activeVehicle:              QGroundControl.multiVehicleManager.activeVehicle
    property real   maxStrength:                QGroundControl.corePlugin.customSettings.maxPulseStrength.rawValue
    property bool   detectorsAvailable:         QGroundControl.corePlugin.detectorList.count > 0
    property bool   controllerHeartbeatLost:    QGroundControl.corePlugin.controllerLostHeartbeat
    property var    tagDatabase:                QGroundControl.corePlugin.tagDatabase
    property var    customSettings:             QGroundControl.corePlugin.customSettings

    Row {
        id:             rowLayout
        anchors.top:    parent.top
        anchors.bottom: parent.bottom
        spacing:        ScreenTools.defaultFontPixelWidth / 2

        Rectangle {
            id:         controllerStatus
            height:     parent.height
            width:      ScreenTools.defaultFontPixelWidth * 20
            color:      QGroundControl.corePlugin.controllerLostHeartbeat ? "red" : "green"
            visible:    controllerHeartbeatLost || !detectorsAvailable

            QGCLabel {
                anchors.fill:           parent
                text:                   controllerHeartbeatLost ? qsTr("Controller Lost") : qsTr("No Detectors")
                color:                  "white"
                horizontalAlignment:    Text.AlignHCenter
                verticalAlignment:      Text.AlignVCenter
            }
        }

        ColumnLayout {
            height:     parent.height
            spacing:    2
            visible:    !controllerStatus.visible

            Repeater {
                model: QGroundControl.corePlugin.detectorList

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
                            anchors.rightMargin:    ((maxStrength - filteredSNR) / maxStrength) *  parent.width
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
                        text:                   object.tagLabel[0]
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

            GridLayout {
                rows:       tagDatabase.tagInfoList.count
                columns:    4
                flow:       GridLayout.TopToBottom

                Repeater {
                    model: tagDatabase.tagInfoList

                    FactCheckBox { 
                        fact:       object.selected 
                        onClicked:  tagDatabase.save()

                        onCheckedChanged: {
                            if (checked && customSettings.allowMultiTagDetection.rawValue === false) {
                                for (var i = 0; i < tagDatabase.tagInfoList.count; i++) {
                                    if (tagDatabase.tagInfoList.get(i) !== object) {
                                        tagDatabase.tagInfoList.get(i).selected.rawValue = false
                                    }
                                }
                            }
                        }
                    }
                }

                Repeater {
                    model: tagDatabase.tagInfoList

                    QGCLabel { text: object.name.valueString }
                }

                Repeater {
                    model: tagDatabase.tagInfoList

                    QGCLabel { text: object.frequencyMHz.valueString }
                }

                Repeater {
                    model: tagDatabase.tagInfoList

                    QGCButton {
                        text:       qsTr("Edit")
                        onClicked:  tagInfoDialogComponent.createObject(mainWindow, { tagInfo: object }).open()
                    }
                }

                Repeater {
                    model: tagDatabase.tagInfoList

                    QGCButton {
                        text:       qsTr("Del")
                        onClicked: {
                            tagDatabase.deleteTagInfoListItem(object)
                            tagDatabase.save()
                        }
                    }
                }
            }
        }
    }

    Component {
        id: indicatorExpandedComponent

        ColumnLayout {
            property var _customSettings:               QGroundControl.corePlugin.customSettings
            property Fact _antennaTypeFact:             _customSettings.antennaType
            property Fact _autoTakeoffRotateRTLFact:    _customSettings.autoTakeoffRotateRTL
            property int _antennaTypeDirectional:       1

            SettingsGroupLayout {
                RowLayout {
                    spacing: ScreenTools.defaultFontPixelWidth

                    QGCButton {
                        text:       qsTr("Add Tag")
                        onClicked: { 
                            if (tagDatabase.tagManufacturerList.count == 0) {
                                mainWindow.showMessageDialog(qsTr("Add Tag"), qsTr("You must add a Manufacturer first."))
                            } else {
                                tagInfoDialogComponent.createObject(mainWindow, { tagInfo: tagDatabase.newTagInfo() }).open()
                            }
                        }
                    }

                    QGCButton {
                        text:       qsTr("Manufacturers")
                        onClicked:  manufacturersDialogComponent.createObject(mainWindow).open()
                    }
                }
            }

            SettingsGroupLayout {
                LabelledFactTextField {
                    Layout.fillWidth:   true
                    label:              fact.shortDescription
                    fact:               _customSettings.takeoffAltitude
                }

                LabelledFactComboBox {
                    Layout.fillWidth:   true
                    label:              fact.shortDescription
                    fact:               _customSettings.rotationType
                }

                LabelledFactComboBox {
                    Layout.fillWidth:   true
                    label:              fact.shortDescription
                    fact:               _customSettings.divisions
                }

                LabelledFactComboBox {
                    Layout.fillWidth:   true
                    label:              fact.shortDescription
                    fact:               _customSettings.gain
                }
            }

            SettingsGroupLayout {
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

                FactCheckBoxSlider {
                    Layout.fillWidth:   true
                    text:               fact.shortDescription
                    fact:               _customSettings.allowMultiTagDetection
                }
            }
        }
    }
}
