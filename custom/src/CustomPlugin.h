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
    void controllerLostHeartbeatChanged ();
    void controllerStatusChanged        ();
    void controllerCPUTempChanged       ();
    void maxSNRChanged                  (double maxSNR);
    void minSNRChanged                  (double minSNR);
    void activeRotationChanged          (bool activeRotation);
    void detectorHeartbeatReceived      (int oneBasedRateIndex);
    void pythonDetectorResultReceived   (uint32_t tagId);  // confirmed pulse or no-pulse in Python mode

private slots:
    void _controllerHeartbeatFailed(void);
    void _stopDetectionOnDisarmed(bool armed);
    bool _validateAtLeastOneTagSelected();

private:
    void    _handleTunnelPulse          (Vehicle* vehicle, const mavlink_tunnel_t& tunnel);
    void    _handleTunnelHeartbeat      (const mavlink_tunnel_t& tunnel);
    void    _say                        (QString text);
    int     _rawPulseToPct              (double rawPulse);
    bool    _useSNRForPulseStrength     (void) { return _customSettings->useSNRForPulseStrength()->rawValue().toBool(); }
    void    _captureScreen              (void);
    void    _setActiveRotation          (bool active);
    void    _sendStopDetectionDirect    (void);

    bool                    _activeRotation     = false;
    QList<double>           _rgCalcedBearings;
    int                     _controllerStatus   = ControllerStatusIdle;
    float                   _controllerCPUTemp  = 0.0;

    QmlObjectListModel      _rotationInfoList;

    CustomOptions*          _customOptions;
    CustomSettings*         _customSettings;
    int                     _vehicleFrequency;
    int                     _lastPulseSendIndex;
    int                     _missedPulseCount;
    QmlObjectListModel      _customMapItems;
    QVariantList            _toolbarIndicators;

    bool                    _controllerLostHeartbeat = true;
    QTimer                  _controllerHeartbeatTimer;

    CSVLogManager           _csvLogManager;

    double                  _maxSNR = qQNaN();
    double                  _minSNR = qQNaN();
};
