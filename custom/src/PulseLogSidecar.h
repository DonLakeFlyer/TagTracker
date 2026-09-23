#pragma once

#include <QtCore/QDateTime>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QString>

/// The JSON file written beside each Pulse-*.csv, same name with a .json extension.
/// It records one pulse detection run's window, the vehicle, the app version, and the
/// telemetry file that covers the run once one is known. The pulse CSV format itself is
/// left alone because existing readers parse it positionally.
namespace PulseLogSidecar {
constexpr int currentVersion = 1;

constexpr const char* keyVersion = "sidecar_version";
constexpr const char* keyPulseLog = "pulse_log";
constexpr const char* keyStartUtc = "start_utc";
constexpr const char* keyStopUtc = "stop_utc";
constexpr const char* keyVehicleId = "vehicle_id";
constexpr const char* keyAppVersion = "app_version";
constexpr const char* keyTelemetryFile = "telemetry_file";

/// Pulse-X.csv -> Pulse-X.json
QString pathForPulseLog(const QString& pulseLogPath);

/// ISO 8601 UTC with milliseconds, e.g. 2026-09-23T17:11:12.123Z
QString utcString(const QDateTime& dateTime);
QDateTime parseUtc(const QJsonValue& value);

/// A new sidecar for a run that has just started. vehicleId < 0 means no vehicle.
QJsonObject create(const QString& pulseLogFileName, const QDateTime& start, int vehicleId, const QString& appVersion);

bool write(const QString& sidecarPath, const QJsonObject& sidecar);
bool read(const QString& sidecarPath, QJsonObject& sidecar);
bool setValue(const QString& sidecarPath, const QString& key, const QJsonValue& value);
}  // namespace PulseLogSidecar
