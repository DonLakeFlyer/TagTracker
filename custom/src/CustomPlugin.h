#pragma once

#include "QGCCorePlugin.h"
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
#include <QtQml/QQmlAbstractUrlInterceptor>
#include <cmath>

class CustomState;
class CustomStateMachine;
class QState;
class QQmlApplicationEngine;

using namespace TunnelProtocol;

class CustomPlugin : public QGCCorePlugin
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

    Q_PROPERTY(bool                 controllerLostHeartbeat MEMBER  _controllerLostHeartbeat    NOTIFY controllerLostHeartbeatChanged)
    Q_PROPERTY(int                  controllerStatus        MEMBER  _controllerStatus           NOTIFY controllerStatusChanged)
    Q_PROPERTY(float                controllerCPUTemp       MEMBER  _controllerCPUTemp          NOTIFY controllerCPUTempChanged)
    Q_PROPERTY(uint                 controllerProtocolVersion READ controllerProtocolVersion    NOTIFY protocolCompatibilityChanged)
    Q_PROPERTY(bool                 protocolCompatible      READ protocolCompatible             NOTIFY protocolCompatibilityChanged)
    Q_PROPERTY(QmlObjectListModel*  detectorList            READ    detectorList                CONSTANT)
    Q_PROPERTY(TagDatabase*         tagDatabase             READ    tagDatabase                 CONSTANT)
    Q_PROPERTY(double               maxSNR                  MEMBER  _maxSNR                     NOTIFY maxSNRChanged)
    Q_PROPERTY(double               minSNR                  MEMBER  _minSNR                     NOTIFY minSNRChanged)
    Q_PROPERTY(bool                 activeRotation          MEMBER  _activeRotation             NOTIFY activeRotationChanged)
    Q_PROPERTY(bool                 rotationInProgress      READ    rotationInProgress          NOTIFY rotationInProgressChanged)
    Q_PROPERTY(QmlObjectListModel*  rotationInfoList        READ    rotationInfoList          CONSTANT)

    CustomSettings*     customSettings  () { return _customSettings; }
    bool                isPythonMode    () { return _customSettings->isPythonMode(); }
    DetectorList *      detectorList() { return DetectorList::instance(); }
    QString             holdFlightMode();
    int                 maxWaitMSecsForKGroup();
    uint32_t            controllerProtocolVersion() const { return _controllerProtocolVersion; }
    bool                protocolCompatible() const;
    bool                hasPriorBearing() const { return std::isfinite(_priorBearingDeg); }
    double              priorBearingDeg() const { return _priorBearingDeg; }
    // True from the moment a rotation state machine starts until it finishes or is stopped
    bool                rotationInProgress() const { return _rotationInProgress; }
    const CollectionStatus_t& lastCollectionStatus() const { return _lastCollectionStatus; }

    CSVLogManager&          csvLogManager() { return _csvLogManager; }
    void                    rotationIsStarting(uint32_t collectionId = 0);
    void                    rotationIsEnding();
    QmlObjectListModel*     rotationInfoList() { return &_rotationInfoList; }

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
    void                registerCustomSettings  (SettingsManager* settingsManager) final;
    bool                mavlinkMessage          (Vehicle *vehicle, LinkInterface *link, const mavlink_message_t &message) final;
    QGCOptions*         options                 (void) final { return qobject_cast<QGCOptions*>(_customOptions); }
    void                adjustSettingMetaData   (const QString& settingsGroup, FactMetaData& metaData, bool& userVisible) final;
    const QmlObjectListModel* customMapItems    (void) final;
    const QVariantList& toolBarIndicators       (void) final;

    QQmlApplicationEngine* createQmlApplicationEngine  (QObject* parent) final;
    void                   destroyQmlApplicationEngine (QQmlApplicationEngine* qmlEngine) final;

    static double normalizeHeading(double heading);
    static const char* controllerStatusString(int status);

signals:
    void controllerLostHeartbeatChanged ();
    void controllerStatusChanged        ();
    void controllerCPUTempChanged       ();
    void protocolCompatibilityChanged   ();
    void maxSNRChanged                  (double maxSNR);
    void minSNRChanged                  (double minSNR);
    void activeRotationChanged          (bool activeRotation);
    void rotationInProgressChanged      (bool rotationInProgress);
    void pythonDetectorResultReceived   (uint32_t tagId);  // confirmed, low-confidence, or no-pulse
    void collectionStatusReceived       (uint32_t collectionId, uint32_t sliceId, uint32_t status, uint32_t errorCode);

private slots:
    void _controllerHeartbeatFailed(void);
    void _stopDetectionOnDisarmed(bool armed);
    bool _validateAtLeastOneTagSelected();
    bool _validatePythonCollectionAllowed();
    bool _validateVehicleAvailable();

private:
    void    _handleUavrtPulse           (Vehicle* vehicle, const mavlink_tunnel_t& tunnel);
    void    _handlePythonPulse          (const mavlink_tunnel_t& tunnel);
    void    _handleTunnelHeartbeat      (const mavlink_tunnel_t& tunnel);
    void    _handleBearingResult        (const mavlink_tunnel_t& tunnel);
    void    _handleCollectionStatus     (const mavlink_tunnel_t& tunnel);
    void    _updateSNRRange             (double snr);
    void    _say                        (QString text);
    bool    _useSNRForPulseStrength     (void) { return _customSettings->useSNRForPulseStrength()->rawValue().toBool(); }
    void    _captureScreen              (void);
    void    _setActiveRotation          (bool active);
    void    _startRotationMachine       (CustomStateMachine* stateMachine);
    void    _setRotationInProgress      (bool inProgress);
    void    _sendStopDetectionDirect    (void);
    void    _sendCollectionCancel       (void);

    bool                    _activeRotation     = false;
    bool                    _rotationInProgress = false;
    uint32_t                _activeCollectionId = 0;
    CollectionStatus_t      _lastCollectionStatus {};
    int                     _controllerStatus   = ControllerStatusIdle;
    float                   _controllerCPUTemp  = 0.0;
    uint32_t                _controllerProtocolVersion = 0;
    bool                    _protocolMismatchReported = false;
    bool                    _uavrtWrongModeReported = false;
    bool                    _pythonWrongModeReported = false;
    bool                    _detectionStartRequested = false;   // survey start requested, heartbeat not yet Detecting
    bool                    _stopDetectionPending = false;      // disarmed during that window; stop once controller reports Detecting

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
    double                  _priorBearingDeg = qQNaN();

    QQmlApplicationEngine*  _qmlEngine = nullptr;
    class CustomOverrideInterceptor* _urlInterceptor = nullptr;
};

/// Redirects qrc:/qml/<path> to :/Custom/qml/<path> when the custom build ships an override.
class CustomOverrideInterceptor : public QQmlAbstractUrlInterceptor
{
public:
    QUrl intercept(const QUrl& url, QQmlAbstractUrlInterceptor::DataType type) final;
};
