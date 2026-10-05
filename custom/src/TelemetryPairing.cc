#include "TelemetryPairing.h"

#include <algorithm>
#include <optional>

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QSaveFile>
#include <QtCore/QtEndian>

#include "AppSettings.h"
#include "CustomLoggingCategory.h"
#include "PulseLogSidecar.h"

namespace TelemetryPairing {

namespace {

// Pulse log times and telemetry timestamps come from the same clock; this only absorbs rounding
constexpr quint64 ToleranceUsecs = 1'000'000;
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

bool sameContents(const QString& pathA, const QString& pathB)
{
    QFile fileA(pathA);
    QFile fileB(pathB);
    if (!fileA.open(QIODevice::ReadOnly) || !fileB.open(QIODevice::ReadOnly) || fileA.size() != fileB.size()) {
        return false;
    }
    constexpr qint64 ChunkBytes = 1024 * 1024;
    while (!fileA.atEnd()) {
        const QByteArray chunkA = fileA.read(ChunkBytes);
        if (chunkA.isEmpty() || chunkA != fileB.read(ChunkBytes)) {
            return false;
        }
    }
    return fileB.atEnd();
}

bool copyInto(const Candidate& candidate, const QString& folderPath)
{
    const QString destination = QDir(folderPath).filePath(candidate.name);
    if (QFileInfo::exists(destination) && sameContents(candidate.path, destination)) {
        return true;
    }

    // QSaveFile writes beside the destination and swaps it in only once complete, so a failed copy
    // leaves any existing file as it was. Where rename is impossible it writes in place instead; a
    // failed copy can then leave a partial file, which the next pairing pass finds different and replaces.
    QFile source(candidate.path);
    QSaveFile target(destination);
    target.setDirectWriteFallback(true);
    if (!source.open(QIODevice::ReadOnly) || !target.open(QIODevice::WriteOnly)) {
        qCWarning(CustomPluginLog) << "Unable to copy telemetry file" << candidate.path << "to" << destination;
        return false;
    }
    constexpr qint64 ChunkBytes = 1024 * 1024;
    while (!source.atEnd()) {
        const QByteArray chunk = source.read(ChunkBytes);
        if (chunk.isEmpty() || target.write(chunk) != chunk.size()) {
            target.cancelWriting();
            qCWarning(CustomPluginLog) << "Unable to copy telemetry file" << candidate.path << "to" << destination;
            return false;
        }
    }
    if (!target.commit()) {
        qCWarning(CustomPluginLog) << "Unable to copy telemetry file" << candidate.path << "to" << destination
                                   << target.errorString();
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

    // QGroundControl names a second save in the same second flight.1.tlog, so the shortest name is
    // the original. Alphabetical after that keeps the choice repeatable.
    std::sort(matches.begin(), matches.end(), [](const Candidate* a, const Candidate* b) {
        return a->name.size() != b->name.size() ? a->name.size() < b->name.size() : a->name < b->name;
    });

    // Recordings never overlap, so several matches are normally copies of one file. Only identical
    // bytes prove that. If they differ, naming either could be wrong, so name neither.
    const Candidate* first = matches.first();
    for (const Candidate* match : matches) {
        if (match == first) {
            continue;
        }
        if (match->size != first->size || match->span->startUsecs != first->span->startUsecs ||
            match->span->endUsecs != first->span->endUsecs || !sameContents(match->path, first->path)) {
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
    const bool mapped = data != nullptr;
    if (!mapped) {
        if (size > MaxUnmappedReadBytes) {
            qCWarning(CustomPluginLog) << "Unable to map telemetry file" << telemetryPath << file.errorString();
            return Span();
        }
        unmapped = file.readAll();
        // A short read, from an I/O error or a removed drive, would leave the walk below past the buffer
        if (unmapped.size() != size) {
            qCWarning(CustomPluginLog) << "Short read of telemetry file" << telemetryPath << unmapped.size() << "of"
                                       << size << "bytes";
            return Span();
        }
        data = reinterpret_cast<const uchar*>(unmapped.constData());
    }

    Span span;
    span.startUsecs = parseTimestamp(data);
    span.endUsecs = span.startUsecs;
    const quint64 nowUsecs = usecsFromDateTime(QDateTime::currentDateTimeUtc());

    // Length of a complete record at offset whose timestamp lies between the start and now, or -1
    const auto recordLength = [&](qint64 offset) -> qint64 {
        if (offset + TimestampBytes >= size) {
            return -1;
        }
        const quint64 timestamp = parseTimestamp(data + offset);
        if (timestamp < span.startUsecs || timestamp > nowUsecs) {
            return -1;
        }
        const qint64 length = frameLength(data + offset + TimestampBytes, size - offset - TimestampBytes);
        if (length <= 0 || offset + TimestampBytes + length > size) {
            return -1;
        }
        return TimestampBytes + length;
    };

    // Each record is a timestamp followed by one MAVLink frame. A record that does not parse is
    // skipped a byte at a time. Bytes can line up as a record by chance, so after a skip a record
    // only counts if the next one lines up too, or it ends the file.
    qint64 pos = 0;
    qint64 records = 0;
    bool resyncing = false;
    while (pos + TimestampBytes < size) {
        const qint64 length = recordLength(pos);
        const bool confirmed =
            length > 0 && (!resyncing || pos + length == size || recordLength(pos + length) > 0);
        if (confirmed) {
            span.endUsecs = std::max(span.endUsecs, parseTimestamp(data + pos));
            records++;
            pos += length;
            resyncing = false;
        } else {
            pos++;
            resyncing = true;
        }
    }

    if (mapped) {
        file.unmap(const_cast<uchar*>(data));
    }
    // The first eight bytes alone are not a recording
    return records > 0 ? span : Span();
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
