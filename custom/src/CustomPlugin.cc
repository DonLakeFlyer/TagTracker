#include "CustomPlugin.h"
#include "Vehicle.h"
#include "CustomSettings.h"
#include "QGCApplication.h"
#include "SettingsManager.h"
#include "AppSettings.h"
#include "FlyViewSettings.h"
#include "TunnelProtocol.h"
#include "DetectorInfoListModel.h"
#include "QGC.h"
#include "QGCLoggingCategory.h"
#include "FTPManager.h"
#include "MultiVehicleManager.h"
#include "MAVLinkProtocol.h"
#include "AudioOutput.h"
#include "PulseMapItem.h"
#include "PulseRoseMapItem.h"

#include "coder_array.h"
#include "bearing.h"

#include <QDebug>
#include <QPointF>
#include <QLineF>
#include <QQmlEngine>
#include <QScreen>
#include <QThread>

using namespace TunnelProtocol;

QGC_LOGGING_CATEGORY(CustomPluginLog, "CustomPluginLog")

Q_DECLARE_METATYPE(CustomPlugin::ControllerStatus)

Q_APPLICATION_STATIC(CustomPlugin, _corePluginInstance);

CustomPlugin::CustomPlugin(QObject* parent)
#ifdef TAG_TRACKER_HERELINK_BUILD
    : HerelinkCorePlugin    (parent)
#else
    : QGCCorePlugin         (parent)
#endif
    , _vehicleStateIndex    (0)
    , _flightStateMachineActive  (false)
    , _vehicleFrequency     (0)
    , _lastPulseSendIndex   (-1)
    , _missedPulseCount     (0)
{
    static_assert(TunnelProtocolValidateSizes, "TunnelProtocolValidateSizes failed");

    qmlRegisterUncreatableType<CustomPlugin>("QGroundControl", 1, 0, "CustomPlugin", "Reference only");

    _vehicleStateTimeoutTimer.setSingleShot(true);
    _tunnelCommandAckTimer.setSingleShot(true);
    _tunnelCommandAckTimer.setInterval(2000);
    _controllerHeartbeatTimer.setSingleShot(true);
    _controllerHeartbeatTimer.setInterval(6000);    // We should get heartbeats every 5 seconds

    connect(&_vehicleStateTimeoutTimer,     &QTimer::timeout, this, &CustomPlugin::_vehicleStateTimeout);
    connect(&_tunnelCommandAckTimer,        &QTimer::timeout, this, &CustomPlugin::_tunnelCommandAckFailed);
    connect(&_controllerHeartbeatTimer,     &QTimer::timeout, this, &CustomPlugin::_controllerHeartbeatFailed);
}

CustomPlugin::~CustomPlugin()
{
    _csvStopFullPulseLog();
    _csvStopRotationPulseLog(false /* calcBearing*/);
}

QGCCorePlugin *CustomPlugin::instance()
{
    return _corePluginInstance();
}

void CustomPlugin::init()
{
#ifdef TAG_TRACKER_HERELINK_BUILD
    HerelinkCorePlugin::init();
#else
    QGCCorePlugin::init();
#endif

    _customSettings             = new CustomSettings            (nullptr);
    _customOptions              = new CustomOptions             (this, nullptr);

    _tagDatabase = new TagDatabase(this);

    _csvClearPrevRotationLogs();
}

const QVariantList& CustomPlugin::toolBarIndicators(void)
{
    _toolbarIndicators = QGCCorePlugin::toolBarIndicators();

    _toolbarIndicators.append(QVariant::fromValue(QUrl::fromUserInput("qrc:/qml/ControllerIndicator.qml")));
    return _toolbarIndicators;
}

bool CustomPlugin::mavlinkMessage(Vehicle *vehicle, LinkInterface *link, const mavlink_message_t &message)
{
#ifdef TAG_TRACKER_HERELINK_BUILD
    if (!HerelinkCorePlugin::mavlinkMessage(vehicle, link, message)) {
        return false;
    }
#endif

    if (message.msgid == MAVLINK_MSG_ID_TUNNEL) {
        mavlink_tunnel_t tunnel;

        mavlink_msg_tunnel_decode(&message, &tunnel);

        HeaderInfo_t header;
        memcpy(&header, tunnel.payload, sizeof(header));

        switch (header.command) {
        case COMMAND_ID_ACK:
            _handleTunnelCommandAck(tunnel);
            break;
        case COMMAND_ID_PULSE:
            _handleTunnelPulse(vehicle, tunnel);
            break;
        case COMMAND_ID_HEARTBEAT:
            _handleTunnelHeartbeat(tunnel);
            break;
        }

        return false;
    } else {
        return true;
    }
}

void CustomPlugin::_handleTunnelHeartbeat(const mavlink_tunnel_t& tunnel)
{
    static uint32_t counter = 0;

    Heartbeat_t heartbeat;

    memcpy(&heartbeat, tunnel.payload, sizeof(heartbeat));

    switch (heartbeat.system_id) {
    case HEARTBEAT_SYSTEM_ID_MAVLINKCONTROLLER:
        qCDebug(CustomPluginLog) << "HEARTBEAT from MavlinkTagController - counter:status:temp" << counter++ << heartbeat.status << heartbeat.cpu_temp_c;
        _controllerLostHeartbeat = false;
        emit controllerLostHeartbeatChanged();
        _controllerHeartbeatTimer.start();
        if (_controllerStatus != heartbeat.status) {
            _controllerStatus = (ControllerStatus)heartbeat.status;
            emit controllerStatusChanged();
        }
        if (_controllerCPUTemp != heartbeat.cpu_temp_c) {
            _controllerCPUTemp = heartbeat.cpu_temp_c;
            emit controllerCPUTempChanged();
        }
        break;
    case HEARTBEAT_SYSTEM_ID_CHANNELIZER:
        qCDebug(CustomPluginLog) << "HEARTBEAT from Channelizer";
        break;
    }
}

void CustomPlugin::_handleTunnelCommandAck(const mavlink_tunnel_t& tunnel)
{
    AckInfo_t ack;

    memcpy(&ack, tunnel.payload, sizeof(ack));

    if (ack.command == _tunnelCommandAckExpected) {
        _tunnelCommandAckExpected = 0;
        _tunnelCommandAckTimer.stop();

        qCDebug(CustomPluginLog) << "Tunnel command ack received - command:result" << _tunnelCommandIdToText(ack.command) << ack.result;
        if (ack.result == COMMAND_RESULT_SUCCESS) {
            switch (ack.command) {
            case COMMAND_ID_START_TAGS:
            case COMMAND_ID_TAG:
                _sendNextTag();
                break;
            case COMMAND_ID_END_TAGS:
                _detectorInfoListModel.setupFromTags(_tagDatabase);
                emit _sendTagsSequenceComplete();
                break;
            case COMMAND_ID_START_DETECTION:
                _say("Detection started");
                emit _detectionStarted();
                _csvStartFullPulseLog();
                break;
            case COMMAND_ID_STOP_DETECTION:
                _say("Detection stopped");
                emit _detectionStopped();
                _csvStopFullPulseLog();
                break;
            }
        } else {
            QString message = QStringLiteral("%1 command failed").arg(_tunnelCommandIdToText(ack.command));
            _say(message);
            qgcApp()->showAppMessage(message);
            qCWarning(CustomPluginLog) << message << "- ack.result" << ack.result;

            switch (ack.command) {
                case COMMAND_ID_START_TAGS:
                case COMMAND_ID_TAG:
                case COMMAND_ID_END_TAGS:
                emit _sendTagsSequenceFailed();
                case COMMAND_ID_START_DETECTION:
                    emit _startDetectionFailed();
                    break;
                case COMMAND_ID_STOP_DETECTION:
                    emit _stopDetectionFailed();
                    break;
            }    
        }

    } else {
        qWarning() << "_handleTunnelCommandAck: Received unexpected command id ack expected:actual" <<
                      _tunnelCommandIdToText(_tunnelCommandAckExpected) <<
                      _tunnelCommandIdToText(ack.command);
    }
}

