import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import QGroundControl
import QGroundControl.FactSystem
import QGroundControl.FactControls
import QGroundControl.Controls
import QGroundControl.CustomControls
import QGroundControl.ScreenTools
import QGroundControl.SettingsManager

SettingsPage {
    property var  _customSettings:  QGroundControl.corePlugin.customSettings
    property var  _tagDatabase:     QGroundControl.corePlugin.tagDatabase
    property bool _isPythonMode:    _customSettings.detectionMode.rawValue === 1

    Component {
        id: tagInfoDialogComponent
        TagInfoDialog { }
    }

    Component {
        id: manufacturersDialogComponent
        TagManufacturersDialog { }
    }

    SettingsGroupLayout {
        Layout.fillWidth:   true
        heading:            qsTr("Tags")

        QGCLabel {
            text:       qsTr("No Tags Specified")
            visible:    _tagDatabase.tagInfoList.count === 0
        }

        GridLayout {
            columns:            4
            Layout.fillWidth:   true
            visible:            _tagDatabase.tagInfoList.count > 0

            Repeater {
                model: _tagDatabase.tagInfoList

                QGCLabel {
                    text:               object.name.valueString
                    Layout.row:         index
                    Layout.column:      0
                }
            }

            Repeater {
                model: _tagDatabase.tagInfoList

                QGCLabel {
                    text:               object.frequencyMHz.valueString + " MHz"
                    Layout.row:         index
                    Layout.column:      1
                }
            }

            Repeater {
                model: _tagDatabase.tagInfoList

                QGCButton {
                    text:               qsTr("Edit")
                    Layout.row:         index
                    Layout.column:      2
                    onClicked:          tagInfoDialogComponent.createObject(mainWindow, { tagInfo: object }).open()
                }
            }

            Repeater {
                model: _tagDatabase.tagInfoList

                QGCButton {
                    text:               qsTr("Del")
                    Layout.row:         index
                    Layout.column:      3
                    onClicked: {
                        _tagDatabase.deleteTagInfoListItem(object)
                        _tagDatabase.save()
                    }
                }
            }
        }

        RowLayout {
            spacing: ScreenTools.defaultFontPixelWidth

            QGCButton {
                text:       qsTr("Add Tag")
                onClicked: {
                    if (_tagDatabase.tagManufacturerList.count == 0) {
                        mainWindow.showMessageDialog(qsTr("Add Tag"), qsTr("You must add a Manufacturer first."))
                    } else {
                        tagInfoDialogComponent.createObject(mainWindow, { tagInfo: _tagDatabase.newTagInfo() }).open()
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
        Layout.fillWidth:   true
        heading:            qsTr("Flight")

        LabelledFactComboBox {
            Layout.fillWidth:   true
            label:              fact.shortDescription
            fact:               _customSettings.detectionFlightMode
        }

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
            fact:               _customSettings.antennaType
        }

        LabelledFactTextField {
            Layout.fillWidth:   true
            label:              fact.shortDescription
            fact:               _customSettings.antennaOffset
        }
    }

    SettingsGroupLayout {
        Layout.fillWidth:   true
        heading:            qsTr("Detection")

        LabelledFactComboBox {
            Layout.fillWidth:   true
            label:              fact.shortDescription
            fact:               _customSettings.detectionMode
        }

        LabelledFactTextField {
            Layout.fillWidth:   true
            label:              fact.shortDescription
            fact:               _customSettings.detectionMargin
            visible:            _isPythonMode
        }

        LabelledFactTextField {
            Layout.fillWidth:   true
            label:              fact.shortDescription
            fact:               _customSettings.confidenceRatio
            visible:            _isPythonMode
        }

        FactCheckBoxSlider {
            Layout.fillWidth:   true
            text:               fact.shortDescription
            fact:               _customSettings.debugDetector
            visible:            _isPythonMode
        }

        LabelledFactTextField {
            Layout.fillWidth:   true
            label:              fact.shortDescription
            fact:               _isPythonMode ? _customSettings.pythonK : _customSettings.k
        }

        LabelledFactTextField {
            Layout.fillWidth:   true
            label:              fact.shortDescription
            fact:               _customSettings.rotationKWaitCount
            visible:            !_isPythonMode
        }

        LabelledFactComboBox {
            Layout.fillWidth:   true
            label:              fact.shortDescription
            fact:               _customSettings.pythonFalseAlarmPreset
            visible:            _isPythonMode
        }

        LabelledFactTextField {
            Layout.fillWidth:   true
            label:              fact.shortDescription
            fact:               _customSettings.pythonFalseAlarmProbability
            visible:            _isPythonMode && _customSettings.pythonFalseAlarmPreset.rawValue === CustomSettings.Custom
        }

        LabelledFactTextField {
            Layout.fillWidth:   true
            label:              fact.shortDescription
            fact:               _customSettings.falseAlarmProbability
            visible:            !_isPythonMode
        }

        LabelledFactComboBox {
            Layout.fillWidth:   true
            label:              fact.shortDescription
            fact:               _customSettings.gain
        }

        FactCheckBoxSlider {
            Layout.fillWidth:   true
            text:               fact.shortDescription
            fact:               _customSettings.allowMultiTagDetection
        }
    }

    SettingsGroupLayout {
        Layout.fillWidth:   true
        heading:            qsTr("Display")

        LabelledFactComboBox {
            Layout.fillWidth:   true
            label:              fact.shortDescription
            fact:               _customSettings.useSNRForPulseStrength
        }

        LabelledFactTextField {
            Layout.fillWidth:   true
            label:              fact.shortDescription
            fact:               _customSettings.maxPulseStrength
        }
    }
}
