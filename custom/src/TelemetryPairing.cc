#include "TelemetryPairing.h"

#include <algorithm>
#include <optional>

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QtEndian>

#include "AppSettings.h"
#include "CustomLoggingCategory.h"
#include "PulseLogSidecar.h"

namespace TelemetryPairing {

namespace {

// Pulse log times and telemetry timestamps come from the same clock; this only absorbs rounding
constexpr quint64 ToleranceUsecs = 1'000'000;
// When resynchronising after an unparseable record, reject timestamps this far past the start
constexpr quint64 MaxRecordingUsecs = 7ull * 24 * 3600 * 1'000'000;
constexpr qint64 MaxUnmappedReadBytes = 512ll * 1024 * 1024;
constexpr int TimestampBytes = 8;

struct Candidate
{
    QString path;
    QString name;
    qint64 size = 0;
    quint64 modifiedUsecs = 0;
    std::optional<quint64> startUsecs;
    std::optional<Span> span;
};

struct FolderRuns
{
    QStringList sidecarPaths;
    QStringList unpairedSidecarPaths;
    quint64 earliestStartUsecs = 0;
    quint64 latestStartUsecs = 0;
};

quint64 usecsFromDateTime(const QDateTime& dateTime)
{
    return static_cast<quint64>(dateTime.toMSecsSinceEpoch()) * 1000;
}

// Same interpretation as LogReplayWorker::_parseTimestamp, including its fallback for byte-swapped logs
quint64 parseTimestamp(const uchar* bytes)
{
    const quint64 nowUsecs = usecsFromDateTime(QDateTime::currentDateTimeUtc());
    quint64 timestamp = qFromBigEndian<quint64>(bytes);
    if (timestamp > nowUsecs) {
        timestamp = qbswap(timestamp);
    }
    return timestamp;
}

// Length of the MAVLink frame at bytes, or -1 if it does not start one. Signatures are stripped
// from received messages before logging, but sent bytes are logged as sent and may carry one.
qint64 frameLength(const uchar* bytes, qint64 available)
{
    if (available < 3) {
        return -1;
    }
    if (bytes[0] == 0xFE) {
        return bytes[1] + 8;
    }
    if (bytes[0] == 0xFD) {
        return bytes[1] + 12 + ((bytes[2] & 0x01) ? 13 : 0);
    }
    return -1;
}

QList<Candidate> listTelemetryFiles(const QString& telemetryDir)
{
    QList<Candidate> candidates;
    const QFileInfoList infos =
        QDir(telemetryDir)
            .entryInfoList({QStringLiteral("*.%1").arg(AppSettings::telemetryFileExtension)}, QDir::Files, QDir::Name);
    for (const QFileInfo& info : infos) {
        Candidate candidate;
        candidate.path = info.absoluteFilePath();
        candidate.name = info.fileName();
        candidate.size = info.size();
        candidate.modifiedUsecs = usecsFromDateTime(info.lastModified());
        candidates.append(candidate);
    }
    return candidates;
}

bool readFolderRuns(const QString& folderPath, FolderRuns& runs)
{
    const QDir folder(folderPath);
    const QStringList names = folder.entryList({QStringLiteral("Pulse-*.json")}, QDir::Files, QDir::Name);
    for (const QString& name : names) {
        const QString path = folder.filePath(name);
        QJsonObject sidecar;
        if (!PulseLogSidecar::read(path, sidecar)) {
            continue;
        }
        const QDateTime start = PulseLogSidecar::parseUtc(sidecar[PulseLogSidecar::keyStartUtc]);
        if (!start.isValid()) {
            continue;
        }
        const quint64 startUsecs = usecsFromDateTime(start);
        if (runs.sidecarPaths.isEmpty()) {
            runs.earliestStartUsecs = runs.latestStartUsecs = startUsecs;
        } else {
            runs.earliestStartUsecs = std::min(runs.earliestStartUsecs, startUsecs);
            runs.latestStartUsecs = std::max(runs.latestStartUsecs, startUsecs);
        }
        runs.sidecarPaths.append(path);
        if (sidecar[PulseLogSidecar::keyTelemetryFile].toString().isEmpty()) {
            runs.unpairedSidecarPaths.append(path);
        }
    }
    return !runs.sidecarPaths.isEmpty();
}

bool covers(Candidate& candidate, const FolderRuns& runs)
{
    // The saved file is written at disconnect, after every run in the folder started, so an
    // older modification time rules a file out without opening it
    if (candidate.modifiedUsecs + ToleranceUsecs < runs.latestStartUsecs) {
        return false;
    }
    if (!candidate.startUsecs) {
        candidate.startUsecs = readStartUsecs(candidate.path);
    }
    if (*candidate.startUsecs == 0 || *candidate.startUsecs > runs.earliestStartUsecs + ToleranceUsecs) {
        return false;
    }
    if (!candidate.span) {
        candidate.span = readSpan(candidate.path);
    }
    return candidate.span->isValid() && candidate.span->endUsecs + ToleranceUsecs >= runs.latestStartUsecs;
}

bool copyInto(const Candidate& candidate, const QString& folderPath)
{
    const QString destination = QDir(folderPath).filePath(candidate.name);
    if (QFileInfo::exists(destination)) {
        if (QFileInfo(destination).size() == candidate.size) {
            return true;
        }
        QFile::remove(destination);
    }
    if (!QFile::copy(candidate.path, destination)) {
        qCWarning(CustomPluginLog) << "Unable to copy telemetry file" << candidate.path << "to" << destination;
        return false;
    }
    return true;
}

Result pairFolderWith(const QString& folderPath, QList<Candidate>& telemetryFiles)
{
    FolderRuns runs;
    if (!readFolderRuns(folderPath, runs)) {
        return Result::NoSidecars;
    }
    if (runs.unpairedSidecarPaths.isEmpty()) {
        return Result::AlreadyPaired;
    }

    QList<Candidate*> matches;
    for (Candidate& candidate : telemetryFiles) {
        if (covers(candidate, runs)) {
            matches.append(&candidate);
        }
    }

    if (matches.isEmpty()) {
        qCDebug(CustomPluginLog) << "No telemetry file covers" << folderPath;
        return Result::NoTelemetry;
    }

    // Recordings never overlap, so several matches are copies of one file unless proven otherwise.
    // If they differ, naming either could be wrong, so name neither.
    const Candidate* first = matches.first();
    for (const Candidate* match : matches) {
        if (match->size != first->size || match->span->startUsecs != first->span->startUsecs ||
            match->span->endUsecs != first->span->endUsecs) {
            QStringList names;
            for (const Candidate* each : matches) {
                names.append(each->name);
            }
            qCWarning(CustomPluginLog) << "Several different telemetry files cover" << folderPath
                                       << "leaving it unpaired:" << names;
            return Result::Ambiguous;
        }
    }

    if (!copyInto(*first, folderPath)) {
        return Result::Error;
    }
    for (const QString& sidecarPath : runs.unpairedSidecarPaths) {
        if (!PulseLogSidecar::setValue(sidecarPath, PulseLogSidecar::keyTelemetryFile, first->name)) {
            return Result::Error;
        }
    }
    qCDebug(CustomPluginLog) << "Paired" << folderPath << "with" << first->name;
    return Result::Paired;
}

}  // namespace

quint64 readStartUsecs(const QString& telemetryPath)
{
    QFile file(telemetryPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }
    const QByteArray bytes = file.read(TimestampBytes);
    if (bytes.size() < TimestampBytes) {
        return 0;
    }
    return parseTimestamp(reinterpret_cast<const uchar*>(bytes.constData()));
}

Span readSpan(const QString& telemetryPath)
{
    QFile file(telemetryPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return Span();
    }
    const qint64 size = file.size();
    if (size <= TimestampBytes) {
        return Span();
    }

    QByteArray unmapped;
    const uchar* data = file.map(0, size);
    if (!data) {
        if (size > MaxUnmappedReadBytes) {
            qCWarning(CustomPluginLog) << "Unable to map telemetry file" << telemetryPath << file.errorString();
            return Span();
        }
        unmapped = file.readAll();
        data = reinterpret_cast<const uchar*>(unmapped.constData());
    }

    Span span;
    span.startUsecs = parseTimestamp(data);
    span.endUsecs = span.startUsecs;

    // Each record is a timestamp followed by one MAVLink frame. A record that does not parse is
    // skipped a byte at a time until a plausible timestamp and frame start line up again.
    qint64 pos = 0;
    while (pos + TimestampBytes < size) {
        const quint64 timestamp = parseTimestamp(data + pos);
        const qint64 length = frameLength(data + pos + TimestampBytes, size - pos - TimestampBytes);
        if (length > 0 && pos + TimestampBytes + length <= size && timestamp >= span.startUsecs &&
            timestamp - span.startUsecs <= MaxRecordingUsecs) {
            span.endUsecs = std::max(span.endUsecs, timestamp);
            pos += TimestampBytes + length;
        } else {
            pos++;
        }
    }

    if (unmapped.isEmpty()) {
        file.unmap(const_cast<uchar*>(data));
    }
    return span;
}

Result pairFolder(const QString& folderPath, const QString& telemetryDir)
{
    QList<Candidate> telemetryFiles = listTelemetryFiles(telemetryDir);
    return pairFolderWith(folderPath, telemetryFiles);
}

Summary pairAll(const QString& logDir, const QString& telemetryDir, const QString& skipFolder)
{
    Summary summary;
    QList<Candidate> telemetryFiles = listTelemetryFiles(telemetryDir);
    const QString skip = skipFolder.isEmpty() ? QString() : QDir(skipFolder).absolutePath();

    const QFileInfoList folders = QDir(logDir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& folder : folders) {
        if (folder.absoluteFilePath() == skip) {
            continue;
        }
        switch (pairFolderWith(folder.absoluteFilePath(), telemetryFiles)) {
            case Result::Paired:
                summary.paired++;
                break;
            case Result::NoTelemetry:
                summary.noTelemetry++;
                break;
            case Result::Ambiguous:
                summary.ambiguous++;
                break;
            case Result::Error:
                summary.errors++;
                break;
            case Result::AlreadyPaired:
            case Result::NoSidecars:
                break;
        }
    }
    return summary;
}

}  // namespace TelemetryPairing