void CustomPlugin::_handleTunnelPulse(Vehicle* vehicle, const mavlink_tunnel_t& tunnel)
{
    if (tunnel.payload_length != sizeof(PulseInfo_t)) {
        qWarning() << "_handleTunnelPulse Received incorrectly sized PulseInfo payload expected:actual" <<  sizeof(PulseInfo_t) << tunnel.payload_length;
    }

    _detectorInfoListModel.handleTunnelPulse(tunnel);

    PulseInfo_t pulseInfo;
    memcpy(&pulseInfo, tunnel.payload, sizeof(pulseInfo));

    bool isDetectorHeartbeat = pulseInfo.frequency_hz == 0;
    if (pulseInfo.confirmed_status || isDetectorHeartbeat) {
        auto evenTagId  = pulseInfo.tag_id - (pulseInfo.tag_id % 2);
        auto tagInfo    = _tagDatabase->findTagInfo(evenTagId);

        if (!tagInfo) {
            qWarning() << "_handleTunnelPulse: Received pulse for unknown tag_id" << pulseInfo.tag_id;
            return;
        }

        if (!isDetectorHeartbeat) {
            qCDebug(CustomPluginLog) << Qt::fixed << qSetRealNumberPrecision(2) <<
                                        "CONFIRMED tag_id" <<
                                        pulseInfo.tag_id <<
                                        "snr" <<
                                        pulseInfo.snr <<
                                        "stft_score" <<
                                        pulseInfo.stft_score;

            _csvLogPulse(_csvFullPulseLogFile, pulseInfo);
            _csvLogPulse(_csvRotationPulseLogFile, pulseInfo);

            if (qIsNaN(_minSNR) || pulseInfo.snr < _minSNR) {
                _minSNR = pulseInfo.snr;
                emit minSNRChanged(_minSNR);
            }
            if (qIsNaN(_maxSNR) || pulseInfo.snr > _maxSNR) {
                _maxSNR = pulseInfo.snr;
                emit maxSNRChanged(_maxSNR);
            }

            // Add pulse to map
            if (_customSettings->showPulseOnMap()->rawValue().toBool() && pulseInfo.snr != 0) {
                double antennaHeading = -1;
                if (_customSettings->antennaType()->rawValue().toInt() == CustomSettings::DirectionalAntenna) {
                    double antennaOffset = _customSettings->antennaOffset()->rawValue().toDouble();
                    double vehicleHeading = vehicle->heading()->rawValue().toDouble();
                    qDebug() << "vehicleHeading" << vehicleHeading;
                    antennaHeading = fmod(vehicleHeading + antennaOffset + 360.0, 360.0);
                    qDebug() << "antennaHeading" << antennaHeading;
                }

                QUrl url = QUrl::fromUserInput("qrc:/qml/PulseMapItem.qml");
                PulseMapItem* mapItem = new PulseMapItem(url, QGeoCoordinate(pulseInfo.position_x, pulseInfo.position_y), pulseInfo.tag_id, _useSNRForPulseStrength() ? pulseInfo.snr : pulseInfo.stft_score, antennaHeading, this);
                _customMapItems.append(mapItem);
            }
        }
    } else {
        qCDebug(CustomPluginLog) << Qt::fixed << qSetRealNumberPrecision(2) <<
                                    "Uncconfirmed tag_id" <<
                                    pulseInfo.tag_id <<
                                    "snr" <<
                                    pulseInfo.snr <<
                                    "stft_score" <<
                                    pulseInfo.stft_score;
    }

}

QString CustomPlugin::_logSavePath(void)
{
    return SettingsManager::instance()->appSettings()->logSavePath();
}

void CustomPlugin::_csvStartFullPulseLog(void)
{
    if (_csvFullPulseLogFile.isOpen()) {
        qgcApp()->showAppMessage("Unabled to open full pulse csv log file - csvFile already open");
        return;
    }

    _csvFullPulseLogFile.setFileName(QString("%1/Pulse-%2.csv").arg(_logSavePath(), QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss-zzz").toLocal8Bit().data()));
    qCDebug(CustomPluginLog) << "Full CSV Pulse logging to:" << _csvFullPulseLogFile.fileName();
    if (!_csvFullPulseLogFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Unbuffered)) {
        qgcApp()->showAppMessage(QString("Open of full pulse csv log file failed: %1").arg(_csvFullPulseLogFile.errorString()));
        return;
    }
}

void CustomPlugin::_csvStopFullPulseLog(void)
{
    if (_csvFullPulseLogFile.isOpen()) {
        _csvFullPulseLogFile.close();
    }
}

void CustomPlugin::_csvClearPrevRotationLogs(void)
{
    QDir csvLogDir(_logSavePath(), {"Rotation-*.csv"});
    for (const QString & filename: csvLogDir.entryList()){
        csvLogDir.remove(filename);
    }
}

void CustomPlugin::_csvStartRotationPulseLog(int rotationCount)
{
    if (_csvRotationPulseLogFile.isOpen()) {
        qgcApp()->showAppMessage("Unabled to open rotation pulse csv log file - csvFile already open");
        return;
    }

    _csvRotationPulseLogFile.setFileName(QString("%1/Rotation-%2.csv").arg(_logSavePath()).arg(rotationCount));
    qCDebug(CustomPluginLog) << "Rotation CSV Pulse logging to:" << _csvRotationPulseLogFile.fileName();
    if (!_csvRotationPulseLogFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Unbuffered)) {
        qgcApp()->showAppMessage(QString("Open of rotation pulse csv log file failed: %1").arg(_csvRotationPulseLogFile.errorString()));
        return;
    }
}

