#include "CompanionLogDownloader.h"

#include "AppSettings.h"
#include "QGCApplication.h"
#include "QGCLoggingCategory.h"
#include "SettingsManager.h"

QGC_LOGGING_CATEGORY(CompanionLogDownloaderLog, "Custom.CompanionLogDownloader")

CompanionLogDownloader::CompanionLogDownloader(QObject* parent) : QObject(parent) {}

void CompanionLogDownloader::download()
{
#if defined(Q_OS_MACOS)
    if (_process) {
        qCWarning(CompanionLogDownloaderLog) << "Download already in progress";
        return;
    }

    const QString destDir = SettingsManager::instance()->appSettings()->logSavePath();
    if (destDir.isEmpty()) {
        qCWarning(CompanionLogDownloaderLog) << "Log save path is empty";
        qgcApp()->showAppMessage(tr("Log save path is not available. Check Application Settings > Save Path."));
        return;
    }

    // BatchMode fails fast instead of hanging on a password prompt scp can't receive from QProcess.
    const QStringList args = {
        QStringLiteral("-r"),
        QStringLiteral("-o"),
        QStringLiteral("BatchMode=yes"),
        QStringLiteral("-o"),
        QStringLiteral("StrictHostKeyChecking=accept-new"),
        QStringLiteral("-o"),
        QStringLiteral("ConnectTimeout=10"),
        QString::fromLatin1(_remoteSource),
        destDir + QLatin1Char('/'),
    };

    qCDebug(CompanionLogDownloaderLog) << "Starting scp"
                                       << "source:" << _remoteSource << "destDir:" << destDir;

    auto* process = new QProcess(this);
    _process = process;
    connect(process, &QProcess::finished, this,
            [this, process, destDir](int exitCode, QProcess::ExitStatus exitStatus) {
                _onFinished(process, destDir, exitCode, exitStatus);
            });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) {
            return;
        }
        qCWarning(CompanionLogDownloaderLog) << "scp failed to start:" << process->errorString();
        qgcApp()->showAppMessage(tr("Unable to start scp: %1").arg(process->errorString()));
        _releaseProcess(process);
    });
    emit downloadingChanged();

    process->start(QStringLiteral("/usr/bin/scp"), args);
#else
    qgcApp()->showAppMessage(tr("Downloading companion logs is only supported on macOS."));
#endif
}

void CompanionLogDownloader::_onFinished(QProcess* process, const QString& destDir, int exitCode,
                                         QProcess::ExitStatus exitStatus)
{
    const QString stdErr = QString::fromLocal8Bit(process->readAllStandardError()).trimmed();

    qCDebug(CompanionLogDownloaderLog) << "scp finished"
                                       << "exitCode:" << exitCode << "exitStatus:" << exitStatus << "stderr:" << stdErr;

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        qgcApp()->showAppMessage(tr("Companion logs downloaded to %1").arg(destDir));
    } else {
        QString message = tr("Companion log download failed: %1").arg(stdErr.isEmpty() ? tr("unknown error") : stdErr);
        // ssh auth failures read "Permission denied (publickey,...)"; plain "Permission denied" is a file error.
        if (stdErr.contains(QStringLiteral("Permission denied ("), Qt::CaseInsensitive)) {
            message +=
                tr("\n\nSSH key access to the companion is required. See README \"Downloading Companion Logs\".");
        } else if (stdErr.contains(QStringLiteral("HOST IDENTIFICATION HAS CHANGED"), Qt::CaseInsensitive)) {
            message +=
                tr("\n\nThe companion's host key changed (reimaged SD card?). "
                   "Run 'ssh-keygen -R raspberrypi.local' then 'ssh-copy-id pi@raspberrypi.local'.");
        }
        qgcApp()->showAppMessage(message);
    }

    _releaseProcess(process);
}

void CompanionLogDownloader::_releaseProcess(QProcess* process)
{
    process->deleteLater();
    if (_process == process) {
        _process = nullptr;
        emit downloadingChanged();
    }
}
