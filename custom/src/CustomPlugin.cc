#include "CustomPlugin.h"
#include "CustomLoggingCategory.h"
#include "CustomSettings.h"
#include "TagDatabase.h"
#include "SendTagsState.h"
#include "CustomStateMachine.h"
#include "SendTunnelCommandState.h"
#include "FullRotateAndCaptureState.h"
#include "SmartRotateAndCaptureState.h"
#include "PythonRotateAndCaptureState.h"
#include "TakeoffState.h"
#include "SetFlightModeState.h"
#include "StartDetectionState.h"
#include "StopDetectionState.h"
#include "FunctionState.h"
#include "SayState.h"
#include "RotateMavlinkCommandState.h"
#include "RotateAndRateHeartbeatWaitState.h"
#include "RotationInfo.h"

#include "Vehicle.h"
#include "QGCApplication.h"
#include "SettingsManager.h"
#include "AppSettings.h"
#include "FlyViewSettings.h"
#include "TunnelProtocol.h"
#include "DetectorList.h"
#include "QGC.h"
#include "FTPManager.h"
#include "MultiVehicleManager.h"
#include "MAVLinkProtocol.h"
#include "AudioOutput.h"
#include "PulseMapItem.h"
#include "PulseRoseMapItem.h"

#include <QDebug>
#include <QPointF>
#include <QLineF>
#include <QQmlEngine>
#include <QScreen>
#include <QThread>
#include <QFinalState>
#include <algorithm>

using namespace TunnelProtocol;

Q_DECLARE_METATYPE(CustomPlugin::ControllerStatus)

Q_APPLICATION_STATIC(CustomPlugin, _corePluginInstance);

CustomPlugin::CustomPlugin(QObject* parent)
#ifdef TAG_TRACKER_HERELINK_BUILD
    : HerelinkCorePlugin    (parent)
#else
    : QGCCorePlugin         (parent)
#endif
    , _vehicleFrequency     (0)
    , _lastPulseSendIndex   (-1)
    , _missedPulseCount     (0)
{
    static_assert(TunnelProtocolValidateSizes, "TunnelProtocolValidateSizes failed");

    qmlRegisterUncreatableType<CustomPlugin>("QGroundControl", 1, 0, "CustomPlugin", "Reference only");

    _controllerHeartbeatTimer.setSingleShot(true);
    _controllerHeartbeatTimer.setInterval(6000);    // We should get heartbeats every 5 seconds

    connect(&_controllerHeartbeatTimer, &QTimer::timeout, this, &CustomPlugin::_controllerHeartbeatFailed);
}

CustomPlugin::~CustomPlugin()
{
    _csvLogManager.csvStopFullPulseLog();
    _csvLogManager.csvStopRotationPulseLog();
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

    _customSettings = new CustomSettings(nullptr);
    _customOptions = new CustomOptions(this, nullptr);

    _csvLogManager.csvClearPrevRotationLogs();
}

TagDatabase* CustomPlugin::tagDatabase()
{
    return TagDatabase::instance();
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
        case COMMAND_ID_PULSE:
            _handleTunnelPulse(vehicle, tunnel);
            return false;
        case COMMAND_ID_HEARTBEAT:
            _handleTunnelHeartbeat(tunnel);
            return false;
        case COMMAND_ID_BEARING_RESULT:
            _handleBearingResult(tunnel);
            return false;
        }
    }

    return true;
}

