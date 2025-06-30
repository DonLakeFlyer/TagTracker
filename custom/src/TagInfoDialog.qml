import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls
import QGroundControl.ScreenTools

import TagTracker

QGCPopupDialog {
    id:         tagInfoDialog
    title:      qsTr("Tag Info")
    buttons:    Dialog.Ok

    property var tagInfo

    property var _tagDatabase:      QGroundControl.corePlugin.tagDatabase
    property var _manufacturerList: _tagDatabase.tagManufacturerList

    onAccepted: {
        if (tagInfo.name.rawValue === "") {
            mainWindow.showMessageDialog(qsTr("Error"), qsTr("Tag name cannot be empty"))
            tagInfoDialog.preventClose = true
            return
        }
        if (_tagDatabase.tagNameExists(tagInfo)) {
            mainWindow.showMessageDialog(qsTr("Error"), qsTr("Tag name already exists"))
            tagInfoDialog.preventClose = true
            return
        }
        QGroundControl.corePlugin.tagDatabase.save()
    }
    
    ColumnLayout {
        spacing: ScreenTools.defaultFontPixelHeight / 2

        LabelledFactTextField {
            fact: tagInfo.name
        }

        LabelledFactTextField {
            fact: tagInfo.frequencyMHz
        }

        LabelledComboBox {
            id:                 manufacturerCombo
            label:              qsTr("Manufacturer")
            Layout.fillWidth:   true

            onActivated: tagInfo.manufacturerId.rawValue = _manufacturerList.get(index).id.rawValue

            Component.onCompleted: {
                var selectedIndex = -1
                var listModel = []
                for (var i=0; i<_manufacturerList.count; i++) {
                    var manufacturer = _manufacturerList.get(i)
                    var manufacturerId = manufacturer.id.rawValue
                    listModel.push(manufacturer.name.valueString)
                    if (manufacturerId === tagInfo.manufacturerId.rawValue) {
                        selectedIndex = i
                    }
                }
                manufacturerCombo.model = listModel

                if (selectedIndex == -1) {
                    console.warning("tagInfoDialogComponent: manufacturer id not found in list", manufacturerId)
                } else {
                    manufacturerCombo.currentIndex = selectedIndex
                }
            }

        }

    }
}
