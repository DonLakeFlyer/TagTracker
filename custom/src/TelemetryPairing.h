#pragma once

#include <QtCore/QString>
#include <QtCore/QtTypes>

/// Finds the telemetry file QGroundControl saved for a connection's log folder, copies it into
/// the folder, and names it in each pulse log sidecar there.
///
/// Matching is by content, never by filename: the filename is the time the recording stopped,
/// while the file's first record timestamp is the time it started. A telemetry file belongs to
/// a folder when its span contains the start of every pulse detection run in the folder. Stop
/// times are not required to fall inside, because a run left going at disconnect stops later.
/// The original telemetry file stays where QGroundControl put it.
namespace TelemetryPairing {

struct Span
{
    quint64 startUsecs = 0;
    quint64 endUsecs = 0;

    bool isValid() const { return startUsecs != 0 && endUsecs >= startUsecs; }
};

enum class Result
{
    Paired,         ///< A telemetry file was copied in and the sidecars updated
    AlreadyPaired,  ///< Every sidecar already names its telemetry file
    NoTelemetry,    ///< No telemetry file covers the runs, e.g. the vehicle never armed
    Ambiguous,      ///< More than one different telemetry file covers the runs; none is named
    NoSidecars,     ///< Not a pulse log folder
    Error,
};

struct Summary
{
    int paired = 0;
    int noTelemetry = 0;
    int ambiguous = 0;
    int errors = 0;
};

/// Timestamp of the first record, in microseconds since the epoch, or 0 if unreadable
quint64 readStartUsecs(const QString& telemetryPath);

/// First and last record timestamps, walking the record framing to the end of the file
Span readSpan(const QString& telemetryPath);

Result pairFolder(const QString& folderPath, const QString& telemetryDir);

/// Pairs every unpaired pulse log folder under logDir, except skipFolder (the one still receiving logs)
Summary pairAll(const QString& logDir, const QString& telemetryDir, const QString& skipFolder = QString());

}  // namespace TelemetryPairing
