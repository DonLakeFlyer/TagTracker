import QtQuick
import QGroundControl
import QGroundControl.Controls

// Rotation compass rose: one wedge per heading slice, north up. width sets the rose size.
Rectangle {
    id:             rose
    height:         width
    radius:         width / 2
    color:          "transparent"
    border.color:   borderColor
    border.width:   1

    property var    rotationInfo                // RotationInfo, must not be null
    property color  borderColor:    "white"

    property int    _sliceCount:    rotationInfo.slices.count
    property real   _sliceSize:     360.0 / _sliceCount

    Repeater {
        model: rose._sliceCount

        Canvas {
            id:             arcCanvas
            anchors.fill:   parent
            visible:        !isNaN(strengthRatio)

            property real centerX:          width / 2
            property real centerY:          height / 2
            property real arcRadians:       (Math.PI * 2) / rose._sliceCount
            property var  sliceInfo:        rose.rotationInfo.slices.get(index)
            property bool noDetection:      isNaN(sliceInfo.displaySNR)
            property real rawStrengthRatio: rose.rotationInfo.maxSNR > 0 ? sliceInfo.displaySNR / rose.rotationInfo.maxSNR : 0
            property bool lowConfidenceOnly: sliceInfo.lowConfidenceOnly
            // Weak (<= 0 dB) Python detections still get a visible stub so they read as "signal, faint"
            property real strengthRatio:    noDetection ? 1 : Math.max(0.1, Math.min(1, rawStrengthRatio))

            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();

                var hasDetection = !noDetection;
                var sliceColor = noDetection ? "white" : (lowConfidenceOnly ? "orange" : "green");

                ctx.beginPath();
                ctx.globalAlpha = 0.75;
                ctx.fillStyle = sliceColor;
                ctx.strokeStyle = "white";
                ctx.lineWidth = 1;
                ctx.moveTo(centerX, centerY);
                ctx.arc(centerX, centerY, (width / 2) * arcCanvas.strengthRatio, 0, arcRadians, false);
                ctx.lineTo(centerX, centerY);

                if (hasDetection) {
                    ctx.fill();
                }
                ctx.stroke();
            }

            transform: Rotation {
                origin.x:   arcCanvas.centerX
                origin.y:   arcCanvas.centerY
                angle:      -90 - (360 / rose._sliceCount / 2) + ((360 / rose._sliceCount) * index)
            }

            Connections {
                target: arcCanvas.sliceInfo

                function onMaxSNRChanged(maxSNR) { arcCanvas.requestPaint() }
                function onDisplaySNRChanged(displaySNR) { arcCanvas.requestPaint() }
                function onDisplaySourceChanged(displaySource) { arcCanvas.requestPaint() }
                function onLowConfidenceOnlyChanged(lowConfidenceOnly) { arcCanvas.requestPaint() }
            }
            Connections {
                target: rose.rotationInfo

                function onMaxSNRChanged(maxSNR) { arcCanvas.requestPaint() }
            }
        }
    }

    Repeater {
        model: rose._sliceCount

        Text {
            property var  sliceInfo:        rose.rotationInfo.slices.get(index)
            property bool noDetection:      isNaN(sliceInfo.displaySNR)
            property bool lowConfidenceOnly: sliceInfo.lowConfidenceOnly
            property string displaySource:  sliceInfo.displaySource
            property real sliceAngleDeg:    -90 + (360 / rose._sliceCount) * index
            property real sliceAngleRad:    sliceAngleDeg * Math.PI / 180
            property real outerRadius:      rose.width / 2

            x:              rose.width / 2 + outerRadius * Math.cos(sliceAngleRad) - width / 2
            y:              rose.height / 2 + outerRadius * Math.sin(sliceAngleRad) - height / 2
            visible:        !noDetection && sliceInfo.displaySNR > 0
            text:           sliceInfo.displaySNR.toFixed(1) + " " + displaySource
            font.pointSize: ScreenTools.largeFontPointSize
            font.bold:      true
            color:          lowConfidenceOnly ? "orange" : "white"
            style:          Text.Outline
            styleColor:     "black"
        }
    }

    // Sector highlight: the rose slice the fitted bearing falls in.
    // Sector-level ("drive that way") is the operator's deliverable.
    Canvas {
        id:             sectorCanvas
        anchors.fill:   parent
        visible:        rose.rotationInfo.bearingSector >= 0

        property real centerX:      width / 2
        property real centerY:      height / 2
        property int  sector:       rose.rotationInfo.bearingSector
        property bool confirmed:    rose.rotationInfo.bearingConfirmed

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            var sliceRad = (Math.PI * 2) / rose._sliceCount;
            var startRad = (-90 - rose._sliceSize / 2 + rose._sliceSize * sector) * Math.PI / 180;
            ctx.beginPath();
            ctx.globalAlpha = 1.0;
            // Context2D has no setLineDash; unconfirmed is thinner and grey instead
            ctx.strokeStyle = confirmed ? "#00e000" : "#c0c0c0";
            ctx.lineWidth = confirmed ? 4 : 2;
            ctx.moveTo(centerX, centerY);
            ctx.arc(centerX, centerY, width / 2, startRad, startRad + sliceRad, false);
            ctx.lineTo(centerX, centerY);
            ctx.stroke();
        }

        Connections {
            target: rose.rotationInfo
            function onBearingChanged() { sectorCanvas.requestPaint() }
        }
    }

    // Bearing arrow overlay
    Canvas {
        id:             bearingArrowCanvas
        anchors.fill:   parent
        visible:        rose.rotationInfo.bearingValid

        property real centerX:      width / 2
        property real centerY:      height / 2
        property real arrowRadius:  width / 2
        property real bearingDeg:   rose.rotationInfo.bearingDeg
        property real rSquared:     rose.rotationInfo.bearingRSquared
        property bool confirmed:    rose.rotationInfo.bearingConfirmed

        // Unconfirmed: grey whatever the fit quality, so a one-heading lock never reads as a result
        property color arrowColor:  !confirmed ? "#c0c0c0" : (rSquared > 0.85 ? "#00e000" : (rSquared > 0.6 ? "#ffcc00" : "#ff3333"))

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();

            // Convert bearing to canvas angle (0° = north = up, clockwise positive)
            var bearingRad = (bearingDeg - 90) * Math.PI / 180;

            // Draw bearing arrow line
            var tipX = centerX + arrowRadius * Math.cos(bearingRad);
            var tipY = centerY + arrowRadius * Math.sin(bearingRad);

            ctx.beginPath();
            ctx.globalAlpha = 1.0;
            ctx.strokeStyle = arrowColor;
            ctx.lineWidth = 3;
            ctx.moveTo(centerX, centerY);
            ctx.lineTo(tipX, tipY);
            ctx.stroke();

            // Draw arrowhead
            var headLen = arrowRadius * 0.12;
            var headAngle = 0.4;
            ctx.beginPath();
            ctx.fillStyle = arrowColor;
            ctx.moveTo(tipX, tipY);
            ctx.lineTo(tipX - headLen * Math.cos(bearingRad - headAngle),
                       tipY - headLen * Math.sin(bearingRad - headAngle));
            ctx.lineTo(tipX - headLen * Math.cos(bearingRad + headAngle),
                       tipY - headLen * Math.sin(bearingRad + headAngle));
            ctx.closePath();
            ctx.fill();
        }

        Connections {
            target: rose.rotationInfo
            function onBearingChanged() { bearingArrowCanvas.requestPaint() }
        }
    }

    // Bearing text label: sector, bearing, fit quality and outcome state.
    // Shown once a BEARING_RESULT has arrived, including "nothing heard"
    // and "heard, no bearing".
    Text {
        anchors.horizontalCenter:   parent.horizontalCenter
        anchors.bottom:             parent.bottom
        anchors.bottomMargin:       -ScreenTools.defaultFontPixelHeight * 2
        visible:                    rose.rotationInfo.bearingReceived
        text: {
            if (!rose.rotationInfo.bearingValid) {
                return rose.rotationInfo.bearingHeard ? qsTr("Heard, no bearing") : qsTr("Nothing heard");
            }
            let label = qsTr("Sector %1").arg(rose.rotationInfo.bearingSector + 1);
            label += "  BRG " + rose.rotationInfo.bearingDeg.toFixed(0) + "°";
            label += "  R² " + rose.rotationInfo.bearingRSquared.toFixed(2);
            label += "  " + rose.rotationInfo.bearingStateText;
            return label;
        }
        font.pointSize: ScreenTools.largeFontPointSize
        font.bold:      true
        color:          !rose.rotationInfo.bearingValid ? "white"
                        : !rose.rotationInfo.bearingConfirmed ? "#c0c0c0"
                        : rose.rotationInfo.bearingRSquared > 0.85 ? "#00e000" : (rose.rotationInfo.bearingRSquared > 0.6 ? "#ffcc00" : "#ff3333")
        style:          Text.Outline
        styleColor:     "black"
    }
}
