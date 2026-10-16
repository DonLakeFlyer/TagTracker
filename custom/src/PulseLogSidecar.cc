#include "PulseLogSidecar.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QSaveFile>

#include "CustomLoggingCategory.h"

namespace PulseLogSidecar {

QString pathForPulseLog(const QString& pulseLogPath)
{
    const QFileInfo info(pulseLogPath);
    return info.dir().filePath(info.completeBaseName() + QStringLiteral(".json"));
}

QString utcString(const QDateTime& dateTime)
{
    return dateTime.toUTC().toString(Qt::ISODateWithMs);
}

QDateTime parseUtc(const QJsonValue& value)
{
    if (!value.isString()) {
        return QDateTime();
    }
    return QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
}

QJsonObject create(const QString& pulseLogFileName, const QDateTime& start, int vehicleId, const QString& appVersion)
{
    QJsonObject sidecar;
    sidecar[keyVersion] = currentVersion;
    sidecar[keyPulseLog] = pulseLogFileName;
    sidecar[keyStartUtc] = utcString(start);
    sidecar[keyStopUtc] = QJsonValue::Null;
    sidecar[keyVehicleId] = vehicleId < 0 ? QJsonValue(QJsonValue::Null) : QJsonValue(vehicleId);
    sidecar[keyAppVersion] = appVersion;
    sidecar[keyTelemetryFile] = QJsonValue::Null;
    return sidecar;
}

bool write(const QString& sidecarPath, const QJsonObject& sidecar)
{
    // QSaveFile so a crash mid-write never leaves a truncated sidecar behind. Storage that cannot
    // rename, possibly Android removable media, gets a direct write instead, as QGroundControl's own
    // telemetry save does.
    QSaveFile file(sidecarPath);
    file.setDirectWriteFallback(true);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(CustomPluginLog) << "Unable to open pulse log sidecar" << sidecarPath << file.errorString();
        return false;
    }
    file.write(QJsonDocument(sidecar).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        qCWarning(CustomPluginLog) << "Unable to write pulse log sidecar" << sidecarPath << file.errorString();
        return false;
    }
    return true;
}

bool read(const QString& sidecarPath, QJsonObject& sidecar)
{
    QFile file(sidecarPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(CustomPluginLog) << "Unreadable pulse log sidecar" << sidecarPath << parseError.errorString();
        return false;
    }
    sidecar = doc.object();
    return sidecar.contains(keyVersion) && sidecar.contains(keyStartUtc);
}

bool setValue(const QString& sidecarPath, const QString& key, const QJsonValue& value)
{
    QJsonObject sidecar;
    if (!read(sidecarPath, sidecar)) {
        return false;
    }
    sidecar[key] = value;
    return write(sidecarPath, sidecar);
}

}  // namespace PulseLogSidecar
