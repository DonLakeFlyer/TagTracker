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

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls
import QGroundControl.FactSystem
import QGroundControl.MultiVehicleManager
import QGroundControl.ScreenTools
import QGroundControl.Palette
import QGroundControl.SettingsManager
import MAVLink

Item {
    id:                 control
    anchors.margins:    -ScreenTools.defaultFontPixelHeight / 2
    anchors.top:        parent.top
    anchors.bottom:     parent.bottom
    width:              rowLayout.width

    property bool showIndicator: true

    property var    activeVehicle:              QGroundControl.multiVehicleManager.activeVehicle
    property bool   controllerHeartbeatLost:    QGroundControl.corePlugin.controllerLostHeartbeat
    property var    tagDatabase:                QGroundControl.corePlugin.tagDatabase
    property var    customSettings:             QGroundControl.corePlugin.customSettings

    function selectedTagName() {
        var names = []
        for (var i = 0; i < tagDatabase.tagInfoList.count; i++) {
            var tag = tagDatabase.tagInfoList.get(i)
            if (tag.selected.rawValue) {
                names.push(tag.name.valueString)
            }
        }
        if (names.length === 0) return qsTr("No Tag")
        return names.join(", ")
    }

    Row {
        id:             rowLayout
        anchors.top:    parent.top
        anchors.bottom: parent.bottom
        spacing:        ScreenTools.defaultFontPixelWidth / 2

        Rectangle {
            height:     parent.height
            width:      ScreenTools.defaultFontPixelWidth * 15
            color:      controllerHeartbeatLost ? "red" : "green"

            QGCLabel {
                id:                     tagNameLabel
                anchors.fill:           parent
                anchors.leftMargin:     ScreenTools.defaultFontPixelWidth / 2
                anchors.rightMargin:    ScreenTools.defaultFontPixelWidth / 2
                horizontalAlignment:    Text.AlignHCenter
                verticalAlignment:      Text.AlignVCenter
                text:                   selectedTagName()
                elide:                  Text.ElideRight
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
            showExpand:         false
            waitForParameters:  false
            contentComponent:   indicatorContentComponent
        }
    }

    Component {
        id: indicatorContentComponent

        ColumnLayout {
            spacing: ScreenTools.defaultFontPixelHeight

            RowLayout {
                spacing: ScreenTools.defaultFontPixelWidth
                visible: customSettings.detectionMode.rawValue === 1

                QGCLabel { text: qsTr("Range") }

                QGCComboBox {
                    id:                 kCombo
                    Layout.fillWidth:   true

                    property var    baseLabels: ["3K", "4K", "5K+"]
                    property var    kValues:    [5, 10, 20]

                    model: {
                        var currentK = customSettings.pythonK.rawValue
                        for (var i = 0; i < kValues.length; i++) {
                            if (kValues[i] === currentK)
                                return baseLabels
                        }
                        return baseLabels.concat(["Custom: " + currentK])
                    }

                    Component.onCompleted: {
                        var currentK = customSettings.pythonK.rawValue
                        for (var i = 0; i < kValues.length; i++) {
                            if (kValues[i] === currentK) {
                                currentIndex = i
                                return
                            }
                        }
                        currentIndex = baseLabels.length
                    }

                    onActivated: (index) => {
                        if (index < kValues.length)
                            customSettings.pythonK.rawValue = kValues[index]
                    }
                }
            }

            QGCLabel {
                text:       qsTr("No Tags Specified")
                visible:    tagDatabase.tagInfoList.count === 0
            }

            GridLayout {
                rows:       tagDatabase.tagInfoList.count
                columns:    3
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
            }
        }
    }
}
