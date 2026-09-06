#include "CSVLogManager.h"
#include "CustomLoggingCategory.h"
#include "TunnelProtocol.h"
#include "CustomPlugin.h"

#include "SettingsManager.h"
#include "AppSettings.h"
#include "QGCApplication.h"
#include "MultiVehicleManager.h"
#include "Vehicle.h"

#include <QGeoCoordinate>

using namespace TunnelProtocol;

CSVLogManager::CSVLogManager(QObject* parent)
    : QObject(parent)
{
}

QString CSVLogManager::logSavePath(void)
{
    return SettingsManager::instance()->appSettings()->logSavePath();
}

void CSVLogManager::csvStartFullPulseLog(void)
{
    if (_csvFullPulseLogFile.isOpen()) {
        qgcApp()->showAppMessage("Unable to open full pulse csv log file - csvFile already open");
        return;
    }

    _csvFullPulseLogFile.setFileName(QString("%1/Pulse-%2.csv").arg(logSavePath(), QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss-zzz").toLocal8Bit().data()));
    qCDebug(CustomPluginLog) << "Full CSV Pulse logging to:" << _csvFullPulseLogFile.fileName();
    if (!_csvFullPulseLogFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Unbuffered)) {
        qgcApp()->showAppMessage(QString("Open of full pulse csv log file failed: %1").arg(_csvFullPulseLogFile.errorString()));
        return;
    }
    _csvWritePulseHeader(_csvFullPulseLogFile);
}

void CSVLogManager::csvStopFullPulseLog(void)
{
    if (_csvFullPulseLogFile.isOpen()) {
        // Closing mid-rotation must still leave a paired STOP_ROTATION row
        csvLogRotationStop();
        _csvFullPulseLogFile.close();
    }
}

void CSVLogManager::csvClearPrevRotationLogs(void)
{
    QDir csvLogDir(logSavePath(), {"Rotation-*.csv"});
    for (const QString & filename: csvLogDir.entryList()){
        csvLogDir.remove(filename);
    }
}

void CSVLogManager::csvStartRotationPulseLog()
{
    if (_csvRotationPulseLogFile.isOpen()) {
        qgcApp()->showAppMessage("Unable to open rotation pulse csv log file - csvFile already open");
        return;
    }

    _csvRotationPulseLogFile.setFileName(QString("%1/Rotation-%2.csv").arg(logSavePath()).arg(_csvRotationCount++));
    qCDebug(CustomPluginLog) << "Rotation CSV Pulse logging to:" << _csvRotationPulseLogFile.fileName();
    if (!_csvRotationPulseLogFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Unbuffered)) {
        qgcApp()->showAppMessage(QString("Open of rotation pulse csv log file failed: %1").arg(_csvRotationPulseLogFile.errorString()));
        return;
    }
    _csvWritePulseHeader(_csvRotationPulseLogFile);
}

void CSVLogManager::csvStopRotationPulseLog()
{
    if (_csvRotationPulseLogFile.isOpen()) {
        _csvRotationPulseLogFile.close();
    }
}

void CSVLogManager::_csvWritePulseHeader(QFile& csvFile)
{
    csvFile.write(QString("# %1, tag_id, frequency_hz, start_time_seconds, predict_next_start_seconds, snr, stft_score, group_seq_counter, group_ind, group_snr, noise_psd, detection_status, confirmed_status, latitude, longitude, altitude_rel, roll_deg, pitch_deg, yaw_deg, antenna_offset\n")
        .arg(COMMAND_ID_PULSE)
        .toUtf8());
}

void CSVLogManager::_csvLogPulse(QFile& csvFile, const TunnelProtocol::PulseInfo_t& pulseInfo)
{
    if (csvFile.isOpen()) {
        auto customSettings = qobject_cast<CustomPlugin*>(CustomPlugin::instance())->customSettings();
        csvFile.write(QString("%1, %2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13, %14, %15, %16, %17, %18, %19, %20\n")
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
            .arg(pulseInfo.latitude,                    0, 'f', 6)
            .arg(pulseInfo.longitude,                   0, 'f', 6)
            .arg(pulseInfo.altitude_rel,                0, 'f', 6)
            .arg(pulseInfo.roll_deg,                    0, 'f', 6)
            .arg(pulseInfo.pitch_deg,                   0, 'f', 6)
            .arg(pulseInfo.yaw_deg,                     0, 'f', 6)
            .arg(customSettings->antennaOffset()->rawValue().toDouble(), 0, 'f', 6)
            .toUtf8());
    }
}

void CSVLogManager::csvLogPulse(const PulseInfo_t& pulseInfo)
{
    if (_csvFullPulseLogFile.isOpen()) {
        _csvLogPulse(_csvFullPulseLogFile, pulseInfo);
    }
    if (_csvRotationPulseLogFile.isOpen()) {
        _csvLogPulse(_csvRotationPulseLogFile, pulseInfo);
    }
}

void CSVLogManager::_csvLogRotationStartStop(bool startRotation)
{
    if (!_csvFullPulseLogFile.isOpen()) {
        return;
    }
    // One START and one STOP marker per rotation
    if (startRotation == _rotationStartLogged) {
        return;
    }

    double latitude = qQNaN();
    double longitude = qQNaN();
    double altitudeAMSL = qQNaN();
    Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();
    if (vehicle) {
        QGeoCoordinate coord = vehicle->coordinate();
        latitude = coord.latitude();
        longitude = coord.longitude();
        altitudeAMSL = vehicle->altitudeAMSL()->rawValue().toDouble();
    } else {
        qCWarning(CustomPluginLog) << "_csvLogRotationStartStop - no vehicle available, logging without position";
    }

    _csvFullPulseLogFile.write(QString("%1,%2,%3,%4\n").arg(startRotation ? COMMAND_ID_START_ROTATION : COMMAND_ID_STOP_ROTATION)
                            .arg(latitude, 0, 'f', 6)
                            .arg(longitude, 0, 'f', 6)
                            .arg(altitudeAMSL, 0, 'f', 6)
                            .toUtf8());
    _rotationStartLogged = startRotation;
}