void CustomPlugin::_csvStopRotationPulseLog(bool calcBearing)
{
    if (_csvRotationPulseLogFile.isOpen()) {
        _csvLogRotationStartStop(_csvRotationPulseLogFile, false /* startRotation */);
        _csvRotationPulseLogFile.close();

        if (calcBearing) {
            coder::array<char, 2U> rotationFileNameAsArray;
            std::string rotationFileName = _csvRotationPulseLogFile.fileName().toStdString();
            rotationFileNameAsArray.set_size(1, rotationFileName.length());
            int index = 0;
            for (auto chr : rotationFileName) {
                rotationFileNameAsArray[index++] = chr;
            }

            double calcedBearing = bearing(rotationFileNameAsArray);
            if (calcedBearing < 0) {
                calcedBearing += 360;
            }
            _rgCalcedBearings.last() = calcedBearing;
            qCDebug(CustomPluginLog) << "Calculated bearing:" << _rgCalcedBearings.last();
            emit calcedBearingsChanged();
        }
    }
}

void CustomPlugin::_csvLogPulse(QFile& csvFile, const PulseInfo_t& pulseInfo)
{
    if (csvFile.isOpen()) {
        if (csvFile.size() == 0) {
            csvFile.write(QString("# %1, tag_id, frequency_hz, start_time_seconds, predict_next_start_seconds, snr, stft_score, group_seq_counter, group_ind, group_snr, noise_psd, detection_status, confirmed_status, position_x, _y, _z, orientation_x, _y, _z, _w, antenna_offset\n")
                .arg(COMMAND_ID_PULSE)
                .toUtf8());
        }
        csvFile.write(QString("%1, %2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13, %14, %15, %16, %17, %18, %19, %20, %21\n")
            .arg(COMMAND_ID_PULSE)
            .arg(pulseInfo.tag_id)
            .arg(pulseInfo.frequency_hz)
            .arg(pulseInfo.start_time_seconds,          0, 'f', 6)
            .arg(pulseInfo.predict_next_start_seconds,  0, 'f', 6)
            .arg(pulseInfo.snr,                         0, 'f', 6)
            .arg(pulseInfo.stft_score,                  0, 'f', 6)
            .arg(pulseInfo.group_seq_counter)
            .arg(pulseInfo.group_ind)
            .arg(pulseInfo.group_snr,                   0, 'f', 6)
            .arg(pulseInfo.noise_psd,                   0, 'g', 7)
            .arg(pulseInfo.detection_status)
            .arg(pulseInfo.confirmed_status)
            .arg(pulseInfo.position_x,                  0, 'f', 6)
            .arg(pulseInfo.position_y,                  0, 'f', 6)
            .arg(pulseInfo.position_z,                  0, 'f', 6)
            .arg(pulseInfo.orientation_x,               0, 'f', 6)
            .arg(pulseInfo.orientation_y,               0, 'f', 6)
            .arg(pulseInfo.orientation_z,               0, 'f', 6)
            .arg(pulseInfo.orientation_w,               0, 'f', 6)
            .arg(_customSettings->antennaOffset()->rawValue().toDouble(), 0, 'f', 6)
            .toUtf8());
    }
}

void CustomPlugin::_csvLogRotationStartStop(QFile& csvFile, bool startRotation)
{
    Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();
    if (!vehicle) {
        qCWarning(CustomPluginLog) << "INTERNAL ERROR: _csvLogRotationStartStop - no vehicle available";
        return;
    }

    if (csvFile.isOpen()) {
        QGeoCoordinate coord = vehicle->coordinate();
        csvFile.write(QString("%1,%2,%3,%4\n").arg(startRotation ? COMMAND_ID_START_ROTATION : COMMAND_ID_STOP_ROTATION)
                      .arg(coord.latitude(), 0, 'f', 6)
                      .arg(coord.longitude(), 0, 'f', 6)
                      .arg(vehicle->altitudeAMSL()->rawValue().toDouble(), 0, 'f', 6)
                      .toUtf8());
    }
}

void CustomPlugin::_updateFlightMachineActive(bool flightMachineActive)
{
    if (_flightStateMachineActive != flightMachineActive) {
        _flightStateMachineActive = flightMachineActive;
        emit flightMachineActiveChanged(flightMachineActive);
    }
}

void CustomPlugin::cancelAndReturn(void)
{
    _say("Cancelling flight.");
    _resetStateAndRTL();
}

void CustomPlugin::autoTakeoffRotateRTL()
{
    qCDebug(CustomPluginLog) << "autoTakeoffRotateRTL";

    Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();
    if (!vehicle) {
        return;
    }

    _clearVehicleStates();

    VehicleState_t vehicleState;

    vehicleState.command = CommandSendTags;
    vehicleState.fact = nullptr;
    vehicleState.targetValueWaitMsecs = 0;
    _vehicleStates.append(vehicleState);

    vehicleState.command = CommandStartDetectors;
    vehicleState.fact = nullptr;
    vehicleState.targetValueWaitMsecs = 0;
    _vehicleStates.append(vehicleState);

    vehicleState.command = CommandTakeoff;
    vehicleState.fact = vehicle->altitudeRelative();
    vehicleState.targetValueWaitMsecs = 2 * 60 * 1000;
    vehicleState.targetValue = _customSettings->takeoffAltitude()->rawValue().toDouble();
    vehicleState.targetVariance = 1;
    _vehicleStates.append(vehicleState);

    _addRotationStates();

    vehicleState.command = CommandStopDetectors;
    vehicleState.fact = nullptr;
    vehicleState.targetValueWaitMsecs = 0;
    _vehicleStates.append(vehicleState);

    vehicleState.command = CommandRTL;
    vehicleState.fact = nullptr;
    vehicleState.targetValueWaitMsecs = 0;
    _vehicleStates.append(vehicleState);

    _updateFlightMachineActive(true);
    _advanceStateMachine();
}

void CustomPlugin::_clearVehicleStates(void)
{
    _vehicleStates.clear();
    _vehicleStateIndex = -1;

    disconnect(this, &CustomPlugin::_detectionStarted, nullptr, nullptr);
    disconnect(this, &CustomPlugin::_detectionStopped, nullptr, nullptr);
    disconnect(this, &CustomPlugin::_sendTagsSequenceComplete, nullptr, nullptr);
    disconnect(this, &CustomPlugin::_sendTagsSequenceFailed, nullptr, nullptr);
}

void CustomPlugin::_addRotationStates(void)
{
    Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();

    // Setup new rotation data
    _rgAngleStrengths.append(QList<double>());
    _rgAngleRatios.append(QList<double>());
    _rgCalcedBearings.append(qQNaN());

    QList<double>& angleStrengths = _rgAngleStrengths.last();
    QList<double>& angleRatios = _rgAngleRatios.last();

    // Prime angle strengths with no values
    _cSlice = _customSettings->divisions()->rawValue().toInt();
    for (int i=0; i<_cSlice; i++) {
        angleStrengths.append(qQNaN());
        angleRatios.append(qQNaN());
    }

    // We always start our rotation pulse captures with the antenna at 0 degrees heading
    double antennaOffset = _customSettings->antennaOffset()->rawValue().toDouble();
    double sliceDegrees = 360.0 / _cSlice;
    double nextHeading = -antennaOffset;

    // We wait at each rotation for enough time to go by to capture a user specified set of k groups
    uint32_t maxK = _customSettings->k()->rawValue().toUInt();
    auto kGroups = _customSettings->rotationKWaitCount()->rawValue().toInt();
    auto maxIntraPulseMsecs = _tagDatabase->maxIntraPulseMsecs();
    uint32_t rotationCaptureWaitMsecs = maxIntraPulseMsecs * ((kGroups * maxK) + 1);

    _currentSlice = 0;

    // Add rotation state machine entries
    for (int i=0; i<_cSlice; i++) {
        VehicleState_t vehicleState;

        if (nextHeading >= 360) {
            nextHeading -= 360;
        } else if (nextHeading < 0) {
            nextHeading += 360;
        }

        vehicleState.command                = CommandSetHeadingForRotationCapture;
        vehicleState.fact                   = vehicle->heading();
        vehicleState.targetValueWaitMsecs   = 10 * 1000;
        vehicleState.targetValue            = nextHeading;
        vehicleState.targetVariance         = 1;
        _vehicleStates.append(vehicleState);

        vehicleState.command                = CommandDelayForSteadyCapture;
        vehicleState.fact                   = nullptr;
        vehicleState.targetValueWaitMsecs   = rotationCaptureWaitMsecs;
        _vehicleStates.append(vehicleState);

        nextHeading += sliceDegrees;
    }

    _retryRotation = false;
}

