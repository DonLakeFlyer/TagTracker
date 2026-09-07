#pragma once

#include "TunnelProtocol.h"

#include <QObject>
#include <QFile>

class CSVLogManager : public QObject
{
    Q_OBJECT

public:
    CSVLogManager(QObject* parent = nullptr);

    QString logSavePath             ();
    void    csvStartFullPulseLog    ();
    void    csvStopFullPulseLog     ();
    void    csvClearPrevRotationLogs();
    void    csvStartRotationPulseLog();
    void    csvStopRotationPulseLog ();
    void    csvLogPulse             (const TunnelProtocol::PulseInfo_t& pulseInfo);
    void    csvLogPythonPulse       (const TunnelProtocol::PythonPulseInfo_t& pulseInfo);
    void    csvLogRotationStart     () { _csvLogRotationStartStop(true); }
    void    csvLogRotationStop      () { _csvLogRotationStartStop(false); }

private:
    void _csvLogRotationStartStop(bool startRotation);
    void _csvWritePulseHeader(QFile& csvFile);
    void _csvLogPulse(QFile& csvFile, const TunnelProtocol::PulseInfo_t& pulseInfo);
    void _csvLogPythonPulse(QFile& csvFile, const TunnelProtocol::PythonPulseInfo_t& pulseInfo);

    QFile   _csvFullPulseLogFile;
    QFile   _csvRotationPulseLogFile;
    int     _csvRotationCount = 1;
    bool    _rotationStartLogged = false;
};
