#pragma once

#include <QtCore/QLoggingCategory>
#include <QtCore/QObject>
#include <QtCore/QProcess>
#include <QtCore/QString>

Q_DECLARE_LOGGING_CATEGORY(CompanionLogDownloaderLog)

/// Copies the companion computer's Logs-* directories to the QGC log save path over WiFi using scp.
/// macOS only. Requires key-based ssh access to the companion (see README "Downloading Companion Logs").
class CompanionLogDownloader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool downloading READ downloading NOTIFY downloadingChanged)

public:
    explicit CompanionLogDownloader(QObject* parent = nullptr);

    bool downloading() const { return _process != nullptr; }

    Q_INVOKABLE void download();

signals:
    void downloadingChanged();

private:
    void _onFinished(QProcess* process, const QString& destDir, int exitCode, QProcess::ExitStatus exitStatus);
    void _releaseProcess(QProcess* process);

    static constexpr const char* _remoteSource = "pi@raspberrypi.local:Logs/Logs-*";

    QProcess* _process = nullptr;
};