void CustomPlugin::_initNewRotationDuringFlight(void)
{
    Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();

    _csvStartRotationPulseLog(_csvRotationCount++);
    _csvLogRotationStartStop(_csvFullPulseLogFile, true /* startRotation */);
    _csvLogRotationStartStop(_csvRotationPulseLogFile, true /* startRotation */);

    emit angleRatiosChanged();
    emit calcedBearingsChanged();

    // Create compass rose ui on map
    QUrl url = QUrl::fromUserInput("qrc:/qml/CustomPulseRoseMapItem.qml");
    PulseRoseMapItem* mapItem = new PulseRoseMapItem(url, _rgAngleStrengths.count() - 1, vehicle->coordinate(), this);
    _customMapItems.append(mapItem);
}

void CustomPlugin::startRotation(void)
{
    qCDebug(CustomPluginLog) << "startRotation";

    Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();

    if (!vehicle) {
        return;
    }

    _clearVehicleStates();
    _addRotationStates();
    _updateFlightMachineActive(true);
    _advanceStateMachine();
}

void CustomPlugin::_startDetection(void)
{
    StartDetectionInfo_t startDetectionInfo;

    memset(&startDetectionInfo, 0, sizeof(startDetectionInfo));

    startDetectionInfo.header.command               = COMMAND_ID_START_DETECTION;
    startDetectionInfo.radio_center_frequency_hz    = _tagDatabase->radioCenterHz();
    startDetectionInfo.gain                         = _customSettings->gain()->rawValue().toUInt();

    _sendTunnelCommand((uint8_t*)&startDetectionInfo, sizeof(startDetectionInfo));
}

void CustomPlugin::startDetection(void)
{
    disconnect(this, &CustomPlugin::_sendTagsSequenceComplete, nullptr, nullptr);
    connect(this, &CustomPlugin::_sendTagsSequenceComplete, this, &CustomPlugin::_startDetection);
    _startSendTagsSequence();
}

void CustomPlugin::stopDetection(void)
{
    StopDetectionInfo_t stopDetectionInfo;

    stopDetectionInfo.header.command = COMMAND_ID_STOP_DETECTION;
    _sendTunnelCommand((uint8_t*)&stopDetectionInfo, sizeof(stopDetectionInfo));
}

void CustomPlugin::rawCapture(void)
{
    disconnect(this, &CustomPlugin::_sendTagsSequenceComplete, nullptr, nullptr);
    connect(this, &CustomPlugin::_sendTagsSequenceComplete, this, &CustomPlugin::_rawCapture);
    _startSendTagsSequence();
}

void CustomPlugin::_rawCapture(void)
{
    RawCaptureInfo_t rawCapture;

    rawCapture.header.command   = COMMAND_ID_RAW_CAPTURE;
    rawCapture.gain             = _customSettings->gain()->rawValue().toUInt();

    _sendTunnelCommand((uint8_t*)&rawCapture, sizeof(rawCapture));
}

void CustomPlugin::sendTags(void)
{
    _startSendTagsSequence();
}

void CustomPlugin::_startSendTagsSequence(void)
{
    bool foundSelectedTag = false;
    for (int i=0; i<_tagDatabase->tagInfoListModel()->count(); i++) {
        TagInfo* tagInfo = _tagDatabase->tagInfoListModel()->value<TagInfo*>(i);
        if (tagInfo->selected()->rawValue().toUInt()) {
            foundSelectedTag = true;
            break;
        }
    }
    if (!foundSelectedTag) {
        qgcApp()->showAppMessage(("No tags are available/selected to send."));
        return;
    }

    if (!_tagDatabase->channelizerTuner()) {
        qgcApp()->showAppMessage("Channelizer tuner failed. Detectors not started");
        return;
    }

    _nextTagIndexToSend = 0;

    StartTagsInfo_t startTagsInfo;

    startTagsInfo.header.command    = COMMAND_ID_START_TAGS;

    _sendTunnelCommand((uint8_t*)&startTagsInfo, sizeof(startTagsInfo));
}

void CustomPlugin::_sendNextTag(void)
{
    // Don't send tags too fast
    QThread::msleep(100);

    auto tagInfoListModel = _tagDatabase->tagInfoListModel();

    if (_nextTagIndexToSend >= tagInfoListModel->count()) {
        _sendEndTags();
    } else {
        auto tagInfo = tagInfoListModel->value<TagInfo*>(_nextTagIndexToSend++);

        if (tagInfo->selected()->rawValue().toUInt()) {
            TunnelProtocol::TagInfo_t tunnelTagInfo;
            auto tagManufacturer = _tagDatabase->findTagManufacturer(tagInfo->manufacturerId()->rawValue().toUInt());

            memset(&tunnelTagInfo, 0, sizeof(tunnelTagInfo));

            tunnelTagInfo.header.command = COMMAND_ID_TAG;
            tunnelTagInfo.id                                        = tagInfo->id()->rawValue().toUInt();
            tunnelTagInfo.frequency_hz                              = tagInfo->frequencyHz()->rawValue().toUInt();
            tunnelTagInfo.pulse_width_msecs                         = tagManufacturer->pulse_width_msecs()->rawValue().toUInt();
            tunnelTagInfo.intra_pulse1_msecs                        = tagManufacturer->ip_msecs_1()->rawValue().toUInt();
            tunnelTagInfo.intra_pulse2_msecs                        = tagManufacturer->ip_msecs_2()->rawValue().toUInt();
            tunnelTagInfo.intra_pulse_uncertainty_msecs             = tagManufacturer->ip_uncertainty_msecs()->rawValue().toUInt();
            tunnelTagInfo.intra_pulse_jitter_msecs                  = tagManufacturer->ip_jitter_msecs()->rawValue().toUInt();
            tunnelTagInfo.k                                         = _customSettings->k()->rawValue().toUInt();
            tunnelTagInfo.false_alarm_probability                   = _customSettings->falseAlarmProbability()->rawValue().toDouble() / 100.0;
            tunnelTagInfo.channelizer_channel_number                = tagInfo->channelizer_channel_number;
            tunnelTagInfo.channelizer_channel_center_frequency_hz   = tagInfo->channelizer_channel_center_frequency_hz;
            tunnelTagInfo.ip1_mu                                    = qQNaN();
            tunnelTagInfo.ip1_sigma                                 = qQNaN();
            tunnelTagInfo.ip2_mu                                    = qQNaN();
            tunnelTagInfo.ip2_sigma                                 = qQNaN();

            _sendTunnelCommand((uint8_t*)&tunnelTagInfo, sizeof(tunnelTagInfo));
        } else {
            _sendNextTag();
        }
    }
}