void CustomPlugin::_handleTunnelHeartbeat(const mavlink_tunnel_t& tunnel)
{
    Heartbeat_t heartbeat;

    memcpy(&heartbeat, tunnel.payload, sizeof(heartbeat));

    switch (heartbeat.system_id) {
    case HEARTBEAT_SYSTEM_ID_MAVLINKCONTROLLER:
        //qCDebug(CustomPluginLog) << "HEARTBEAT from MavlinkTagController - status:temp" << controllerStatusString(heartbeat.status) << heartbeat.cpu_temp_c;
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

void CustomPlugin::_handleTunnelPulse(Vehicle* vehicle, const mavlink_tunnel_t& tunnel)
{
    if (tunnel.payload_length != sizeof(PulseInfo_t)) {
        qWarning() << "_handleTunnelPulse Received incorrectly sized PulseInfo payload expected:actual" <<  sizeof(PulseInfo_t) << tunnel.payload_length;
        return;
    }

    detectorList()->handleTunnelPulse(tunnel);

    PulseInfo_t pulseInfo;
    memcpy(&pulseInfo, tunnel.payload, sizeof(pulseInfo));

    bool isDetectorHeartbeat = pulseInfo.frequency_hz == 0;
    const bool isPythonMode = _customSettings->detectionMode()->rawValue().toUInt() == DETECTION_MODE_PYTHON;
    const bool isNoPulseResult = !isDetectorHeartbeat && pulseInfo.detection_status == kNoPulseDetectionStatus;
    const bool isLowConfidencePulse = !isDetectorHeartbeat && !pulseInfo.confirmed_status && !isNoPulseResult;

    if (pulseInfo.confirmed_status || isDetectorHeartbeat || (isPythonMode && (isLowConfidencePulse || isNoPulseResult))) {
        if (!isDetectorHeartbeat) {
            qCDebug(CustomPluginLog) << Qt::fixed << qSetRealNumberPrecision(2) <<
                (pulseInfo.confirmed_status ? "Confirmed pulse tag_id" : (isNoPulseResult ? "No-pulse tag_id" : "Low-confidence pulse tag_id")) <<
                pulseInfo.tag_id <<
                "snr" <<
                pulseInfo.snr <<
                "heading" <<
                pulseInfo.yaw_deg <<
                "stft_score" <<
                pulseInfo.stft_score <<
                "detection_status" <<
                pulseInfo.detection_status <<
                "group_seq_counter" <<
                pulseInfo.group_seq_counter <<
                "group_ind" <<
                pulseInfo.group_ind <<
                "noise_psd" <<
                pulseInfo.noise_psd <<
                "predict_next" <<
                pulseInfo.predict_next_start_seconds <<
                "at time" <<
                pulseInfo.start_time_seconds;
        }

        if (_activeRotation) {
            // Send pulse info to current rotation info, including low-confidence pulses in Python mode.
            auto rotationInfo = _rotationInfoList.value<RotationInfo*>(_rotationInfoList.count() - 1);
            if (rotationInfo) {
                rotationInfo->pulseInfoReceived(pulseInfo);
            } else {
                qWarning() << "_handleTunnelPulse: No rotation info available - " << Q_FUNC_INFO;
            }
        }

        auto evenTagId  = pulseInfo.tag_id - (pulseInfo.tag_id % 2);
        auto tagInfo    = TagDatabase::instance()->findTagInfo(evenTagId);

        if (!tagInfo) {
            qWarning() << "_handleTunnelPulse: Received pulse for unknown tag_id" << pulseInfo.tag_id;
            return;
        }

        if (isDetectorHeartbeat) {
            emit detectorHeartbeatReceived(pulseInfo.tag_id % 2 ? 1 : 2 /* oneBaseRateIndex */);
        } else {
            // Notify Python rotation state machine that this detector has a result
            if (isPythonMode) {
                emit pythonDetectorResultReceived(pulseInfo.tag_id);
            }

            if (pulseInfo.confirmed_status) {
                _csvLogManager.csvLogPulse(pulseInfo);

                if (qIsNaN(_minSNR) || pulseInfo.snr < _minSNR) {
                    _minSNR = pulseInfo.snr;
                    emit minSNRChanged(_minSNR);
                }
                if (qIsNaN(_maxSNR) || pulseInfo.snr > _maxSNR) {
                    _maxSNR = pulseInfo.snr;
                    emit maxSNRChanged(_maxSNR);
                }

                // Add pulse to map
                if (_customSettings->detectionFlightMode()->rawValue().toUInt() == CustomSettings::SurveyDetection && pulseInfo.snr != 0) {
                    double antennaHeading = -1;
                    if (_customSettings->antennaType()->rawValue().toInt() == CustomSettings::DirectionalAntenna) {
                        double antennaOffset = _customSettings->antennaOffset()->rawValue().toDouble();
                        double vehicleHeading = vehicle->heading()->rawValue().toDouble();
                        qDebug() << "vehicleHeading" << vehicleHeading;
                        antennaHeading = fmod(vehicleHeading + antennaOffset + 360.0, 360.0);
                        qDebug() << "antennaHeading" << antennaHeading;
                    }

                    QUrl url = QUrl::fromUserInput("qrc:/qml/PulseMapItem.qml");
                    PulseMapItem* mapItem = new PulseMapItem(url, QGeoCoordinate(pulseInfo.latitude, pulseInfo.longitude), pulseInfo.tag_id, _useSNRForPulseStrength() ? pulseInfo.snr : pulseInfo.stft_score, antennaHeading, this);
                    _customMapItems.append(mapItem);
                }
            }
        }
    }

}

void CustomPlugin::autoDetection()
{
    qCDebug(CustomPluginLog) << Q_FUNC_INFO;

    if (!_validateAtLeastOneTagSelected()) {
        return;
    }

    // We always stop detection on disarm
    connect(MultiVehicleManager::instance()->activeVehicle(), &Vehicle::armedChanged, this, &CustomPlugin::_stopDetectionOnDisarmed);

    auto stateMachine = new CustomStateMachine("Auto Detection", this);

    AirspyStatusInfo_t airspyStatusInfo;
    memset(&airspyStatusInfo, 0, sizeof(airspyStatusInfo));
    airspyStatusInfo.header.command = COMMAND_ID_AIRSPY_STATUS;

    auto announceAutoStartState = new SayState("AnnounceAuto", stateMachine, "Starting auto detection");
    auto airspyStatusState      = new SendTunnelCommandState("AirspyStatusCheck", stateMachine, reinterpret_cast<uint8_t*>(&airspyStatusInfo), sizeof(airspyStatusInfo));
    auto takeoffState           = new TakeoffState(stateMachine, _customSettings->takeoffAltitude()->rawValue().toDouble());
    auto eventModeRTLState      = new FunctionState("eventModeRTLState", stateMachine, [stateMachine] () { stateMachine->setEventMode(CustomStateMachine::CancelOnFlightModeChange | CustomStateMachine::RTLOnError); });
    const bool isPythonMode     = _customSettings->detectionMode()->rawValue().toUInt() == DETECTION_MODE_PYTHON;
    auto startDetectionState    = new StartDetectionState(stateMachine, !isPythonMode);
    auto stopDetectionState     = new StopDetectionState(stateMachine, !isPythonMode);
    CustomState* rotateAndCaptureState = isPythonMode
                                        ? static_cast<CustomState*>(new PythonRotateAndCaptureState(stateMachine))
                                        : static_cast<CustomState*>(new FullRotateAndCaptureState(stateMachine));
    auto announceAutoEndState   = new SayState("AnnounceAutoEnd", stateMachine, "Auto detection complete. Returning");
    auto eventModeNoneState     = new FunctionState("eventModeNoneState", stateMachine, [stateMachine] () { stateMachine->setEventMode(0); });
    auto rtlState               = new SetFlightModeState(stateMachine, MultiVehicleManager::instance()->activeVehicle()->rtlFlightMode());
    auto finalState             = new QFinalState(stateMachine);

    // Transitions
    announceAutoStartState->addTransition   (announceAutoStartState,    &SayState::functionCompleted,       airspyStatusState);
    airspyStatusState->addTransition        (airspyStatusState,         &SendTunnelCommandState::commandSucceeded, startDetectionState);
    startDetectionState->addTransition      (startDetectionState,       &CustomState::finished,             takeoffState);
    takeoffState->addTransition             (takeoffState,              &TakeoffState::takeoffComplete,     eventModeRTLState);
    eventModeRTLState->addTransition        (eventModeRTLState,         &FunctionState::functionCompleted,  rotateAndCaptureState);
    rotateAndCaptureState->addTransition    (rotateAndCaptureState,     &QState::finished,                  eventModeNoneState);
    eventModeNoneState->addTransition       (eventModeNoneState,        &FunctionState::functionCompleted,  stopDetectionState);
    stopDetectionState->addTransition       (stopDetectionState,        &CustomState::finished,             announceAutoEndState);
    announceAutoEndState->addTransition     (announceAutoEndState,      &SayState::functionCompleted,       rtlState);
    rtlState->addTransition                 (rtlState,                  &QState::finished,                  finalState);

    if (isPythonMode) {
        stateMachine->registerStopHandler([this]() { _sendStopDetectionDirect(); });
    }

    stateMachine->setInitialState(announceAutoStartState);
    stateMachine->start();
}

void CustomPlugin::startRotation(void)
{
    qCDebug(CustomPluginLog) << Q_FUNC_INFO;

    auto stateMachine = new CustomStateMachine("Start Rotation", this);
    stateMachine->setEventMode(CustomStateMachine::CancelOnFlightModeChange);

    // States

    bool needStartDetection = _controllerStatus != ControllerStatusDetecting;
    const bool isPythonMode   = _customSettings->detectionMode()->rawValue().toUInt() == DETECTION_MODE_PYTHON;

    StartDetectionState* startDetectionState = nullptr;
    StopDetectionState* stopDetectionState = nullptr;
    if (needStartDetection) {
        startDetectionState = new StartDetectionState(stateMachine, !isPythonMode);
        stopDetectionState = new StopDetectionState(stateMachine, !isPythonMode);
    }
    auto finalState = new QFinalState(stateMachine);

    CustomState* rotateState = nullptr;
    if (_customSettings->detectionMode()->rawValue().toUInt() == DETECTION_MODE_PYTHON) {
        rotateState = new PythonRotateAndCaptureState(stateMachine);
    } else if (_customSettings->rotationType()->rawValue().toInt() == 1) {
        rotateState = new FullRotateAndCaptureState(stateMachine);
    } else {
        rotateState = new SmartRotateAndCaptureState(stateMachine);
    }

    // Transitions
    if (needStartDetection) {
        startDetectionState->addTransition(startDetectionState, &CustomState::finished, rotateState);
        rotateState->addTransition(rotateState, &QState::finished, stopDetectionState);
        stopDetectionState->addTransition(stopDetectionState, &CustomState::finished, finalState);
    } else {
        rotateState->addTransition(rotateState, &CustomState::finished, finalState);
    }

    if (isPythonMode) {
        stateMachine->registerStopHandler([this]() { _sendStopDetectionDirect(); });
    }

    stateMachine->setInitialState(startDetectionState ? startDetectionState : rotateState);

    stateMachine->start();
}

void CustomPlugin::startDetection(void)
{
    auto stateMachine = new CustomStateMachine("Start Detection", this);

    auto startDetectionState = new StartDetectionState(stateMachine);
    auto finalState = new QFinalState(stateMachine);

    // Transitions
    startDetectionState->addTransition(startDetectionState, &CustomState::finished, finalState);

    stateMachine->setInitialState(startDetectionState);

    stateMachine->start();
}

void CustomPlugin::stopDetection(void)
{
    auto stateMachine = new CustomStateMachine("Stop Detection", this);

    auto stopDetectionState = new StopDetectionState(stateMachine);
    auto finalState = new QFinalState(stateMachine);

    // Transitions
    stopDetectionState->addTransition(stopDetectionState, &CustomState::finished, finalState);

    stateMachine->setInitialState(stopDetectionState);

    stateMachine->start();
}

void CustomPlugin::rawCapture(void)
{
    auto tagDatabase        = TagDatabase::instance();
    auto tagInfoListModel   = tagDatabase->tagInfoListModel();

    // Find the first selected tag
    TagInfo* selectedTagInfo = nullptr;
    for (int i = 0; i < tagInfoListModel->count(); i++) {
        auto tagInfo = tagInfoListModel->value<TagInfo*>(i);
        if (tagInfo->selected()->rawValue().toUInt()) {
            selectedTagInfo = tagInfo;
            break;
        }
    }
    if (!selectedTagInfo) {
        qCWarning(CustomPluginLog) << Q_FUNC_INFO << "No selected tag found for raw capture";
        return;
    }

    RawCaptureInfo_t rawCapture = {};

    rawCapture.header.command   = COMMAND_ID_RAW_CAPTURE;
    rawCapture.gain             = _customSettings->gain()->rawValue().toUInt();
    rawCapture.frequency_hz     = selectedTagInfo->frequencyMHz()->rawValue().toDouble() * 1000000;

    auto stateMachine = new CustomStateMachine("RawCapture", this);

    auto finalState = new QFinalState(stateMachine);

    auto sendTagsState          = new SendTagsState(stateMachine);
    auto sendRawCaptureState    = new SendTunnelCommandState("RawCaptureCommand", stateMachine, (uint8_t*)&rawCapture, sizeof(rawCapture));

    // Send Tags -> Raw Capture
    sendTagsState->addTransition(sendTagsState, &QState::finished, sendRawCaptureState);

    // Raw Capure -> Final State
    sendRawCaptureState->addTransition(sendRawCaptureState, &SendTunnelCommandState::commandSucceeded, finalState);

    stateMachine->setInitialState(sendTagsState);

    stateMachine->start();
}

void CustomPlugin::saveLogs()
{
    SaveLogsInfo_t saveLogsInfo;

    saveLogsInfo.header.command = COMMAND_ID_SAVE_LOGS;

    auto stateMachine = new CustomStateMachine("SaveLogs", this);

    auto sendSaveLogsState  = new SendTunnelCommandState("SaveLogsCommand", stateMachine, (uint8_t*)&saveLogsInfo, sizeof(saveLogsInfo));
    auto finalState         = new QFinalState(stateMachine);

    // Send Save Logs -> Final State
    sendSaveLogsState->addTransition(sendSaveLogsState, &SendTunnelCommandState::commandSucceeded, finalState);

    stateMachine->setInitialState(sendSaveLogsState);

    stateMachine->start();
}

void CustomPlugin::cleanLogs()
{
    CleanLogsInfo_t cleanLogsInfo;

    cleanLogsInfo.header.command = COMMAND_ID_CLEAN_LOGS;

    auto stateMachine = new CustomStateMachine("CleanLogs", this);

    auto sendCleanLogsState = new SendTunnelCommandState("ClearLogsCommand", stateMachine, (uint8_t*)&cleanLogsInfo, sizeof(cleanLogsInfo));
    auto finalState         = new QFinalState(stateMachine);

    // Send Clean Logs -> Final State
    sendCleanLogsState->addTransition(sendCleanLogsState, &SendTunnelCommandState::commandSucceeded, finalState);

    stateMachine->setInitialState(sendCleanLogsState);

    stateMachine->start();
}

QString CustomPlugin::holdFlightMode(void)
{
    return MultiVehicleManager::instance()->activeVehicle()->px4Firmware() ? "Hold" : "Guided";
}

void CustomPlugin::_say(QString text)
{
    qCDebug(CustomPluginLog) << "_say" << text;
    AudioOutput::instance()->say(text.toLower());
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

QmlObjectListModel* CustomPlugin::customMapItems(void)
{
    return &_customMapItems;
}

void CustomPlugin::_controllerHeartbeatFailed()
{
    _controllerLostHeartbeat = true;
    emit controllerLostHeartbeatChanged();
}

void CustomPlugin::_captureScreen(void)
{
    QString saveFile = QString("%1/Screen-%2.jpg").arg(_csvLogManager.logSavePath(), QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss-zzz").toLocal8Bit().data());

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

void CustomPlugin::clearMap()
{
    _customMapItems.clearAndDeleteContents();
    _minSNR = qQNaN();
    _maxSNR = qQNaN();
    emit minSNRChanged(_minSNR);
    emit maxSNRChanged(_maxSNR);
}

void CustomPlugin::_sendStopDetectionDirect()
{
    Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();
    if (!vehicle) {
        return;
    }

    WeakLinkInterfacePtr weakLink = vehicle->vehicleLinkManager()->primaryLink();
    if (weakLink.expired()) {
        return;
    }

    qCDebug(CustomPluginLog) << "Sending direct STOP_ROTATION_DETECTION (Python mode cleanup)" << " - " << Q_FUNC_INFO;

    StopRotationDetection_t stopRotationDetection;
    memset(&stopRotationDetection, 0, sizeof(stopRotationDetection));
    stopRotationDetection.header.command = COMMAND_ID_STOP_ROTATION_DETECTION;

    SharedLinkInterfacePtr  sharedLink  = weakLink.lock();
    MAVLinkProtocol*        mavlink     = MAVLinkProtocol::instance();
    mavlink_message_t       msg;
    mavlink_tunnel_t        tunnel;

    memset(&tunnel, 0, sizeof(tunnel));
    memcpy(tunnel.payload, &stopRotationDetection, sizeof(stopRotationDetection));

    tunnel.target_system    = vehicle->id();
    tunnel.target_component = MAV_COMP_ID_ONBOARD_COMPUTER;
    tunnel.payload_type     = MAV_TUNNEL_PAYLOAD_TYPE_UNKNOWN;
    tunnel.payload_length   = sizeof(stopRotationDetection);

    mavlink_msg_tunnel_encode_chan(
                static_cast<uint8_t>(mavlink->getSystemId()),
                static_cast<uint8_t>(mavlink->getComponentId()),
                sharedLink->mavlinkChannel(),
                &msg,
                &tunnel);

    vehicle->sendMessageOnLinkThreadSafe(sharedLink.get(), msg);

    // Match StopDetectionState cleanup: stop logging and clear detector list
    _csvLogManager.csvStopFullPulseLog();
    DetectorList::instance()->clear();
}

void CustomPlugin::_stopDetectionOnDisarmed(bool armed)
{
    if (!armed && _controllerStatus == ControllerStatusDetecting) {
        stopDetection();
    }
}

bool CustomPlugin::_validateAtLeastOneTagSelected()
{
    auto tagDatabase = TagDatabase::instance();
    auto tagInfoListModel = tagDatabase->tagInfoListModel();

    for (int i=0; i<tagInfoListModel->count(); i++) {
        TagInfo* tagInfo = tagInfoListModel->value<TagInfo*>(i);
        if (tagInfo->selected()->rawValue().toUInt()) {
            return true;
        }
    }

    qgcApp()->showAppMessage(tr("At least one tag must be selected"));

    return false;
}

void CustomPlugin::rotationIsStarting()
{
    _setActiveRotation(true);

    // Setup up new RotationInfo
    int cSlices = _customSettings->divisions()->rawValue().toInt();
    auto rotationInfo = new RotationInfo(cSlices, this);
    _rotationInfoList.append(rotationInfo);

    CSVLogManager& logManager = csvLogManager();
    logManager.csvStartRotationPulseLog();
    logManager.csvLogRotationStart();

    // Create compass rose ui on map
    QUrl url = QUrl::fromUserInput("qrc:/qml/CustomPulseRoseMapItem.qml");
    PulseRoseMapItem* mapItem = new PulseRoseMapItem(url, _rotationInfoList.count() - 1, MultiVehicleManager::instance()->activeVehicle()->coordinate(), this);
    customMapItems()->append(mapItem);
}

void CustomPlugin::rotationIsEnding()
{
    if (_activeRotation) {
        _setActiveRotation(false);

        // In C++ detector mode, fit bearing locally. In Python mode the controller
        // computes the bearing and sends COMMAND_ID_BEARING_RESULT.
        const bool isPythonMode = _customSettings->detectionMode()->rawValue().toUInt() == DETECTION_MODE_PYTHON;
        if (!isPythonMode && _rotationInfoList.count() > 0) {
            auto* rotationInfo = _rotationInfoList.value<RotationInfo*>(_rotationInfoList.count() - 1);
            if (rotationInfo) {
                rotationInfo->fitBearing();
            }
        }

        csvLogManager().csvLogRotationStop();
        csvLogManager().csvStopRotationPulseLog();
    }
}

void CustomPlugin::_handleBearingResult(const mavlink_tunnel_t& tunnel)
{
    if (tunnel.payload_length != sizeof(BearingResult_t)) {
        qWarning() << "_handleBearingResult: Received incorrectly sized BearingResult payload expected:actual" << sizeof(BearingResult_t) << tunnel.payload_length;
        return;
    }

    BearingResult_t bearingResult;
    memcpy(&bearingResult, tunnel.payload, sizeof(bearingResult));

    qCDebug(CustomPluginLog) << "BearingResult received - tag_id:bearing:r2:nValid:bestSNR"
                             << bearingResult.tag_id
                             << bearingResult.bearing_deg
                             << bearingResult.r_squared
                             << bearingResult.n_valid_slices
                             << bearingResult.best_snr;

    if (_rotationInfoList.count() > 0) {
        auto* rotationInfo = _rotationInfoList.value<RotationInfo*>(_rotationInfoList.count() - 1);
        if (rotationInfo) {
            rotationInfo->setBearingResult(bearingResult.bearing_deg, bearingResult.r_squared,
                                           bearingResult.n_valid_slices, bearingResult.best_snr);
        }
    }
}

void CustomPlugin::_setActiveRotation(bool active)
{
    if (_activeRotation != active) {
        _activeRotation = active;
        emit activeRotationChanged(active);
    }
}

int CustomPlugin::maxWaitMSecsForKGroup()
{
    const bool isPythonMode = _customSettings->detectionMode()->rawValue().toUInt() == DETECTION_MODE_PYTHON;
    uint32_t maxK           = isPythonMode ? _customSettings->pythonK()->rawValue().toUInt()
                                           : _customSettings->k()->rawValue().toUInt();
    auto maxIntraPulseMsecs = TagDatabase::instance()->maxIntraPulseMsecs();

    if (isPythonMode) {
        return maxIntraPulseMsecs * (maxK + 1);
    } else {
        auto kGroups = _customSettings->rotationKWaitCount()->rawValue().toInt();
        return maxIntraPulseMsecs * ((kGroups * maxK) + 1);
    }
}

double CustomPlugin::normalizeHeading(double heading)
{
    heading = fmod(heading, 360.0);
    if (heading < 0.0) {
        heading += 360.0;
    }
    return heading;
}

const char* CustomPlugin::controllerStatusString(int status)
{
    switch (status) {
    case ControllerStatusIdle:          return "Idle";
    case ControllerStatusReceivingTags: return "ReceivingTags";
    case ControllerStatusHasTags:       return "HasTags";
    case ControllerStatusDetecting:     return "Detecting";
    case ControllerStatusCapture:       return "Capture";
    default:                            return "Unknown";
    }
}
