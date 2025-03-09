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
        qgcApp()->showAppMessage("Unabled to open full pulse csv log file - csvFile already open");
        return;
    }

    _csvFullPulseLogFile.setFileName(QString("%1/Pulse-%2.csv").arg(logSavePath(), QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss-zzz").toLocal8Bit().data()));
    qCDebug(CustomPluginLog) << "Full CSV Pulse logging to:" << _csvFullPulseLogFile.fileName();
    if (!_csvFullPulseLogFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Unbuffered)) {
        qgcApp()->showAppMessage(QString("Open of full pulse csv log file failed: %1").arg(_csvFullPulseLogFile.errorString()));
        return;
    }
}

void CSVLogManager::csvStopFullPulseLog(void)
{
    if (_csvFullPulseLogFile.isOpen()) {
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
        qgcApp()->showAppMessage("Unabled to open rotation pulse csv log file - csvFile already open");
        return;
    }

    _csvRotationPulseLogFile.setFileName(QString("%1/Rotation-%2.csv").arg(logSavePath()).arg(_csvRotationCount++));
    qCDebug(CustomPluginLog) << "Rotation CSV Pulse logging to:" << _csvRotationPulseLogFile.fileName();
    if (!_csvRotationPulseLogFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Unbuffered)) {
        qgcApp()->showAppMessage(QString("Open of rotation pulse csv log file failed: %1").arg(_csvRotationPulseLogFile.errorString()));
        return;
    }
}

void CSVLogManager::csvStopRotationPulseLog(bool calcBearing)
{
    if (_csvRotationPulseLogFile.isOpen()) {
        csvLogRotationStop();
        _csvRotationPulseLogFile.close();

#if 0        
        if (calcBearing) {
            coder::array<char, 2U> rotationFileNameAsArray;
            std::string rotationFileName = _csvRotationPulseLogFile.fileName().toStdString();
            rotationFileNameAsArray.set_size(1, rotationFileName.length());
            int index = 0;
            for (auto chr : rotationFileName) {
                rotationFileNameAsArray[index++] = chr;
            }

            double calcedBearing = bearing(rotationFileNameAsArray);
            if (calcedBearing < 0) {
                calcedBearing += 360;
            }
            _rgCalcedBearings.last() = calcedBearing;
            qCDebug(CustomPluginLog) << "Calculated bearing:" << _rgCalcedBearings.last();
            FIX ME emit calcedBearingsChanged();
        }
#endif        
    }
}

void CSVLogManager::_csvLogPulse(QFile& csvFile, const TunnelProtocol::PulseInfo_t& pulseInfo)
{
    if (csvFile.isOpen()) {
        if (csvFile.size() == 0) {
            csvFile.write(QString("# %1, tag_id, frequency_hz, start_time_seconds, predict_next_start_seconds, snr, stft_score, group_seq_counter, group_ind, group_snr, noise_psd, detection_status, confirmed_status, position_x, _y, _z, orientation_x, _y, _z, _w, antenna_offset\n")
                .arg(COMMAND_ID_PULSE)
                .toUtf8());
        }
        auto customSettings = qobject_cast<CustomPlugin*>(CustomPlugin::instance())->customSettings();
        csvFile.write(QString("%1, %2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13, %14, %15, %16, %17, %18, %19, %20, %21\n")
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
            .arg(pulseInfo.position_x,                  0, 'f', 6)
            .arg(pulseInfo.position_y,                  0, 'f', 6)
            .arg(pulseInfo.position_z,                  0, 'f', 6)
            .arg(pulseInfo.orientation_x,               0, 'f', 6)
            .arg(pulseInfo.orientation_y,               0, 'f', 6)
            .arg(pulseInfo.orientation_z,               0, 'f', 6)
            .arg(pulseInfo.orientation_w,               0, 'f', 6)
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
    Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();
    if (!vehicle) {
        qCWarning(CustomPluginLog) << "INTERNAL ERROR: _csvLogRotationStartStop - no vehicle available";
        return;
    }

    if (_csvFullPulseLogFile.isOpen()) {
        QGeoCoordinate coord = vehicle->coordinate();
        _csvFullPulseLogFile.write(QString("%1,%2,%3,%4\n").arg(startRotation ? COMMAND_ID_START_ROTATION : COMMAND_ID_STOP_ROTATION)
                                .arg(coord.latitude(), 0, 'f', 6)
                                .arg(coord.longitude(), 0, 'f', 6)
                                .arg(vehicle->altitudeAMSL()->rawValue().toDouble(), 0, 'f', 6)
                                .toUtf8());
    }
}