void CustomPlugin::_sendEndTags(void)
{
    EndTagsInfo_t endTagsInfo;

    endTagsInfo.header.command = COMMAND_ID_END_TAGS;
    _sendTunnelCommand((uint8_t*)&endTagsInfo, sizeof(endTagsInfo));
}

void CustomPlugin::_sendCommandAndVerify(Vehicle* vehicle, MAV_CMD command, double param1, double param2, double param3, double param4, double param5, double param6, double param7)
{
    connect(vehicle, &Vehicle::mavCommandResult, this, &CustomPlugin::_mavCommandResult);
    vehicle->sendMavCommand(MAV_COMP_ID_AUTOPILOT1,
                            command,
                            false /* showError */,
                            static_cast<float>(param1),
                            static_cast<float>(param2),
                            static_cast<float>(param3),
                            static_cast<float>(param4),
                            static_cast<float>(param5),
                            static_cast<float>(param6),
                            static_cast<float>(param7));
}

void CustomPlugin::_mavCommandResult(int vehicleId, int component, int command, int result, bool noResponseFromVehicle)
{
    Q_UNUSED(vehicleId);
    Q_UNUSED(component);

    Vehicle* vehicle = dynamic_cast<Vehicle*>(sender());
    if (!vehicle) {
        qWarning() << "Vehicle dynamic cast failed!";
        return;
    }

    if (!_flightStateMachineActive) {
        disconnect(vehicle, &Vehicle::mavCommandResult, this, &CustomPlugin::_mavCommandResult);
        return;
    }

    const VehicleState_t& currentState = _vehicleStates[_vehicleStateIndex];

    if (currentState.command == CommandTakeoff && command == MAV_CMD_NAV_TAKEOFF) {
        disconnect(vehicle, &Vehicle::mavCommandResult, this, &CustomPlugin::_mavCommandResult);
        if (noResponseFromVehicle) {
            _say(QStringLiteral("Vehicle did not respond to takeoff command"));
            _updateFlightMachineActive(false);
        } else if (result != MAV_RESULT_ACCEPTED) {
            _say(QStringLiteral("takeoff command failed"));
            _updateFlightMachineActive(false);
        }
    } else if (currentState.command == CommandSetHeadingForRotationCapture && command == (vehicle->px4Firmware() ? MAV_CMD_DO_REPOSITION : MAV_CMD_CONDITION_YAW)) {
        disconnect(vehicle, &Vehicle::mavCommandResult, this, &CustomPlugin::_mavCommandResult);
        if (noResponseFromVehicle) {
            if (_retryRotation) {
                _retryRotation = false;
                _say(QStringLiteral("Vehicle did not response to Rotate command. Retrying."));
                _rotateVehicle(vehicle, _vehicleStates[_vehicleStateIndex].targetValue);
            } else {
                _say(QStringLiteral("Vehicle did not respond to Rotate command. Flight cancelled. Vehicle returning."));
                _resetStateAndRTL();
            }
        } else if (result != MAV_RESULT_ACCEPTED) {
            _say(QStringLiteral("Rotate command failed. Flight cancelled. Vehicle returning."));
            _resetStateAndRTL();
        }
    }
}

void CustomPlugin::_takeoff(Vehicle* vehicle, double takeoffAltRel)
{
    vehicle->guidedModeTakeoff(takeoffAltRel);
    return;
    
    double vehicleAltitudeAMSL = vehicle->altitudeAMSL()->rawValue().toDouble();
    if (qIsNaN(vehicleAltitudeAMSL)) {
        qgcApp()->showAppMessage(tr("Unable to takeoff, vehicle position not known."), tr("Error"));
        return;
    }



    double takeoffAltAMSL = takeoffAltRel + vehicleAltitudeAMSL;

    _sendCommandAndVerify(
                vehicle,
                MAV_CMD_NAV_TAKEOFF,
                -1,                             // No pitch requested
                0, 0,                           // param 2-4 unused
                qQNaN(), qQNaN(), qQNaN(),      // No yaw, lat, lon
                takeoffAltAMSL);                // AMSL altitude
}



void CustomPlugin::_rotateVehicle(Vehicle* vehicle, double headingDegrees)
{
    if (vehicle->px4Firmware()) {
        _sendCommandAndVerify(
            vehicle,
            MAV_CMD_DO_REPOSITION,
            -1,                                     // no change in ground speed
            MAV_DO_REPOSITION_FLAGS_CHANGE_MODE,    // switch to guided mode
            0,                                      // reserved
            qDegreesToRadians(headingDegrees),      // change heading
            qQNaN(), qQNaN(), qQNaN());             // no change lat, lon, alt
    } else if (vehicle->apmFirmware()){
        _sendCommandAndVerify(
            vehicle,
            MAV_CMD_CONDITION_YAW,
            headingDegrees,
            0,                                      // Use default angular speed
            1,                                      // Rotate clockwise
            0,                                      // heading specified as absolute angle
            0, 0, 0);                               // Unused
    }
}

void CustomPlugin::_setupDelayForSteadyCapture(void)
{
    _detectorInfoListModel.resetMaxStrength();
    _detectorInfoListModel.resetPulseGroupCount();
}

QString CustomPlugin::_holdFlightMode(void)
{
    return MultiVehicleManager::instance()->activeVehicle()->px4Firmware() ? "Hold" : "Guided";
}

void CustomPlugin::_advanceStateMachineOnSignal()
{
    disconnect(this, &CustomPlugin::_detectionStarted, nullptr, nullptr);
    disconnect(this, &CustomPlugin::_startDetectionFailed, nullptr, nullptr);
    disconnect(this, &CustomPlugin::_detectionStopped, nullptr, nullptr);
    disconnect(this, &CustomPlugin::_stopDetectionFailed, nullptr, nullptr);
    disconnect(this, &CustomPlugin::_sendTagsSequenceComplete, nullptr, nullptr);
    disconnect(this, &CustomPlugin::_sendTagsSequenceFailed, nullptr, nullptr);

    _advanceStateMachine();
}

