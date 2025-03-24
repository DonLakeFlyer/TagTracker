import QtQml

import QGroundControl

QtObject {
    readonly property int actionAutoDetection:              customActionStart + 1
    readonly property int actionStartDetection:             customActionStart + 2
    readonly property int actionStopDetection:              customActionStart + 3
    readonly property int actionStartRotation:              customActionStart + 4
    readonly property int actionRawCapture:                 customActionStart + 5
    readonly property int actionSaveLogs:                   customActionStart + 6
    readonly property int actionClearLogs:                  customActionStart + 7

    readonly property string autoDetectionTitle:            qsTr("Auto")
    readonly property string startDetectionTitle:           qsTr("Start Detection")
    readonly property string stopDetectionTitle:            qsTr("Stop Detection")
    readonly property string startRotationTitle:            qsTr("Rotate")
    readonly property string rawCaptureTitle:               qsTr("Airspy Capture")
    readonly property string downloadLogsTitle:             qsTr("Download")
    readonly property string saveLogsTitle:                 qsTr("Save Logs")
    readonly property string clearLogsTitle:                qsTr("Clear Logs")

    readonly property string autoDetectionMessage:          qsTr("Takeoff, rotate, return.")
    readonly property string startDetectionMessage:         qsTr("Start pulse detection for the specified tag(s).")
    readonly property string stopDetectionMessage:          qsTr("Stop all pulse detection.")
    readonly property string startRotationMessage:          qsTr("Start rotation in place.")
    readonly property string rawCaptureMessage:             qsTr("Start sdr raw capture.")
    readonly property string downloadLogsMessage:           qsTr("Download companion logs.")
    readonly property string saveLogsMessage:               qsTr("Save companion logs.")
    readonly property string clearLogsMessage:              qsTr("Clear companion logs.")

    function customConfirmAction(actionCode, actionData, mapIndicator, confirmDialog) {
        switch (actionCode) {
        case actionAutoDetection:
            confirmDialog.hideTrigger = true
            confirmDialog.title = startDetectionTitle
            confirmDialog.message = startDetectionMessage
            break
        case actionStartDetection:
            confirmDialog.hideTrigger = true
            confirmDialog.title = startDetectionTitle
            confirmDialog.message = startDetectionMessage
            break
        case actionStopDetection:
            confirmDialog.hideTrigger = true
            confirmDialog.title = stopDetectionTitle
            confirmDialog.message = stopDetectionMessage
            break
        case actionStartRotation:
            confirmDialog.hideTrigger = true
            confirmDialog.title = startRotationTitle
            confirmDialog.message = startRotationMessage
            break
        case actionRawCapture:
            confirmDialog.hideTrigger = true
            confirmDialog.title = rawCaptureTitle
            confirmDialog.message = rawCaptureMessage
            break
        case actionSaveLogs:
            confirmDialog.hideTrigger = true
            confirmDialog.title = saveLogsTitle
            confirmDialog.message = saveLogsMessage
            break
        case actionClearLogs:
            confirmDialog.hideTrigger = true
            confirmDialog.title = clearLogsTitle
            confirmDialog.message = clearLogsMessage
            break
        default:
            return false;
        }

        return true;
    }

    function customExecuteAction(actionCode, actionData, sliderOutputValue, optionCheckedode) {
        switch (actionCode) {
        case actionAutoDetection:
            QGroundControl.corePlugin.autoDetection()
            break
        case actionStartDetection:
            QGroundControl.corePlugin.startDetection()
            break
        case actionStopDetection:
            QGroundControl.corePlugin.stopDetection()
            break
        case actionStartRotation:
            QGroundControl.corePlugin.startRotation()
            break
        case actionRawCapture:
            QGroundControl.corePlugin.rawCapture()
            break
        case actionSaveLogs:
            QGroundControl.corePlugin.saveLogs()
            break
        case actionClearLogs:
            QGroundControl.corePlugin.cleanLogs()
            break
        default:
            return false;
        }

        return true;
    }
}
