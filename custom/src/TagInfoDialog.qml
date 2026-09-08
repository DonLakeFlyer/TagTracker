import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls
import QGroundControl.CustomControls

QGCPopupDialog {
    id:         tagInfoDialog
    title:      qsTr("Tag Info")
    buttons:    Dialog.Ok | Dialog.Cancel

    property var    tagInfo
    property bool   isNew:  false

    property var _tagDatabase:      QGroundControl.corePlugin.tagDatabase
    property var _manufacturerList: _tagDatabase.tagManufacturerList
    property var _originalValues:   ({})

    readonly property var _editedFacts: [ tagInfo.name, tagInfo.frequencyMHz, tagInfo.manufacturerId ]

    Component.onCompleted: {
        for (const fact of _editedFacts) {
            _originalValues[fact.name] = fact.rawValue
        }
    }

    onRejected: {
        if (isNew) {
            _tagDatabase.deleteTagInfoListItem(tagInfo)
        } else {
            for (const fact of _editedFacts) {
                fact.rawValue = _originalValues[fact.name]
            }
        }
    }

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
            fact:                       tagInfo.name
            textFieldPreferredWidth:    ScreenTools.defaultFontPixelWidth * 15
        }

        LabelledFactTextField {
            fact:                       tagInfo.frequencyMHz
            textFieldPreferredWidth:    ScreenTools.defaultFontPixelWidth * 15
        }

        LabelledComboBox {
            id:                 manufacturerCombo
            label:              qsTr("Manufacturer")
            Layout.fillWidth:   true

            onActivated: tagInfo.manufacturerId.rawValue = _manufacturerList.get(index).id.rawValue

            Component.onCompleted: {
                let selectedIndex = -1
                let listModel = []
                for (let i=0; i<_manufacturerList.count; i++) {
                    const manufacturer = _manufacturerList.get(i)
                    listModel.push(manufacturer.name.valueString)
                    if (manufacturer.id.rawValue === tagInfo.manufacturerId.rawValue) {
                        selectedIndex = i
                    }
                }
                manufacturerCombo.model = listModel

                if (selectedIndex == -1) {
                    console.warn("tagInfoDialogComponent: manufacturer id not found in list", tagInfo.manufacturerId.rawValue)
                } else {
                    manufacturerCombo.currentIndex = selectedIndex
                }
            }

        }

    }
}