void CustomPlugin::_advanceStateMachine(void)
{
    Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();

    if (!vehicle) {
        return;
    }

    // Clear previous state
    if (_vehicleStateIndex > 0) {
        const VehicleState_t& previousState = _vehicleStates[_vehicleStateIndex];

        if (previousState.targetValueWaitMsecs) {
            _vehicleStateTimeoutTimer.stop();
            if (previousState.fact) {
                disconnect(previousState.fact, &Fact::rawValueChanged, this, &CustomPlugin::_vehicleStateRawValueChanged);
            }
        }

        _retryRotation = false;
    }

    if (_vehicleStateIndex == _vehicleStates.count() - 1) {
        // State machine complete
        _say(QStringLiteral("Collection complete."));
        _updateFlightMachineActive(false);
        _csvStopRotationPulseLog(true /* calcBearing*/);
        return;
    }

    const VehicleState_t& currentState = _vehicleStates[++_vehicleStateIndex];
    qDebug() << "currentState.command" << currentState.command;

    if (currentState.command != CommandTakeoff && vehicle->flightMode() != "Takeoff" && vehicle->flightMode() != _holdFlightMode()) {
        // User cancel
        _say(QStringLiteral("Collection cancelled."));
        _updateFlightMachineActive(false);
        _csvStopRotationPulseLog(false /* calcBearing*/);
        return;
    }

    switch (currentState.command) {
    case CommandTakeoff:
        // Takeoff to specified altitude
        _say(QStringLiteral("Waiting for takeoff to %1 %2.").arg(FactMetaData::metersToAppSettingsVerticalDistanceUnits(currentState.targetValue).toDouble()).arg(FactMetaData::appSettingsVerticalDistanceUnitsString()));
        _takeoff(vehicle, currentState.targetValue);
        break;
    case CommandSetHeadingForRotationCapture:
        if (currentState.targetValue == 0) {
            // First slice
            _initNewRotationDuringFlight();
        }
        _retryRotation = true;
        _rotateVehicle(vehicle, currentState.targetValue);
        break;
    case CommandDelayForSteadyCapture:
        _say(QStringLiteral("Collecting data at %1 degrees.").arg(qRound(currentState.targetValue)));
        _setupDelayForSteadyCapture();
        break;
    case CommandRTL:
        _setRTLFlightModeAndValidate(vehicle);
        _say("Collection complete, returning");
        break;
    case CommandSendTags:
        connect(this, &CustomPlugin::_sendTagsSequenceComplete, this, &CustomPlugin::_advanceStateMachineOnSignal);
        connect(this, &CustomPlugin::_sendTagsSequenceFailed, this, &CustomPlugin::cancelAndReturn);
        _startSendTagsSequence();
        break;
    case CommandStartDetectors:
        connect(this, &CustomPlugin::_detectionStarted, this, &CustomPlugin::_advanceStateMachineOnSignal);
        connect(this, &CustomPlugin::_startDetectionFailed, this, &CustomPlugin::cancelAndReturn);
        _startDetection();
        break;
    case CommandStopDetectors:
        connect(this, &CustomPlugin::_detectionStopped, this, &CustomPlugin::_advanceStateMachineOnSignal);
        connect(this, &CustomPlugin::_stopDetectionFailed, this, &CustomPlugin::_advanceStateMachineOnSignal);
        stopDetection();
        break;
    }

    if (currentState.targetValueWaitMsecs) {
        _vehicleStateTimeoutTimer.setInterval(currentState.targetValueWaitMsecs);
        _vehicleStateTimeoutTimer.start();
        if (currentState.fact) {
            connect(currentState.fact, &Fact::rawValueChanged, this, &CustomPlugin::_vehicleStateRawValueChanged);
            currentState.fact->rawValueChanged(currentState.fact->rawValue());
        }
    }
}

// This will advance the state machine if the value reaches the target value
void CustomPlugin::_vehicleStateRawValueChanged(QVariant rawValue)
{
    Fact* fact = dynamic_cast<Fact*>(sender());
    if (!fact) {
        qCritical() << "Fact dynamic cast failed!";
        return;
    }

    if (!_flightStateMachineActive) {
        disconnect(fact, &Fact::rawValueChanged, this, &CustomPlugin::_vehicleStateRawValueChanged);
    }

    const VehicleState_t& currentState = _vehicleStates[_vehicleStateIndex];

    //qCDebug(CustomPluginLog) << "Waiting for value actual:wait:variance" << rawValue.toDouble() << currentState.targetValue << currentState.targetVariance;

    if (qAbs(rawValue.toDouble() - currentState.targetValue) <= currentState.targetVariance) {
        // Target value reached
        disconnect(fact, &Fact::rawValueChanged, this, &CustomPlugin::_vehicleStateRawValueChanged);
        _advanceStateMachine();
    }
}

void CustomPlugin::_say(QString text)
{
    qCDebug(CustomPluginLog) << "_say" << text;
    AudioOutput::instance()->say(text.toLower());
}

int CustomPlugin::_rawPulseToPct(double rawPulse)
{
    double maxPossiblePulse = static_cast<double>(_customSettings->maxPulseStrength()->rawValue().toDouble());
    return static_cast<int>(100.0 * (rawPulse / maxPossiblePulse));
}

void CustomPlugin::_rotationDelayComplete(void)
{
    double maxStrength = _detectorInfoListModel.maxStrength();
    qCDebug(CustomPluginLog) << "_rotationDelayComplete: max snr" << maxStrength;
    _rgAngleStrengths.last()[_currentSlice] = maxStrength;

    // Adjust the angle ratios to this new information
    maxStrength = 0;
    for (int i=0; i<_cSlice; i++) {
        if (_rgAngleStrengths.last()[i] > maxStrength) {
            maxStrength = _rgAngleStrengths.last()[i];
        }
    }
    for (int i=0; i<_cSlice; i++) {
        double angleStrength = _rgAngleStrengths.last()[i];
        if (!qIsNaN(angleStrength)) {
            _rgAngleRatios.last()[i] = _rgAngleStrengths.last()[i] / maxStrength;
        }
    }
    emit angleRatiosChanged();

    // Advance to next slice
    if (++_currentSlice >= _cSlice) {
        _currentSlice = 0;
    }

    _advanceStateMachine();
}

void CustomPlugin::_vehicleStateTimeout(void)
{
    if (_vehicleStates[_vehicleStateIndex].command == CommandDelayForSteadyCapture) {
        _rotationDelayComplete();
        return;
    } else {
        _say("Failed to reach target.");
    }
    cancelAndReturn();
}

bool CustomPlugin::_armVehicleAndValidate(Vehicle* vehicle)
{
    if (vehicle->armed()) {
        return true;
    }

    bool armedChanged = false;

    // We try arming 3 times
    for (int retries=0; retries<3; retries++) {
        vehicle->setArmed(true, false /* showError */);

        // Wait for vehicle to return armed state
        for (int i=0; i<10; i++) {
            if (vehicle->armed()) {
                armedChanged = true;
                break;
            }
            QThread::msleep(100);
            qgcApp()->processEvents(QEventLoop::ExcludeUserInputEvents);
        }
        if (armedChanged) {
            break;
        }
    }

    if (!armedChanged) {
        _say("Vehicle failed to arm");
    }

    return armedChanged;
}

