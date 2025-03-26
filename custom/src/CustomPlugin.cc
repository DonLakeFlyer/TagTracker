#include "CustomPlugin.h"
#include "CustomLoggingCategory.h"
#include "CustomSettings.h"
#include "TagDatabase.h"
#include "SendTagsState.h"
#include "CustomStateMachine.h"
#include "SendTunnelCommandState.h"
#include "FullRotateAndCaptureState.h"
#include "SmartRotateAndCaptureState.h"
#include "TakeoffState.h"
#include "SetFlightModeState.h"
#include "StartDetectionState.h"
#include "StopDetectionState.h"
#include "FunctionState.h"
#include "SayState.h"
#include "RotateMavlinkCommandState.h"
#include "RotateAndRateHeartbeatWaitState.h"

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

#include "coder_array.h"
#include "bearing.h"

#include <QDebug>
#include <QPointF>
#include <QLineF>
#include <QQmlEngine>
#include <QScreen>
#include <QThread>
#include <QFinalState>

using namespace TunnelProtocol;

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

    _controllerHeartbeatTimer.setSingleShot(true);
    _controllerHeartbeatTimer.setInterval(6000);    // We should get heartbeats every 5 seconds

    connect(&_controllerHeartbeatTimer, &QTimer::timeout, this, &CustomPlugin::_controllerHeartbeatFailed);
}

CustomPlugin::~CustomPlugin()
{
    _csvLogManager.csvStopFullPulseLog();
    _csvLogManager.csvStopRotationPulseLog(false /* calcBearing*/);
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
        }
    }

    return true;
}

