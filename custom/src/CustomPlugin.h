#pragma once

#ifdef TAG_TRACKER_HERELINK_BUILD
    #include "HerelinkCorePlugin.h"
#else
    #include "QGCCorePlugin.h"
#endif
#include "QmlObjectListModel.h"
#include "CustomOptions.h"
#include "CustomSettings.h"
#include "TunnelProtocol.h"
#include "DetectorInfoListModel.h"
#include "TagDatabase.h"

#include <QElapsedTimer>
#include <QGeoCoordinate>
#include <QTimer>
#include <QLoggingCategory>
#include <QFile>

Q_DECLARE_LOGGING_CATEGORY(CustomPluginLog)

class CustomPlugin : 
#ifdef TAG_TRACKER_HERELINK_BUILD
    public HerelinkCorePlugin
#else
    public QGCCorePlugin
#endif
{
    Q_OBJECT

public:
    CustomPlugin(QObject* parent = nullptr);
    ~CustomPlugin();

    static QGCCorePlugin *instance();

    // IMPORTANT: This enum must match the heartbeat status values in TunnelProtocol.h
    Q_ENUMS(ControllerStatus)
    enum ControllerStatus {
        ControllerStatusIdle           = HEARTBEAT_STATUS_IDLE,
        ControllerStatusReceivingTags  = HEARTBEAT_STATUS_RECEIVING_TAGS,
        ControllerStatusHasTags        = HEARTBEAT_STATUS_HAS_TAGS,
        ControllerStatusDetecting      = HEARTBEAT_STATUS_DETECTING,
        ControllerStatusCapture        = HEARTBEAT_STATUS_CAPTURE
    };

    Q_PROPERTY(CustomSettings*      customSettings          READ    customSettings              CONSTANT)
    Q_PROPERTY(QList<QList<double>> angleRatios             MEMBER  _rgAngleRatios              NOTIFY angleRatiosChanged)
    Q_PROPERTY(QList<double>        calcedBearings          MEMBER  _rgCalcedBearings           NOTIFY calcedBearingsChanged)
    Q_PROPERTY(bool                 flightMachineActive     MEMBER  _flightStateMachineActive   NOTIFY flightMachineActiveChanged)
    Q_PROPERTY(bool                 controllerLostHeartbeat MEMBER  _controllerLostHeartbeat    NOTIFY controllerLostHeartbeatChanged)
    Q_PROPERTY(int                  controllerStatus        MEMBER  _controllerStatus           NOTIFY controllerStatusChanged)
    Q_PROPERTY(float                controllerCPUTemp       MEMBER  _controllerCPUTemp          NOTIFY controllerCPUTempChanged)
    Q_PROPERTY(QmlObjectListModel*  detectorInfoList        READ    detectorInfoList            CONSTANT)
    Q_PROPERTY(TagDatabase*         tagDatabase             MEMBER  _tagDatabase                CONSTANT)
    Q_PROPERTY(double               maxSNR                  MEMBER  _maxSNR                     NOTIFY maxSNRChanged)
    Q_PROPERTY(double               minSNR                  MEMBER  _minSNR                     NOTIFY minSNRChanged)

    CustomSettings*     customSettings  () { return _customSettings; }
    QmlObjectListModel* detectorInfoList() { return dynamic_cast<QmlObjectListModel*>(&_detectorInfoListModel); }

    Q_INVOKABLE void autoTakeoffRotateRTL();
    Q_INVOKABLE void startRotation      (void);
    Q_INVOKABLE void cancelAndReturn    (void);
    Q_INVOKABLE void sendTags           (void);
    Q_INVOKABLE void startDetection     (void);
    Q_INVOKABLE void stopDetection      (void);
    Q_INVOKABLE void rawCapture         (void);
    Q_INVOKABLE void downloadLogDirList (void);
    Q_INVOKABLE void downloadLogDirFiles(const QString& dirPath);
    Q_INVOKABLE void captureScreen      (void);
    Q_INVOKABLE void saveLogs           (void);
    Q_INVOKABLE void cleanLogs          (void);
    Q_INVOKABLE void clearMap           (void);

    // Overrides from QGCCorePlugin
    void                init                    (void) final;
    bool                mavlinkMessage          (Vehicle *vehicle, LinkInterface *link, const mavlink_message_t &message) final;
    QGCOptions*         options                 (void) final { return qobject_cast<QGCOptions*>(_customOptions); }
    bool                adjustSettingMetaData   (const QString& settingsGroup, FactMetaData& metaData) final;
    QmlObjectListModel* customMapItems          (void) final;
    const QVariantList& toolBarIndicators       (void) final;

signals:
    void angleRatiosChanged             (void);
    void calcedBearingsChanged          (void);
    void flightMachineActiveChanged     (bool flightMachineActive);
    void pulseInfoListsChanged          (void);
    void controllerLostHeartbeatChanged ();
    void controllerStatusChanged        ();
    void controllerCPUTempChanged       ();
    void logDirListDownloaded           (const QStringList& dirList, const QString& errorMsg);
    void downloadLogDirFilesComplete    (const QString& errorMsg);
    void maxSNRChanged                  (double maxSNR);
    void minSNRChanged                  (double minSNR);
    void _sendTagsSequenceComplete      (void);
    void _sendTagsSequenceFailed        (void);
    void _detectionStarted              (void);
    void _startDetectionFailed          (void);
    void _detectionStopped              (void);
    void _stopDetectionFailed           (void);

private slots:
    void _vehicleStateRawValueChanged   (QVariant rawValue);
    void _advanceStateMachine           (void);
    void _advanceStateMachineOnSignal   ();
    void _vehicleStateTimeout           (void);
    void _updateFlightMachineActive     (bool flightMachineActive);
    void _mavCommandResult              (int vehicleId, int component, int command, int result, bool noResponseFromVehicle);
    void _tunnelCommandAckFailed        (void);
    void _controllerHeartbeatFailed     (void);
    void _logDirListDownloaded          (const QStringList& dirList, const QString& errorMsg);
    void _logDirDownloadedForFiles      (const QStringList& dirList, const QString& errorMsg);
    void _logFileDownloadComplete       (const QString& file, const QString& errorMsg);
    void _startDetection                (void);
    void _rawCapture                    (void);

private:
    typedef enum {
        CommandSendTags,
        CommandStartDetectors,
        CommandStopDetectors,
        CommandTakeoff,
        CommandRTL,
        CommandSetHeadingForRotationCapture,
        CommandDelayForSteadyCapture,
    } VehicleStateCommand_t;

    typedef struct VehicleState_t {
        VehicleStateCommand_t   command;
        Fact*                   fact = nullptr;
        int                     targetValueWaitMsecs = 0;
        double                  targetValue = 0;
        double                  targetVariance = 0;
    } VehicleState_t;

    typedef struct HeartbeatInfo_t {
        bool    heartbeatLost               { true };
        uint    heartbeatTimerInterval      { 0 };
        QTimer  timer;
        uint    rotationDelayHeartbeatCount { 0 };
    } HeartbeatInfo_t;

    void    _handleTunnelCommandAck     (const mavlink_tunnel_t& tunnel);
    void    _handleTunnelPulse          (Vehicle* vehicle, const mavlink_tunnel_t& tunnel);
    void    _handleTunnelHeartbeat      (const mavlink_tunnel_t& tunnel);
    void    _rotateVehicle              (Vehicle* vehicle, double headingDegrees);
    void    _say                        (QString text);
    bool    _armVehicleAndValidate      (Vehicle* vehicle);
    bool    _setRTLFlightModeAndValidate(Vehicle* vehicle);
    void    _sendCommandAndVerify       (Vehicle* vehicle, MAV_CMD command, double param1 = 0.0, double param2 = 0.0, double param3 = 0.0, double param4 = 0.0, double param5 = 0.0, double param6 = 0.0, double param7 = 0.0);
    void    _takeoff                    (Vehicle* vehicle, double takeoffAltRel);
    void    _resetStateAndRTL           (void);
    int     _rawPulseToPct              (double rawPulse);
    void    _sendTunnelCommand          (uint8_t* payload, size_t payloadSize);
    QString _tunnelCommandIdToText      (uint32_t command);
    double  _pulseTimeSeconds           (void) { return _lastPulseInfo.start_time_seconds; }
    double  _pulseSNR                   (void) { return _lastPulseInfo.snr; }
    bool    _pulseConfirmed             (void) { return _lastPulseInfo.confirmed_status; }
    void    _sendNextTag                (void);
    void    _sendEndTags                (void);
    void    _setupDelayForSteadyCapture (void);
    void    _rotationDelayComplete      (void);
    QString _logSavePath                (void);
    void    _csvStartFullPulseLog       (void);
    void    _csvStopFullPulseLog        (void);
    void    _csvClearPrevRotationLogs   (void);
    void    _csvStartRotationPulseLog   (int rotationCount);
    void    _csvStopRotationPulseLog    (bool calcBearing);
    void    _csvLogPulse                (QFile& csvFile, const TunnelProtocol::PulseInfo_t& pulseInfo);
    void    _csvLogRotationStartStop    (QFile& csvFile, bool startRotation);
    void    _logFilesDownloadWorker     (void);
    bool    _useSNRForPulseStrength     (void) { return _customSettings->useSNRForPulseStrength()->rawValue().toBool(); }
    void    _captureScreen              (void);
    void    _startSendTagsSequence      (void);
    void    _addRotationStates          (void);
    void    _initNewRotationDuringFlight(void);
    void    _clearVehicleStates         (void);
    QString _holdFlightMode             (void);

    QVariantList            _settingsPages;
    QVariantList            _instrumentPages;
    int                     _vehicleStateIndex;
    QList<VehicleState_t>   _vehicleStates;
    QList<QList<double>>    _rgAngleStrengths;
    QList<QList<double>>    _rgAngleRatios;
    QList<double>           _rgCalcedBearings;
    bool                    _flightStateMachineActive;
    int                     _currentSlice;
    int                     _cSlice;
    int                     _detectionStatus    = -1;
    bool                    _retryRotation      = false;
    int                     _controllerStatus   = ControllerStatusIdle;
    float                   _controllerCPUTemp  = 0.0;
    int                     _nextTagIndexToSend = 0;

    QTimer                  _vehicleStateTimeoutTimer;
    QTimer                  _tunnelCommandAckTimer;
    uint32_t                _tunnelCommandAckExpected;
    CustomOptions*          _customOptions;
    CustomSettings*         _customSettings;
    int                     _vehicleFrequency;
    int                     _lastPulseSendIndex;
    int                     _missedPulseCount;
    QmlObjectListModel      _customMapItems;
    QFile                   _csvFullPulseLogFile;
    QFile                   _csvRotationPulseLogFile;
    int                     _csvRotationCount = 1;
    TunnelProtocol::PulseInfo_t _lastPulseInfo;
    QVariantList            _toolbarIndicators;

    DetectorInfoListModel   _detectorInfoListModel;

    TagDatabase*             _tagDatabase = nullptr;

    bool                    _controllerLostHeartbeat { true };
    QTimer                  _controllerHeartbeatTimer;

    int                     _curLogFileDownloadIndex;
    QString                 _logDirPathOnVehicle;
    QStringList             _logFileDownloadList;

    double                  _maxSNR = qQNaN();
    double                  _minSNR = qQNaN();
};