bool CustomPlugin::_setRTLFlightModeAndValidate(Vehicle* vehicle)
{
    QString rtlFlightMode = vehicle->rtlFlightMode();

    if (vehicle->flightMode() == rtlFlightMode) {
        return true;
    }

    bool flightModeChanged = false;

    // We try 3 times
    for (int retries=0; retries<3; retries++) {
        vehicle->setFlightMode(rtlFlightMode);

        // Wait for vehicle to return flight mode
        for (int i=0; i<10; i++) {
            if (vehicle->flightMode() == rtlFlightMode) {
                flightModeChanged = true;
                break;
            }
            QThread::msleep(100);
            qgcApp()->processEvents(QEventLoop::ExcludeUserInputEvents);
        }
        if (flightModeChanged) {
            break;
        }
    }

    if (!flightModeChanged) {
        _say("Vehicle failed to respond to Return command");
    }

    return flightModeChanged;
}

void CustomPlugin::_resetStateAndRTL(void)
{
    qCDebug(CustomPluginLog) << "_resetStateAndRTL";
    _vehicleStateTimeoutTimer.stop();

    Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();
    if (vehicle) {
        disconnect(vehicle, &Vehicle::mavCommandResult, this, &CustomPlugin::_mavCommandResult);
    }

    for (const VehicleState_t& vehicleState: _vehicleStates) {
        if (vehicleState.fact) {
            disconnect(vehicleState.fact, &Fact::rawValueChanged, this, &CustomPlugin::_vehicleStateRawValueChanged);
        }
    }

    _setRTLFlightModeAndValidate(vehicle);

    _updateFlightMachineActive(false);

    _csvStopFullPulseLog();
    _csvStopRotationPulseLog(false /* calcBearing*/);
}

bool CustomPlugin::adjustSettingMetaData(const QString& settingsGroup, FactMetaData& metaData)
{
    if (settingsGroup == AppSettings::settingsGroup && metaData.name() == AppSettings::batteryPercentRemainingAnnounceName) {
        metaData.setRawDefaultValue(20);
    } else if (settingsGroup == FlyViewSettings::settingsGroup && metaData.name() == FlyViewSettings::showSimpleCameraControlName) {
        metaData.setRawDefaultValue(false);
    }

#ifdef TAG_TRACKER_HERELINK_BUILD
    return HerelinkCorePlugin::adjustSettingMetaData(settingsGroup, metaData);
#else
    return true;
#endif
}

QString CustomPlugin::_tunnelCommandIdToText(uint32_t vhfCommandId)
{
    switch (vhfCommandId) {
    case COMMAND_ID_TAG:
        return QStringLiteral("tag send");
    case COMMAND_ID_START_TAGS:
        return QStringLiteral("start tag send");
    case COMMAND_ID_END_TAGS:
        return QStringLiteral("end tag send");
    case COMMAND_ID_PULSE:
        return QStringLiteral("pulse");
    case COMMAND_ID_RAW_CAPTURE:
        return QStringLiteral("raw capture");
    case COMMAND_ID_START_DETECTION:
        return QStringLiteral("start detection");
    case COMMAND_ID_STOP_DETECTION:
        return QStringLiteral("stop detection");
    case COMMAND_ID_SAVE_LOGS:
        return QStringLiteral("save logs");
    case COMMAND_ID_CLEAN_LOGS:
        return QStringLiteral("clean logs");
    default:
        return QStringLiteral("unknown command:%1").arg(vhfCommandId);
    }
}

void CustomPlugin::_tunnelCommandAckFailed(void)
{
    QString message = QStringLiteral("%1 failed. no response from vehicle.").arg(_tunnelCommandIdToText(_tunnelCommandAckExpected));

    _say(QStringLiteral("%1 failed. no response from vehicle.").arg(_tunnelCommandIdToText(_tunnelCommandAckExpected)));
    qgcApp()->showAppMessage(message);

    _tunnelCommandAckExpected = 0;
}

void CustomPlugin::_sendTunnelCommand(uint8_t* payload, size_t payloadSize)
{
    Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();
    if (!vehicle) {
        qCDebug(CustomPluginLog) << "_sendTunnelCommand called with no vehicle active";
        return;
    }

    WeakLinkInterfacePtr    weakPrimaryLink     = vehicle->vehicleLinkManager()->primaryLink();

    if (!weakPrimaryLink.expired()) {
        SharedLinkInterfacePtr  sharedLink  = weakPrimaryLink.lock();
        MAVLinkProtocol*        mavlink     = MAVLinkProtocol::instance();
        mavlink_message_t       msg;
        mavlink_tunnel_t        tunnel;

        memset(&tunnel, 0, sizeof(tunnel));

        HeaderInfo_t tunnelHeader;
        memcpy(&tunnelHeader, payload, sizeof(tunnelHeader));

        _tunnelCommandAckExpected = tunnelHeader.command;
        _tunnelCommandAckTimer.start();

        memcpy(tunnel.payload, payload, payloadSize);

        tunnel.target_system    = vehicle->id();
        tunnel.target_component = MAV_COMP_ID_ONBOARD_COMPUTER;
        tunnel.payload_type     = MAV_TUNNEL_PAYLOAD_TYPE_UNKNOWN;
        tunnel.payload_length   = payloadSize;

        mavlink_msg_tunnel_encode_chan(
                    static_cast<uint8_t>(mavlink->getSystemId()),
                    static_cast<uint8_t>(mavlink->getComponentId()),
                    sharedLink->mavlinkChannel(),
                    &msg,
                    &tunnel);

        vehicle->sendMessageOnLinkThreadSafe(sharedLink.get(), msg);
    }


}

QmlObjectListModel* CustomPlugin::customMapItems(void)
{
    return &_customMapItems;
}

void CustomPlugin::_controllerHeartbeatFailed()
{
    _controllerLostHeartbeat = true;
    emit controllerLostHeartbeatChanged();
}

void CustomPlugin::downloadLogDirList(void)
{
    Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();
    if (!vehicle) {
        return;
    }

    auto ftpManager = vehicle->ftpManager();
    connect(ftpManager, &FTPManager::listDirectoryComplete, this, &CustomPlugin::_logDirListDownloaded);
    if (!ftpManager->listDirectory(MAV_COMP_ID_ONBOARD_COMPUTER, ".")) {
        qCDebug(CustomPluginLog) << "downloadLogDirList: returned false";
        emit logDirListDownloaded(QStringList(), QStringLiteral("download failed"));
    }
}

void CustomPlugin::_logDirListDownloaded(const QStringList& dirList, const QString& errorMsg)
{
    disconnect(qobject_cast<FTPManager*>(sender()), &FTPManager::listDirectoryComplete, this, &CustomPlugin::_logDirListDownloaded);

    if (!errorMsg.isEmpty()) {
        qCDebug(CustomPluginLog) << "_logDirListDownloaded: error" << errorMsg;
        emit logDirListDownloaded(QStringList(), errorMsg);
        return;
    }
    
    QStringList logDirectories;

    // For each entry in the dirList, look for a directories which start with "Logs-"
    for (const QString& entry: dirList) {
        if (entry.startsWith("DLogs-")) {
            logDirectories.append(entry.last(entry.length() - 1));
        }
    }
    
    emit logDirListDownloaded(logDirectories, errorMsg);
}