void CustomPlugin::_handleTunnelHeartbeat(const mavlink_tunnel_t& tunnel)
{
    static uint32_t counter = 0;

    Heartbeat_t heartbeat;

    memcpy(&heartbeat, tunnel.payload, sizeof(heartbeat));

    switch (heartbeat.system_id) {
    case HEARTBEAT_SYSTEM_ID_MAVLINKCONTROLLER:
        //qCDebug(CustomPluginLog) << "HEARTBEAT from MavlinkTagController - counter:status:temp" << counter++ << heartbeat.status << heartbeat.cpu_temp_c;
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
    }

    detectorList()->handleTunnelPulse(tunnel);

    PulseInfo_t pulseInfo;
    memcpy(&pulseInfo, tunnel.payload, sizeof(pulseInfo));

    bool isDetectorHeartbeat = pulseInfo.frequency_hz == 0;
    if (pulseInfo.confirmed_status || isDetectorHeartbeat) {
        auto evenTagId  = pulseInfo.tag_id - (pulseInfo.tag_id % 2);
        auto tagInfo    = TagDatabase::instance()->findTagInfo(evenTagId);

        if (!tagInfo) {
            qWarning() << "_handleTunnelPulse: Received pulse for unknown tag_id" << pulseInfo.tag_id;
            return;
        }

        if (isDetectorHeartbeat) {
            emit detectorHeartbeatReceived(pulseInfo.tag_id % 2 ? 1 : 2 /* oneBaseRateIndex */);
        } else {
            qCDebug(CustomPluginLog) << Qt::fixed << qSetRealNumberPrecision(2) <<
                                        "Confirmed pulse tag_id" <<
                                        pulseInfo.tag_id <<
                                        "snr" <<
                                        pulseInfo.snr <<
                                        "heading" <<
                                        pulseInfo.orientation_z <<
                                        "stft_score" <<
                                        pulseInfo.stft_score <<
                                        "group_seq_counter" <<
                                        pulseInfo.group_seq_counter <<
                                        "at time" <<
                                        pulseInfo.start_time_seconds;

            _csvLogManager.csvLogPulse(pulseInfo);
            _updateSliceInfo(pulseInfo);

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
#if 0        
        qCDebug(CustomPluginLog) << Qt::fixed << qSetRealNumberPrecision(2) <<
                                    "Unconfirmed tag_id" <<
                                    pulseInfo.tag_id <<
                                    "snr" <<
                                    pulseInfo.snr <<
                                    "stft_score" <<
                                    pulseInfo.stft_score;
#endif                                    
    }

}

void CustomPlugin::_updateSliceInfo(const PulseInfo_t& pulseInfo)
{
    if (!_activeRotation) {
        return;
    }
    if (_rgAngleStrengths.isEmpty()) {
        qWarning() << "No angle strengths list" << " - " << Q_FUNC_INFO;
        return;
    }

    // Determine which slice this pulse applies to
    double degreesPerSlice = 360.0 / rgAngleStrengths().last().count();
    double heading = pulseInfo.orientation_z;
    double adjustedHeading = heading + degreesPerSlice / 2;
    adjustedHeading = fmod(adjustedHeading + 360.0, 360.0);
    int sliceIndex = static_cast<int>(adjustedHeading / degreesPerSlice);

    qCDebug(CustomPluginLog) << "heading" << heading << "adjustedHeading" << adjustedHeading << "sliceIndex" << sliceIndex << "snr" << pulseInfo.snr << " - " << Q_FUNC_INFO;

    if (sliceIndex < 0 || sliceIndex >= rgAngleStrengths().last().count()) {
        qWarning() << "Invalid sliceIndex" << sliceIndex;
        return;
    }
    auto& currentAngleStrengths = rgAngleStrengths().last();
    if (qIsNaN(currentAngleStrengths[sliceIndex]) || pulseInfo.snr > currentAngleStrengths[sliceIndex]) {
        // We have a new pulse which is greater than the current max for this slice
        // Update the slice and recalculate the ratios

        qCDebug(CustomPluginLog) << "New max for slice" << sliceIndex << "snr" << pulseInfo.snr << " - " << Q_FUNC_INFO;
        currentAngleStrengths[sliceIndex] = pulseInfo.snr;

        double maxOverallStrength = 0;
        for (int i=0; i<currentAngleStrengths.count(); i++) {
            if (currentAngleStrengths[i] > maxOverallStrength) {
                maxOverallStrength = currentAngleStrengths[i];
            }
        }

        // Recalc ratios based on new info
        auto& currentAngleRatios = rgAngleRatios().last();
        for (int i=0; i<currentAngleStrengths.count(); i++) {
            double angleStrength = currentAngleStrengths[i];
            if (!qIsNaN(angleStrength)) {
                currentAngleRatios[i] = currentAngleStrengths[i] / maxOverallStrength;
                //qCDebug(CustomPluginLog) << "New angle ratio for slice" << i << "ratio" << currentAngleRatios[i] << "strength" << angleStrength << "maxOverallStrength" << maxOverallStrength << " - " << Q_FUNC_INFO;
            }
        }
    
        emit angleRatiosChanged();
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

    auto announceAutoStartState = new SayState("AnnounceAuto", stateMachine, "Starting auto detection");
    auto takeoffState           = new TakeoffState(stateMachine, _customSettings->takeoffAltitude()->rawValue().toDouble());
    auto eventModeRTLState      = new FunctionState("eventModeRTLState", stateMachine, [stateMachine] () { stateMachine->setEventMode(CustomStateMachine::CancelOnFlightModeChange | CustomStateMachine::RTLOnError); });
    auto startDetectionState    = new StartDetectionState(stateMachine);
    auto rotateAndCaptureState  = new FullRotateAndCaptureState(stateMachine);
    auto stopDetectionState     = new StopDetectionState(stateMachine);
    auto announceAutoEndState   = new SayState("AnnounceAutoEnd", stateMachine, "Auto detection complete. Returning");
    auto eventModeNoneState     = new FunctionState("eventModeNoneState", stateMachine, [stateMachine] () { stateMachine->setEventMode(0); });
    auto rtlState               = new SetFlightModeState(stateMachine, MultiVehicleManager::instance()->activeVehicle()->rtlFlightMode());
    auto finalState             = new QFinalState(stateMachine);

    // Transitions
    announceAutoStartState->addTransition   (announceAutoStartState,    &SayState::functionCompleted,       takeoffState);
    takeoffState->addTransition             (takeoffState,              &TakeoffState::takeoffComplete,     eventModeRTLState);
    eventModeRTLState->addTransition        (eventModeRTLState,         &FunctionState::functionCompleted,  startDetectionState);
    startDetectionState->addTransition      (startDetectionState,       &CustomState::finished,             rotateAndCaptureState);
    rotateAndCaptureState->addTransition    (rotateAndCaptureState,     &QState::finished,                  eventModeNoneState);
    eventModeNoneState->addTransition       (eventModeNoneState,        &FunctionState::functionCompleted,  stopDetectionState);
    stopDetectionState->addTransition       (stopDetectionState,        &CustomState::finished,             announceAutoEndState);
    announceAutoEndState->addTransition     (announceAutoEndState,      &SayState::functionCompleted,       rtlState);
    rtlState->addTransition                 (rtlState,                  &QState::finished,                  finalState);

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

    StartDetectionState* startDetectionState = nullptr;
    StopDetectionState* stopDetectionState = nullptr;
    if (needStartDetection) {
        startDetectionState = new StartDetectionState(stateMachine);
        stopDetectionState = new StopDetectionState(stateMachine);
    }
    auto finalState = new QFinalState(stateMachine);

    CustomState* rotateState = nullptr;
    if (_customSettings->rotationType()->rawValue().toInt() == 1) {
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
    RawCaptureInfo_t rawCapture;

    rawCapture.header.command   = COMMAND_ID_RAW_CAPTURE;
    rawCapture.gain             = _customSettings->gain()->rawValue().toUInt();

    auto stateMachine = new CustomStateMachine("RawCapture", this);

    auto errorState = stateMachine->errorState();
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

int CustomPlugin::_rawPulseToPct(double rawPulse)
{
    double maxPossiblePulse = static_cast<double>(_customSettings->maxPulseStrength()->rawValue().toDouble());
    return static_cast<int>(100.0 * (rawPulse / maxPossiblePulse));
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

    // Setup new rotation data  
    rgAngleStrengths().append(QList<double>());
    rgAngleRatios().append(QList<double>());
    rgCalcedBearings().append(qQNaN());

    QList<double>& refAngleStrengths = rgAngleStrengths().last();
    QList<double>& refAngleRatios = rgAngleRatios().last();

    // Prime angle strengths with no values
    for (int i=0; i<_customSettings->divisions()->rawValue().toInt(); i++) {
        refAngleStrengths.append(qQNaN());
        refAngleRatios.append(qQNaN());
    }

    CSVLogManager& logManager = csvLogManager();
    logManager.csvStartRotationPulseLog();
    logManager.csvLogRotationStart();

    emit angleRatiosChanged();
    emit calcedBearingsChanged();

    // Create compass rose ui on map
    QUrl url = QUrl::fromUserInput("qrc:/qml/CustomPulseRoseMapItem.qml");
    PulseRoseMapItem* mapItem = new PulseRoseMapItem(url, rgAngleStrengths().count() - 1, MultiVehicleManager::instance()->activeVehicle()->coordinate(), this);
    customMapItems()->append(mapItem);
}

void CustomPlugin::rotationIsEnding()
{
    if (_activeRotation) {
        _setActiveRotation(false);
        csvLogManager().csvLogRotationStop();
        csvLogManager().csvStopRotationPulseLog(false /* calcBearing*/);
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
    uint32_t maxK           = _customSettings->k()->rawValue().toUInt();
    auto kGroups            = _customSettings->rotationKWaitCount()->rawValue().toInt();
    auto maxIntraPulseMsecs = TagDatabase::instance()->maxIntraPulseMsecs();

    return maxIntraPulseMsecs * ((kGroups * maxK) + 1);;
}
