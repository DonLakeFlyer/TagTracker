/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import QtLocation
import QtPositioning
import QtQuick.Window
import QtQml.Models

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView
import QGroundControl.FlightMap
import QGroundControl.CustomControls
// To implement a custom overlay copy this code to your own control in your custom code source. Then override the
// FlyViewCustomLayer.qml resource with your own qml. See the custom example and documentation for details.
Item {
    id: _root

    property var occluders              // FlyViewOccluders: where the upstream widgets are, use these to position your controls
    property var mapControl

    // Rects of the controls in this layer which cover the map, so the map keeps the vehicle out from under them
    property var customOccluders: [
        _occluderRect(controllerOperationCard, rightColumn.x, rightColumn.y),
        _occluderRect(wifiDownloadCard, rightColumn.x, rightColumn.y),
        _occluderRect(pulseOverlayBackground, rightColumn.x, rightColumn.y),
        _occluderRect(snrGradient, rightColumn.x, rightColumn.y),
        _occluderRect(gridPreviewPanel, 0, 0)
    ]

    property var _customPlugin:     QGroundControl.corePlugin
    property var _customSettings:   QGroundControl.settingsManager.customSettings
    property var _guidedController: globals.guidedControllerFlyView
    property bool _surveyDetection: _customSettings.detectionFlightMode.rawValue === CustomSettings.SurveyDetection

    // Auto Grid preview: drawn from the Grid tap through the editor and the confirm slider, and after confirm until
    // QGC draws the uploaded mission
    property var    _gridPreview:               _customPlugin.gridPreview
    property var    _confirmDialog:             _guidedController ? _guidedController.confirmDialog : null
    property var    _gridPreviewMapItems:       null
    // Stand out on satellite imagery and differ from QGC's orange mission path and red flight trail
    readonly property color _gridPathColor:     "#ff00ff"
    readonly property color _gridOutlineColor:  "#00ffff"
    readonly property color _gridStartColor:    "#00e676"
    readonly property color _gridHaloColor:     "black"

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    property real _minSNR:          _customPlugin.minSNR
    property real _maxSNR:          _customPlugin.maxSNR
    property real _snrRange:        _maxSNR - _minSNR
    property real _pixelsPerSNR:    snrGradient.height / _snrRange

    property int    _tickSNRIncrement:      _snrRange / 5 > 3 ? 5 : 1
    property int    _maxTickSNR:            Math.floor(_maxSNR / _tickSNRIncrement) * _tickSNRIncrement
    property int    _tickCount:             Math.floor(_snrRange / _tickSNRIncrement)
    property real   _tickFirstPixelY:       (_maxSNR - _maxTickSNR) * _pixelsPerSNR
    property real   _tickPixelIncrement:    _tickSNRIncrement * _pixelsPerSNR

    function _occluderRect(item, offsetX, offsetY) {
        return item.visible ? Qt.rect(offsetX + item.x, offsetY + item.y, item.width, item.height) : Qt.rect(0, 0, 0, 0)
    }

    // Only the QtLocation map engine takes map item groups
    function _addGridPreviewMapItems() {
        if (_gridPreviewMapItems || !mapControl || typeof mapControl.addMapItemGroup !== "function") {
            return
        }
        _gridPreviewMapItems = gridPreviewMapItemsComponent.createObject(mapControl)
        mapControl.addMapItemGroup(_gridPreviewMapItems)
    }

    // Confirm hides the slider before running the action, which hands the preview over, so check once that is done
    function _clearGridPreviewIfSliderClosed() {
        if (_gridPreview.confirming && !(_confirmDialog && _confirmDialog.visible)) {
            _gridPreview.clear()
        }
    }

    function _startGrid() {
        if (_gridPreview.requestConfirm()) {
            _guidedController.confirmAction(_guidedController._customController.actionStartGrid)
        }
    }

    onMapControlChanged: _addGridPreviewMapItems()

    // Declared before rightColumn so the progress cards draw over it
    SimpleRotationView {
        anchors.fill:       parent
        // Also covers FlyView's widget margin so no map shows around the edge
        anchors.margins:    -ScreenTools.defaultFontPixelWidth * 0.75
        guidedController:   _guidedController
        visible:            _customSettings.simpleRotationScreen.rawValue && _customSettings.detectionFlightMode.rawValue === CustomSettings.Auto
    }

    ColumnLayout {
        id:                     rightColumn
        anchors.top:            parent.top
        anchors.right:          parent.right
        anchors.bottom:         parent.bottom
        anchors.margins:        ScreenTools.defaultFontPixelWidth
        anchors.bottomMargin:   parent.height - Math.min(occluders.bottomRight.y, occluders.virtualJoystickRight.y) + ScreenTools.defaultFontPixelWidth
        spacing:                ScreenTools.defaultFontPixelHeight / 4

        // Progress card shared by controller operations and the WiFi log download.
        component OperationCard: Rectangle {
            id:                     operationCard
            Layout.alignment:       Qt.AlignRight
            Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 26
            Layout.preferredHeight: operationColumn.height + ScreenTools.defaultFontPixelWidth * 2
            color:                  Qt.rgba(qgcPal.window.r, qgcPal.window.g, qgcPal.window.b, 0.75)
            radius:                 ScreenTools.defaultFontPixelWidth / 2

            property string title
            property string message
            property real   fraction:   -1      // 0..1, or -1 when indeterminate
            property bool   running:    true
            property bool   failed:     false

            ColumnLayout {
                id:                 operationColumn
                anchors.left:       parent.left
                anchors.right:      parent.right
                anchors.margins:    ScreenTools.defaultFontPixelWidth
                anchors.verticalCenter: parent.verticalCenter
                spacing:            ScreenTools.defaultFontPixelHeight / 4

                QGCLabel {
                    Layout.fillWidth:   true
                    text:               operationCard.title
                    font.bold:          true
                    elide:              Text.ElideRight
                }

                Rectangle {
                    id:                     operationBar
                    Layout.fillWidth:       true
                    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 0.75
                    color:                  qgcPal.borderSubtle
                    radius:                 height / 4
                    clip:                   true

                    property bool indeterminate: operationCard.fraction < 0 && operationCard.running

                    readonly property int  _sweepMSecs:    900
                    readonly property real _sweepDistance: width - operationFill.width

                    Rectangle {
                        id:             operationFill
                        anchors.top:    parent.top
                        anchors.bottom: parent.bottom
                        radius:         parent.radius
                        color:          operationCard.failed ? "red" : qgcPal.colorGreen
                        width:          operationBar.indeterminate ? parent.width / 4 : parent.width * Math.max(0, Math.min(1, operationCard.running ? operationCard.fraction : 1))
                        x:              0

                        SequentialAnimation on x {
                            running:    operationBar.indeterminate && operationCard.visible
                            loops:      Animation.Infinite
                            onStopped:  operationFill.x = 0
                            NumberAnimation { from: 0; to: operationBar._sweepDistance; duration: operationBar._sweepMSecs; easing.type: Easing.InOutQuad }
                            NumberAnimation { from: operationBar._sweepDistance; to: 0; duration: operationBar._sweepMSecs; easing.type: Easing.InOutQuad }
                        }
                    }

                    QGCLabel {
                        anchors.centerIn:   parent
                        text:               operationCard.failed ? qsTr("Failed")
                                            : (!operationCard.running ? qsTr("Done")
                                            : (operationCard.fraction < 0 ? "" : Math.round(operationCard.fraction * 100) + "%"))
                        font.pointSize:     ScreenTools.smallFontPointSize
                        font.bold:          true
                    }
                }

                QGCLabel {
                    Layout.fillWidth:   true
                    text:               operationCard.message
                    font.pointSize:     ScreenTools.smallFontPointSize
                    elide:              Text.ElideMiddle
                    visible:            text !== "" && text !== operationCard.title
                }
            }
        }

        // Controller long-running operation (raw capture, log save/delete, detection start/stop)
        OperationCard {
            id:         controllerOperationCard

            property var _operation: _customPlugin.operationProgress

            visible:    _operation.active
            title:      _operation.title
            message:    _operation.message
            fraction:   _operation.fraction
            running:    _operation.running
            failed:     _operation.failed
        }

        // scp gives no machine-readable progress, so the WiFi download is always indeterminate.
        OperationCard {
            id:         wifiDownloadCard
            visible:    _customPlugin.companionLogDownloader.downloading
            title:      _guidedController._customController.downloadLogsTitle
            message:    qsTr("Copying companion logs over WiFi...")
        }

        Rectangle {
            id:                     pulseOverlayBackground
            Layout.alignment:       Qt.AlignRight
            Layout.preferredWidth:  pulseOverlay.width + ScreenTools.defaultFontPixelWidth * 2
            Layout.preferredHeight: pulseOverlay.height + ScreenTools.defaultFontPixelWidth * 2
            color:                  Qt.rgba(qgcPal.window.r, qgcPal.window.g, qgcPal.window.b, 0.75)
            radius:                 ScreenTools.defaultFontPixelWidth / 2
            // Rotation shows only detectors whose heartbeat was lost
            visible:                _customPlugin.detectorList.count > 0 && !_customPlugin.controllerLostHeartbeat &&
                                    (_surveyDetection || _customPlugin.detectorList.anyHeartbeatLost)

            ColumnLayout {
                id:                 pulseOverlay
                anchors.centerIn:   parent
                spacing:            2

                Repeater {
                    model: _customPlugin.detectorList

                    RowLayout {
                        visible:                    _surveyDetection || object.heartbeatLost

                        property real maxStrength:  _customSettings.maxPulseStrength.rawValue

                        Rectangle {
                            id:                     pulseRect
                            Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.5
                            Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 15
                            color:                  object.heartbeatLost ? "red" : (object.lastPulseNoPulse ? "gray" : "transparent")

                            Rectangle {
                                property real filteredSNR: Math.max(0, Math.min(object.lastPulseStrength, maxStrength))

                                anchors.rightMargin:    maxStrength <= 0 ? parent.width : ((maxStrength - filteredSNR) / maxStrength) * parent.width
                                anchors.fill:           parent
                                color:                  object.lastPulseLowConfidence ? "orange" : "green"
                                visible:                !object.heartbeatLost && !object.lastPulseNoPulse && !object.waitingForFirstPulse
                            }

                            QGCLabel {
                                anchors.fill:           parent
                                text:                   object.waitingForFirstPulse ? qsTr("Waiting...") : (object.lastPulseNoPulse ? qsTr("No Pulse ×%1").arg(object.noPulseCount) : Math.max(0, Math.min(object.lastPulseStrength, maxStrength)).toFixed(1))
                                font.bold:              true
                                color:                  (object.lastPulseNoPulse || object.heartbeatLost) ? "white" : "black"
                                horizontalAlignment:    Text.AlignHCenter
                                verticalAlignment:      Text.AlignVCenter
                            }
                        }

                        QGCLabel {
                            text:               object.rateLabel !== "" ? object.rateLabel : object.tagLabel[0]
                            visible:            !object.lastPulseNoPulse && !object.waitingForFirstPulse
                            color:              qgcPal.textPrimary
                            verticalAlignment:  Text.AlignVCenter
                        }
                    }
                }
            }
        }

        Rectangle {
            id:                 snrGradient
            Layout.alignment:   Qt.AlignRight
            width:              ScreenTools.defaultFontPixelWidth * 5
            Layout.fillHeight:  true
            visible:            _customSettings.detectionFlightMode.rawValue === CustomSettings.SurveyDetection

            gradient: Gradient {
                GradientStop { position: 0; color: "red" }
                GradientStop { position: 0.25; color: "yellow" }
                GradientStop { position: 0.5; color: "green" }
                GradientStop { position: 0.75; color: "blue" }
                GradientStop { position: 1; color: "navy" }
            }

            Repeater {
                model: _tickCount

                Rectangle {
                    anchors.horizontalCenter:   parent.horizontalCenter
                    width:                      tickLabel.contentWidth + (2 * _labelMargins)
                    height:                     tickLabel.contentHeight + (2 * _labelMargins)
                    color:                      "white"
                    y:                          _tickFirstPixelY + (index * _tickPixelIncrement)
                    radius:                     ScreenTools.defaultFontPixelWidth / 4

                    property real _labelMargins: ScreenTools.defaultFontPixelWidth / 4

                    QGCLabel {
                        id:     tickLabel
                        x:      _labelMargins
                        y:      _labelMargins
                        text:   _maxTickSNR - (index * _tickSNRIncrement)
                        color:  "black"
                    }
                }
            }
        }

        // Keeps the cards pinned to the top when the gradient is hidden
        Item {
            Layout.fillHeight:  true
            visible:            !snrGradient.visible
        }
    }

    Rectangle {
        id:         gridPreviewPanel
        objectName: "autoGridEditorPanel"
        x:          occluders.toolStrip.x + occluders.toolStrip.width + ScreenTools.defaultFontPixelWidth
        y:          occluders.toolStrip.y
        width:      ScreenTools.defaultFontPixelWidth * 36
        height:     gridPreviewColumn.height + ScreenTools.defaultFontPixelWidth * 2
        color:      Qt.rgba(qgcPal.window.r, qgcPal.window.g, qgcPal.window.b, 0.85)
        radius:     ScreenTools.defaultFontPixelWidth / 2
        visible:    _gridPreview.editing

        component GridValueRow: RowLayout {
            Layout.fillWidth:   true
            spacing:            ScreenTools.defaultFontPixelWidth

            property string label
            property string buttonPrefix  // objectName prefix for the − and + buttons
            property real   value
            property real   minValue
            property real   maxValue

            signal step(int steps)

            QGCLabel {
                Layout.fillWidth:   true
                text:               label
            }
            QGCButton {
                objectName: buttonPrefix + "Minus"
                text:       "\u2212"
                enabled:    value > minValue
                onClicked:  step(-1)
            }
            QGCLabel {
                Layout.minimumWidth:    ScreenTools.defaultFontPixelWidth * 7
                horizontalAlignment:    Text.AlignHCenter
                text:                   qsTr("%1 m").arg(value)
            }
            QGCButton {
                objectName: buttonPrefix + "Plus"
                text:       "+"
                enabled:    value < maxValue
                onClicked:  step(1)
            }
        }

        ColumnLayout {
            id:                 gridPreviewColumn
            anchors.left:       parent.left
            anchors.right:      parent.right
            anchors.top:        parent.top
            anchors.margins:    ScreenTools.defaultFontPixelWidth
            spacing:            ScreenTools.defaultFontPixelHeight / 4

            QGCLabel {
                text:       qsTr("Grid (this grid only)")
                font.bold:  true
            }

            GridValueRow {
                label:          qsTr("Width")
                buttonPrefix:   "autoGridWidth"
                value:      _gridPreview.width
                minValue:   _gridPreview.widthMin
                maxValue:   _gridPreview.widthMax
                onStep:     (steps) => _gridPreview.adjustWidth(steps)
            }

            GridValueRow {
                label:          qsTr("Depth")
                buttonPrefix:   "autoGridDepth"
                value:      _gridPreview.depth
                minValue:   _gridPreview.depthMin
                maxValue:   _gridPreview.depthMax
                onStep:     (steps) => _gridPreview.adjustDepth(steps)
            }

            GridValueRow {
                label:          qsTr("Lane spacing")
                buttonPrefix:   "autoGridSpacing"
                value:      _gridPreview.spacing
                minValue:   _gridPreview.spacingMin
                maxValue:   _gridPreview.spacingMax
                onStep:     (steps) => _gridPreview.adjustSpacing(steps)
            }

            QGCLabel {
                objectName:         "autoGridSummaryLabel"
                Layout.fillWidth:   true
                text:               _gridPreview.confirmMessage
                color:              _gridPreview.valid ? qgcPal.textPrimary : qgcPal.colorRed
                wrapMode:           Text.WordWrap
                font.pointSize:     ScreenTools.smallFontPointSize
            }

            RowLayout {
                Layout.fillWidth:   true
                spacing:            ScreenTools.defaultFontPixelWidth

                QGCButton {
                    objectName:         "autoGridStartButton"
                    Layout.fillWidth:   true
                    text:               qsTr("Start grid")
                    primary:            true
                    enabled:            _gridPreview.valid
                    onClicked:          _startGrid()
                }
                QGCButton {
                    objectName:         "autoGridCancelButton"
                    Layout.fillWidth:   true
                    text:               qsTr("Cancel")
                    onClicked:          _gridPreview.clear()
                }
            }
        }
    }

    Component {
        id: gridPreviewMapItemsComponent

        MapItemGroup {
            z: QGroundControl.zOrderTrajectoryLines

            MapPolyline {
                line.width: 5
                line.color: _root._gridHaloColor
                path:       _root._gridPreview.outline
                visible:    _root._gridPreview.active
            }
            MapPolyline {
                line.width: 2
                line.color: _root._gridOutlineColor
                path:       _root._gridPreview.outline
                visible:    _root._gridPreview.active
            }
            MapPolyline {
                line.width: 7
                line.color: _root._gridHaloColor
                path:       _root._gridPreview.path
                visible:    _root._gridPreview.active
            }
            MapPolyline {
                line.width: 4
                line.color: _root._gridPathColor
                path:       _root._gridPreview.path
                visible:    _root._gridPreview.active
            }

            // Below the vehicle icon, which sits on the origin at tap
            MapQuickItem {
                coordinate:     _root._gridPreview.origin
                anchorPoint.x:  sourceItem.width / 2
                anchorPoint.y:  -ScreenTools.defaultFontPixelHeight * 1.5
                visible:        _root._gridPreview.active
                z:              QGroundControl.zOrderMapItems

                sourceItem: Rectangle {
                    width:          startLabel.contentWidth + ScreenTools.defaultFontPixelWidth
                    height:         startLabel.contentHeight + ScreenTools.defaultFontPixelWidth / 2
                    radius:         height / 2
                    color:          _root._gridStartColor
                    border.color:   _root._gridHaloColor
                    border.width:   1

                    QGCLabel {
                        id:                 startLabel
                        anchors.centerIn:   parent
                        text:               qsTr("START")
                        color:              "black"
                        font.bold:          true
                    }
                }
            }
        }
    }

    Connections {
        target: _confirmDialog
        function onVisibleChanged() {
            // Another action's slider replaces the Grid editor
            if (_confirmDialog.visible && _confirmDialog.action !== _guidedController._customController.actionStartGrid
                    && _gridPreview.editing) {
                _gridPreview.clear()
            }
            Qt.callLater(_clearGridPreviewIfSliderClosed)
        }
    }

    Component.onCompleted: _addGridPreviewMapItems()

    Component.onDestruction: {
        if (_gridPreviewMapItems && mapControl) {
            // A map item group must leave the map before it is destroyed
            mapControl.removeMapItemGroup(_gridPreviewMapItems)
            _gridPreviewMapItems.destroy()
        }
    }
}
