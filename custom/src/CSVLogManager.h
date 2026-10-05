#pragma once

#include "TunnelProtocol.h"

#include <QObject>
#include <QFile>
#include <QDateTime>

class CSVLogManager : public QObject
{
    Q_OBJECT

public:
    CSVLogManager(QObject* parent = nullptr);

    void    init                    ();
    QString logSavePath             ();
    /// Tests point the logs somewhere disposable instead of the app's save path
    void    setLogSavePathOverride  (const QString& path) { _logSavePathOverride = path; }
    /// The folder this connection's pulse and rotation logs go into, empty until the first log opens
    QString connectionFolder        () const { return _connectionFolder; }
    /// The vehicle connection is over: the next log starts a new folder
    void    connectionEnded         ();
    void    csvStartFullPulseLog    ();
    bool    fullPulseLogOpen        () const { return _csvFullPulseLogFile.isOpen(); }
    void    csvStopFullPulseLog     ();
    void    csvStartRotationPulseLog();
    void    csvStopRotationPulseLog ();
    void    csvLogPulse             (const TunnelProtocol::PulseInfo_t& pulseInfo);
    void    csvLogPythonPulse       (const TunnelProtocol::PythonPulseInfo_t& pulseInfo);
    void    csvLogRotationStart     () { _csvLogRotationStartStop(true); }
    void    csvLogRotationStop      () { _csvLogRotationStartStop(false); }

signals:
    /// Emitted when a connection's folder will receive no more logs
    void connectionFolderClosed(const QString& folderPath);

private slots:
    void _vehicleRemoved();

private:
    bool _openConnectionFolder(const QDateTime& now);
    void _csvLogRotationStartStop(bool startRotation);
    void _csvWritePulseHeader(QFile& csvFile);
    void _csvLogPulse(QFile& csvFile, const TunnelProtocol::PulseInfo_t& pulseInfo);
    void _csvLogPythonPulse(QFile& csvFile, const TunnelProtocol::PythonPulseInfo_t& pulseInfo);

    QString _logSavePathOverride;
    QString _connectionFolder;
    bool    _connectionFolderHasVehicle = false;
    QString _sidecarPath;
    QFile   _csvFullPulseLogFile;
    QFile   _csvRotationPulseLogFile;
    int     _csvRotationCount = 1;
    bool    _rotationStartLogged = false;
};