void CustomPlugin::downloadLogDirFiles(const QString& logDir)
{
    Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();
    if (!vehicle) {
        return;
    }

    qCDebug(CustomPluginLog) << "downloadLogDirFiles - logDir" << logDir;

    QString logSavePath = QStringLiteral("%1/%2").arg(_logSavePath(), logDir);
    qCDebug(CustomPluginLog) << "downloadLogDirFiles - requesting download - logDir:logSavePath" << logDir << logSavePath;

    // Delete any previous download directory
    QDir qgcLogDir(_logSavePath());
    if (qgcLogDir.exists(logDir)) {
        QDir logSaveDir(logSavePath);
        qCDebug(CustomPluginLog) << "downloadLogDirFiles - removing existing directory" << logSaveDir.path();
        if (!logSaveDir.removeRecursively()) {
            qCDebug(CustomPluginLog) << "downloadLogDirFiles - removeRecursively: returned false";
            emit downloadLogDirFilesComplete(QStringLiteral("removeRecursively failed"));
            return;
        }
    }

    // Create new download directory
    if (!qgcLogDir.mkdir(logDir)) {
        qCDebug(CustomPluginLog) << "downloadLogDirFiles - mkdir: returned false";
        emit downloadLogDirFilesComplete(QStringLiteral("mkdir failed"));
        return;
    }

    auto ftpManager = vehicle->ftpManager();
    connect(ftpManager, &FTPManager::listDirectoryComplete, this, &CustomPlugin::_logDirDownloadedForFiles);
    _logDirPathOnVehicle = logDir;
    if (!ftpManager->listDirectory(MAV_COMP_ID_ONBOARD_COMPUTER, logDir)) {
        qCDebug(CustomPluginLog) << "downloadLogDirFiles - listDirectory: returned false";
        emit downloadLogDirFilesComplete(QStringLiteral("listDirectory failed"));
    }
}

void CustomPlugin::_logDirDownloadedForFiles(const QStringList& dirList, const QString& errorMsg)
{
    qCDebug(CustomPluginLog) << "_logDirDownloadedForFiles - dirList:errorMsg" << dirList << errorMsg;

    disconnect(qobject_cast<FTPManager*>(sender()), &FTPManager::listDirectoryComplete, this, &CustomPlugin::_logDirDownloadedForFiles);

    if (!errorMsg.isEmpty()) {
        qCDebug(CustomPluginLog) << "_logDirDownloadedForFiles: error" << errorMsg;
        emit downloadLogDirFilesComplete(errorMsg);
        return;
    }
    
    // For each entry in the dirList, look for a directories which start with "Logs-"
    _logFileDownloadList.clear();
    for (const QString& entry: dirList) {
        if (entry.startsWith("F") && !entry.startsWith("F.") && !entry.startsWith("Fdata_record") && !entry.startsWith("Fspectro_segment")) {
            // Note MavlinkTagController only sends file name. It doesn't include file size.
            _logFileDownloadList.append(entry.last(entry.length() - 1));
        }
    }
    qCDebug(CustomPluginLog) << "_logDirDownloadedForFiles: _logFileDownloadList" << _logFileDownloadList;
    
    if (_logFileDownloadList.count() == 0) {
        qCDebug(CustomPluginLog) << "_logDirDownloadedForFiles: no files found";
        emit downloadLogDirFilesComplete(QStringLiteral("no files found"));
        return;
    }

    _curLogFileDownloadIndex = 0;
    _logFilesDownloadWorker();
}

void CustomPlugin::_logFilesDownloadWorker(void)
{
    Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();
    if (!vehicle) {
        return;
    }

    auto ftpManager = vehicle->ftpManager();
    connect(ftpManager, &FTPManager::downloadComplete, this, &CustomPlugin::_logFileDownloadComplete);

    QString logSavePath = QStringLiteral("%1/%2").arg(_logSavePath(), _logDirPathOnVehicle);
    QString logFilePath = QStringLiteral("%1/%2").arg(_logDirPathOnVehicle, _logFileDownloadList[_curLogFileDownloadIndex]);
    qCDebug(CustomPluginLog) << "_logFilesDownloadWorker - requesting download - logSavePath:logFilePath" << logSavePath << logFilePath;

    if (!ftpManager->download(MAV_COMP_ID_ONBOARD_COMPUTER, logFilePath, logSavePath)) {
        qCDebug(CustomPluginLog) << "_logFilesDownloadWorker - download: returned false";
        emit downloadLogDirFilesComplete(QStringLiteral("download failed"));
        return;
    }
}

void CustomPlugin::_logFileDownloadComplete(const QString& file, const QString& errorMsg)
{
    disconnect(qobject_cast<FTPManager*>(sender()), &FTPManager::downloadComplete, this, &CustomPlugin::_logFileDownloadComplete);

    if (!errorMsg.isEmpty()) {
        qCDebug(CustomPluginLog) << QStringLiteral("_logFileDownloadComplete: failed to download file(%1) - error(%2)").arg(file).arg(errorMsg);
        emit downloadLogDirFilesComplete(errorMsg);
        return;
    }

    qCDebug(CustomPluginLog) << "_logFileDownloadComplete: downloaded file successfully -" << file;

    _curLogFileDownloadIndex++;
    if (_curLogFileDownloadIndex < _logFileDownloadList.count()) {
        _logFilesDownloadWorker();
    } else {
        emit downloadLogDirFilesComplete(QString());
    }
}

void CustomPlugin::_captureScreen(void)
{
    QString saveFile = QString("%1/Screen-%2.jpg").arg(_logSavePath(), QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss-zzz").toLocal8Bit().data());

    qCDebug(CustomPluginLog) << "captureScreenshot: saveFile" << saveFile;

    QScreen *screen = QGuiApplication::primaryScreen();
    QPixmap map = screen->grabWindow(0);
    if (!map.save(saveFile, "JPG")) {
        qCDebug(CustomPluginLog) << "captureScreenshot: save failed";
    }
}

void CustomPlugin::captureScreen(void)
{
    // We need to delay the screen capture to allow the dialog to close
    QTimer::singleShot(500, [this]() { _captureScreen(); } );
}

void CustomPlugin::saveLogs()
{
    SaveLogsInfo_t saveLogsInfo;

    saveLogsInfo.header.command = COMMAND_ID_SAVE_LOGS;
    _sendTunnelCommand((uint8_t*)&saveLogsInfo, sizeof(saveLogsInfo));
}

void CustomPlugin::cleanLogs()
{
    CleanLogsInfo_t cleanLogsInfo;

    cleanLogsInfo.header.command = COMMAND_ID_CLEAN_LOGS;
    _sendTunnelCommand((uint8_t*)&cleanLogsInfo, sizeof(cleanLogsInfo));
}

void CustomPlugin::clearMap()
{
    _customMapItems.clearAndDeleteContents();
    _minSNR = qQNaN();
    _maxSNR = qQNaN();
    emit minSNRChanged(_minSNR);
    emit maxSNRChanged(_maxSNR);
}