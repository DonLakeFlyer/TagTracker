#include "CSVLogManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGeoCoordinate>

#include "AppSettings.h"
#include "CustomLoggingCategory.h"
#include "CustomPlugin.h"
#include "MultiVehicleManager.h"
#include "PulseLogSidecar.h"
#include "QGCApplication.h"
#include "SettingsManager.h"
#include "TunnelProtocol.h"
#include "Vehicle.h"

using namespace TunnelProtocol;

CSVLogManager::CSVLogManager(QObject* parent)
    : QObject(parent)
{
}

void CSVLogManager::init()
{
    // Queued so QGroundControl has saved the telemetry file before the folder is closed.
    // MAVLinkProtocol saves it from its own vehicleRemoved slot, which is connected after ours.
    connect(MultiVehicleManager::instance(), &MultiVehicleManager::vehicleRemoved, this,
            &CSVLogManager::_vehicleRemoved, Qt::QueuedConnection);
}

QString CSVLogManager::logSavePath(void)
{
    if (!_logSavePathOverride.isEmpty()) {
        return _logSavePathOverride;
    }
    return SettingsManager::instance()->appSettings()->logSavePath();
}

void CSVLogManager::_vehicleRemoved()
{
    // QGroundControl ends the telemetry recording only when the last vehicle goes
    if (MultiVehicleManager::instance()->vehicles()->count() == 0) {
        connectionEnded();
    }
}

void CSVLogManager::connectionEnded()
{
    if (_connectionFolder.isEmpty()) {
        return;
    }
    const QString folder = _connectionFolder;
    // Cleared first, because closing the pulse log below can call back into here
    _connectionFolder.clear();
    // Detection can still be running when the vehicle goes. Its logs belong to this folder and
    // must be closed, with their stop times recorded, before the folder is paired.
    csvStopFullPulseLog();
    csvStopRotationPulseLog();
    qCDebug(CustomPluginLog) << "Log folder closed:" << folder;
    emit connectionFolderClosed(folder);
}

bool CSVLogManager::_openConnectionFolder(const QDateTime& now)
{
    if (!_connectionFolder.isEmpty()) {
        return true;
    }

    const QString folder = QString("%1/%2").arg(logSavePath(), now.toString("yyyy-MM-dd-hh-mm-ss-zzz"));
    if (!QDir().mkpath(folder)) {
        qgcApp()->showAppMessage(QString("Unable to create log folder %1").arg(folder));
        return false;
    }

    _connectionFolder = folder;
    _connectionFolderHasVehicle = MultiVehicleManager::instance()->activeVehicle() != nullptr;
    // Rotation logs are numbered per folder, so numbering never collides with an earlier connection's
    _csvRotationCount = 1;
    qCDebug(CustomPluginLog) << "Log folder opened:" << folder << "vehicle:" << _connectionFolderHasVehicle;
    return true;
}

void CSVLogManager::csvStartFullPulseLog(void)
{
    if (_csvFullPulseLogFile.isOpen()) {
        qgcApp()->showAppMessage("Unable to open full pulse csv log file - csvFile already open");
        return;
    }

    const QDateTime now = QDateTime::currentDateTime();
    if (!_openConnectionFolder(now)) {
        return;
    }

    _csvFullPulseLogFile.setFileName(
        QString("%1/Pulse-%2.csv")
            .arg(_connectionFolder, now.toString("yyyy-MM-dd-hh-mm-ss-zzz").toLocal8Bit().data()));
    qCDebug(CustomPluginLog) << "Full CSV Pulse logging to:" << _csvFullPulseLogFile.fileName();
    if (!_csvFullPulseLogFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Unbuffered)) {
        qgcApp()->showAppMessage(QString("Open of full pulse csv log file failed: %1").arg(_csvFullPulseLogFile.errorString()));
        return;
    }
    _csvWritePulseHeader(_csvFullPulseLogFile);

    Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();
    _sidecarPath = PulseLogSidecar::pathForPulseLog(_csvFullPulseLogFile.fileName());
    PulseLogSidecar::write(
        _sidecarPath, PulseLogSidecar::create(QFileInfo(_csvFullPulseLogFile.fileName()).fileName(), now,
                                              vehicle ? vehicle->id() : -1, QCoreApplication::applicationVersion()));
}

void CSVLogManager::csvStopFullPulseLog(void)
{
    if (_csvFullPulseLogFile.isOpen()) {
        // Closing mid-rotation must still leave a paired STOP_ROTATION row
        csvLogRotationStop();
        _csvFullPulseLogFile.close();

        PulseLogSidecar::setValue(_sidecarPath, PulseLogSidecar::keyStopUtc,
                                  PulseLogSidecar::utcString(QDateTime::currentDateTime()));
        _sidecarPath.clear();

        // No vehicle means no telemetry recording and no disconnect to close the folder, so each such run gets its own
        if (!_connectionFolderHasVehicle) {
            connectionEnded();
        }
    }
}

void CSVLogManager::csvStartRotationPulseLog()
{
    if (_csvRotationPulseLogFile.isOpen()) {
        qgcApp()->showAppMessage("Unable to open rotation pulse csv log file - csvFile already open");
        return;
    }

    if (!_openConnectionFolder(QDateTime::currentDateTime())) {
        return;
    }

    _csvRotationPulseLogFile.setFileName(QString("%1/Rotation-%2.csv").arg(_connectionFolder).arg(_csvRotationCount++));
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
    csvFile.write(QString("# %1, collection_id, slice_id, tag_id, frequency_hz, cycle_counter, start_time_seconds, predict_next_start_seconds, snr, score_ratio, signal_psd, noise_psd, detection_status, confirmed_status, rate_state, candidate_id, latitude, longitude, altitude_rel, roll_deg, pitch_deg, yaw_deg, antenna_offset\n")
        .arg(COMMAND_ID_PYTHON_PULSE)
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

void CSVLogManager::_csvLogPythonPulse(QFile& csvFile, const TunnelProtocol::PythonPulseInfo_t& pulseInfo)
{
    if (csvFile.isOpen()) {
        auto customSettings = qobject_cast<CustomPlugin*>(CustomPlugin::instance())->customSettings();
        csvFile.write(QString("%1, %2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13, %14, %15, %16, %17, %18, %19, %20, %21, %22, %23\n")
            .arg(COMMAND_ID_PYTHON_PULSE)
            .arg(pulseInfo.collection_id)
            .arg(pulseInfo.slice_id)
            .arg(pulseInfo.tag_id)
            .arg(pulseInfo.frequency_hz)
            .arg(pulseInfo.cycle_counter)
            .arg(pulseInfo.start_time_seconds,          0, 'f', 6)
            .arg(pulseInfo.predict_next_start_seconds,  0, 'f', 6)
            .arg(pulseInfo.snr,                         0, 'f', 6)
            .arg(pulseInfo.score_ratio,                 0, 'f', 6)
            .arg(pulseInfo.signal_psd,                  0, 'g', 7)
            .arg(pulseInfo.noise_psd,                   0, 'g', 7)
            .arg(pulseInfo.detection_status)
            .arg(pulseInfo.confirmed_status)
            .arg(pulseInfo.rate_state)
            .arg(pulseInfo.candidate_id)
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

void CSVLogManager::csvLogPythonPulse(const PythonPulseInfo_t& pulseInfo)
{
    if (_csvFullPulseLogFile.isOpen()) {
        _csvLogPythonPulse(_csvFullPulseLogFile, pulseInfo);
    }
    if (_csvRotationPulseLogFile.isOpen()) {
        _csvLogPythonPulse(_csvRotationPulseLogFile, pulseInfo);
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
