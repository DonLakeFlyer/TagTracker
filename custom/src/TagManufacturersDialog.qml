import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls
import QGroundControl.CustomControls
import QGroundControl.ScreenTools

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
            onClicked:  manufacturerDialogComponent.createObject(mainWindow, { tagManufacturer: tagDatabase.newTagManufacturer() }).open()
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
            buttons:    Dialog.Ok

            property var tagManufacturer

            property var _tagDatabase: QGroundControl.corePlugin.tagDatabase

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

            Column {
                spacing: ScreenTools.defaultFontPixelHeight

                FactTextFieldGrid {
                    useNameForLabels: false
                    factList: [
                        tagManufacturer.name,
                        tagManufacturer.ip_msecs_1_id,
                        tagManufacturer.ip_msecs_1, 
                        tagManufacturer.ip_msecs_2_id,
                        tagManufacturer.ip_msecs_2, 
                        tagManufacturer.pulse_width_msecs, 
                        tagManufacturer.ip_uncertainty_msecs,
                        tagManufacturer.ip_jitter_msecs,
                    ]
                }
            }
        }
    }
}
