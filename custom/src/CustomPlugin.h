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
#include "DetectorList.h"
#include "TagDatabase.h"
#include "CSVLogManager.h"

#include <QElapsedTimer>
#include <QGeoCoordinate>
#include <QTimer>
#include <QFile>

class CustomState;
class QState;

using namespace TunnelProtocol;

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
    Q_PROPERTY(QList<double>        calcedBearings          MEMBER  _rgCalcedBearings           NOTIFY calcedBearingsChanged)
    Q_PROPERTY(bool                 flightMachineActive     MEMBER  _flightStateMachineActive   NOTIFY flightMachineActiveChanged)
    Q_PROPERTY(bool                 controllerLostHeartbeat MEMBER  _controllerLostHeartbeat    NOTIFY controllerLostHeartbeatChanged)
    Q_PROPERTY(int                  controllerStatus        MEMBER  _controllerStatus           NOTIFY controllerStatusChanged)
    Q_PROPERTY(float                controllerCPUTemp       MEMBER  _controllerCPUTemp          NOTIFY controllerCPUTempChanged)
    Q_PROPERTY(QmlObjectListModel*  detectorList            READ    detectorList                CONSTANT)
    Q_PROPERTY(TagDatabase*         tagDatabase             READ    tagDatabase                 CONSTANT)
    Q_PROPERTY(double               maxSNR                  MEMBER  _maxSNR                     NOTIFY maxSNRChanged)
    Q_PROPERTY(double               minSNR                  MEMBER  _minSNR                     NOTIFY minSNRChanged)
    Q_PROPERTY(bool                 activeRotation          MEMBER  _activeRotation             NOTIFY activeRotationChanged)
    Q_PROPERTY(QmlObjectListModel*  rotationInfoList        READ    rotationInfoList          CONSTANT)

    CustomSettings*     customSettings  () { return _customSettings; }
    DetectorList *      detectorList() { return DetectorList::instance(); }
    QString             holdFlightMode();
    int                 maxWaitMSecsForKGroup();

    QList<double>&          rgCalcedBearings() { return _rgCalcedBearings; }
    CSVLogManager&          csvLogManager() { return _csvLogManager; }
    void                    rotationIsStarting();
    void                    rotationIsEnding();
    QmlObjectListModel*     rotationInfoList() { return &_rotationInfoList; }

    void signalCalcedBearingsChanged() { emit calcedBearingsChanged(); }

    TagDatabase* tagDatabase();

    Q_INVOKABLE void autoDetection      ();
    Q_INVOKABLE void startRotation      (void);
    Q_INVOKABLE void startDetection     (void);
    Q_INVOKABLE void stopDetection      (void);
    Q_INVOKABLE void rawCapture         (void);
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

    static double normalizeHeading(double heading);

signals:
    void calcedBearingsChanged          (void);
    void flightMachineActiveChanged     (bool flightMachineActive);
    void pulseInfoListsChanged          (void);
    void controllerLostHeartbeatChanged ();
    void controllerStatusChanged        ();
    void controllerCPUTempChanged       ();
    void maxSNRChanged                  (double maxSNR);
    void minSNRChanged                  (double minSNR);
    void _detectionStarted              (void);
    void _startDetectionFailed          (void);
    void _detectionStopped              (void);
    void _stopDetectionFailed           (void);
    void activeRotationChanged          (bool activeRotation);
    void detectorHeartbeatReceived      (int oneBasedRateIndex);

private slots:
    void _controllerHeartbeatFailed(void);
    void _stopDetectionOnDisarmed(bool armed);
    bool _validateAtLeastOneTagSelected();

private:
    typedef enum {
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

    void    _handleTunnelPulse          (Vehicle* vehicle, const mavlink_tunnel_t& tunnel);
    void    _handleTunnelHeartbeat      (const mavlink_tunnel_t& tunnel);
    void    _say                        (QString text);
    int     _rawPulseToPct              (double rawPulse);
    double  _pulseTimeSeconds           (void) { return _lastPulseInfo.start_time_seconds; }
    double  _pulseSNR                   (void) { return _lastPulseInfo.snr; }
    bool    _pulseConfirmed             (void) { return _lastPulseInfo.confirmed_status; }
    bool    _useSNRForPulseStrength     (void) { return _customSettings->useSNRForPulseStrength()->rawValue().toBool(); }
    void    _captureScreen              (void);
    void    _setActiveRotation          (bool active);

    QVariantList            _settingsPages;
    QVariantList            _instrumentPages;
    int                     _vehicleStateIndex;
    QList<VehicleState_t>   _vehicleStates;
    bool                    _activeRotation     = false;
    QList<double>           _rgCalcedBearings;
    bool                    _flightStateMachineActive;
    int                     _currentSlice;
    int                     _cSlice;
    bool                    _retryRotation      = false;
    int                     _controllerStatus   = ControllerStatusIdle;
    float                   _controllerCPUTemp  = 0.0;

    QmlObjectListModel      _rotationInfoList;

    CustomOptions*          _customOptions;
    CustomSettings*         _customSettings;
    int                     _vehicleFrequency;
    int                     _lastPulseSendIndex;
    int                     _missedPulseCount;
    QmlObjectListModel      _customMapItems;
    TunnelProtocol::PulseInfo_t _lastPulseInfo;
    QVariantList            _toolbarIndicators;

    bool                    _controllerLostHeartbeat { false };
    QTimer                  _controllerHeartbeatTimer;

    int                     _curLogFileDownloadIndex;
    QString                 _logDirPathOnVehicle;
    QStringList             _logFileDownloadList;

    CSVLogManager           _csvLogManager;

    double                  _maxSNR = qQNaN();
    double                  _minSNR = qQNaN();
};
