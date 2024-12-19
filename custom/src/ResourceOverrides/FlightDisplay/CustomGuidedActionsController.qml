import QtQml

import QGroundControl

QtObject {
    readonly property int actionSendTags:                   customActionStart + 1
    readonly property int actionStartDetection:             customActionStart + 2
    readonly property int actionStopDetection:              customActionStart + 3
    readonly property int actionStartRotation:              customActionStart + 4
    readonly property int actionRawCapture:                 customActionStart + 5
    readonly property int actionDownloadLogs:               customActionStart + 6

    readonly property string sendTagsTitle:                 qsTr("Tagsꜛ")
    readonly property string startDetectionTitle:           qsTr("Start")
    readonly property string stopDetectionTitle:            qsTr("Stop")
    readonly property string startRotationTitle:            qsTr("Rotate")
    readonly property string rawCaptureTitle:               qsTr("Capture")
    readonly property string downloadLogsTitle:             qsTr("Download")

    readonly property string sendTagsMessage:               qsTr("Send tag(s) to vehicle.")
    readonly property string startDetectionMessage:         qsTr("Start pulse detection for the specified tag(s).")
    readonly property string stopDetectionMessage:          qsTr("Stop all pulse detection.")
    readonly property string startRotationMessage:          qsTr("Start rotation in place.")
    readonly property string rawCaptureMessage:             qsTr("Start sdr raw capture.")
    readonly property string downloadLogsMessage:           qsTr("Download companion logs.")

    function customConfirmAction(actionCode, actionData, mapIndicator, confirmDialog) {
        switch (actionCode) {
        case actionSendTags:
            confirmDialog.hideTrigger = true
            confirmDialog.title = sendTagsTitle
            confirmDialog.message = sendTagsMessage
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
        case actionDownloadLogs:
            confirmDialog.hideTrigger = true
            confirmDialog.title = downloadLogsTitle
            confirmDialog.message = downloadLogsMessage
            break
        default:
            return false;
        }

        return true;
    }

    function customExecuteAction(actionCactionCode, actionData, sliderOutputValue, optionCheckedode) {
        switch (actionCode) {
        case actionSendTags:
            QGroundControl.corePlugin.sendTags()
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
        case actionDownloadLogs:
            QGroundControl.corePlugin.downloadLogs()
            break
        default:
            return false;
        }

        return true;
    }
}