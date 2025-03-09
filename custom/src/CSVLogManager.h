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
    void    csvStopRotationPulseLog (bool calcBearing);
    void    csvLogPulse             (const TunnelProtocol::PulseInfo_t& pulseInfo);
    void    csvLogRotationStart     () { _csvLogRotationStartStop(true); }
    void    csvLogRotationStop      () { _csvLogRotationStartStop(false); }

private:
    void _csvLogRotationStartStop(bool startRotation);
    void _csvLogPulse(QFile& csvFile, const TunnelProtocol::PulseInfo_t& pulseInfo);

    QFile   _csvFullPulseLogFile;
    QFile   _csvRotationPulseLogFile;
    int     _csvRotationCount = 1;
};