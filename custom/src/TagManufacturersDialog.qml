import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls
import QGroundControl.CustomControls

QGCPopupDialog {
    id:         manufacturersDialog
    title:      qsTr("Manufacturers")
    buttons:    Dialog.Ok

    property var tagDatabase: QGroundControl.corePlugin.tagDatabase

    ColumnLayout {
        id:         mainLayout
        spacing:    ScreenTools.defaultFontPixelHeight

        QGCButton {
            text:       qsTr("New Manufacturer")
            onClicked:  manufacturerDialogComponent.createObject(mainWindow, { tagManufacturer: tagDatabase.newTagManufacturer(), isNew: true }).open()
        }

        GridLayout {
            rows:       tagDatabase.tagManufacturerList.count + 1
            columns:    9
            flow:       GridLayout.TopToBottom

            QGCLabel { text: qsTr("Name") }
            Repeater {
                model: tagDatabase.tagManufacturerList

                QGCLabel { text: object.name.valueString }
            }

            QGCLabel { text: qsTr("Id 1") }
            Repeater {
                model: tagDatabase.tagManufacturerList

                QGCLabel { text: object.ip_msecs_1_id.valueString }
            }

            QGCLabel { text: qsTr("Rate 1") }
            Repeater {
                model: tagDatabase.tagManufacturerList

                QGCLabel { text: object.ip_msecs_1.valueString }
            }

            QGCLabel { text: qsTr("Id 2") }
            Repeater {
                model: tagDatabase.tagManufacturerList

                QGCLabel { text: object.ip_msecs_2_id.valueString }
            }

            QGCLabel { text: qsTr("Rate 2") }
            Repeater {
                model: tagDatabase.tagManufacturerList

                QGCLabel { text: object.ip_msecs_2.valueString }
            }

            QGCLabel { text: qsTr("Width") }
            Repeater {
                model: tagDatabase.tagManufacturerList

                QGCLabel { text: object.pulse_width_msecs.valueString }
            }

            QGCLabel { text: qsTr("Uncertainty") }
            Repeater {
                model: tagDatabase.tagManufacturerList

                QGCLabel { text: object.ip_uncertainty_msecs.valueString }
            }

            QGCLabel { text: qsTr("Jitter") }
            Repeater {
                model: tagDatabase.tagManufacturerList

                QGCLabel { text: object.ip_jitter_msecs.valueString }
            }

            QGCLabel { }
            Repeater {
                model: tagDatabase.tagManufacturerList

                QGCButton {
                    text:       qsTr("Edit")
                    onClicked:  manufacturerDialogComponent.createObject(mainWindow, { tagManufacturer: object }).open()
                }
            }

            QGCLabel { }
            Repeater {
                model: tagDatabase.tagManufacturerList

                QGCButton {
                    text: qsTr("Del")
                    onClicked: {
                        if (!tagDatabase.deleteTagManufacturerListItem(object)) {
                            mainWindow.showMessageDialog(qsTr("Manufacturer In Use"), qsTr("Unable to delete manufacturer. Still being referenced by tag(s)."))
                        } else {
                            QGroundControl.corePlugin.tagDatabase.save()
                        }
                    }
                }
            }
        }
    }

    Component {
        id: manufacturerDialogComponent

        QGCPopupDialog {
            id:         manufacturerDialog
            title:      qsTr("Tag Manufacturer")
            buttons:    Dialog.Ok | Dialog.Cancel

            property var    tagManufacturer
            property bool   isNew:  false

            property var _tagDatabase:      QGroundControl.corePlugin.tagDatabase
            property var _originalValues:   ({})

            readonly property var _editedFacts: [
                tagManufacturer.name,
                tagManufacturer.ip_msecs_1_id,
                tagManufacturer.ip_msecs_1,
                tagManufacturer.ip_msecs_2_id,
                tagManufacturer.ip_msecs_2,
                tagManufacturer.pulse_width_msecs,
                tagManufacturer.ip_uncertainty_msecs,
                tagManufacturer.ip_jitter_msecs,
            ]

            Component.onCompleted: {
                for (const fact of _editedFacts) {
                    _originalValues[fact.name] = fact.rawValue
                }
            }

            onRejected: {
                if (isNew) {
                    _tagDatabase.deleteTagManufacturerListItem(tagManufacturer)
                } else {
                    for (const fact of _editedFacts) {
                        fact.rawValue = _originalValues[fact.name]
                    }
                }
            }

            onAccepted: {
                if (tagManufacturer.name.rawValue === "") {
                    mainWindow.showMessageDialog(qsTr("Error"), qsTr("Manufacturer name cannot be empty"))
                    manufacturerDialog.preventClose = true
                    return
                }
                if (_tagDatabase.manufacturerNameExists(tagManufacturer)) {
                    mainWindow.showMessageDialog(qsTr("Error"), qsTr("Manufacturer name already exists"))
                    manufacturerDialog.preventClose = true
                    return
                }
                QGroundControl.corePlugin.tagDatabase.save()
            }

            ColumnLayout {
                spacing: ScreenTools.defaultFontPixelHeight / 2

                Repeater {
                    model: _editedFacts

                    LabelledFactTextField {
                        Layout.fillWidth:           true
                        fact:                       modelData
                        textFieldPreferredWidth:    ScreenTools.defaultFontPixelWidth * 15
                    }
                }
            }
        }
    }
}
