#include "OperationProgress.h"
#include "CustomLoggingCategory.h"
#include "TunnelProtocolText.h"

#include <QtCore/QDebug>

#include <cstring>

using namespace TunnelProtocol;

OperationProgress::OperationProgress(QObject* parent)
    : QObject(parent)
{
    _hideTimer.setSingleShot(true);
    connect(&_hideTimer, &QTimer::timeout, this, &OperationProgress::_hide);
    _stallTimer.setSingleShot(true);
    connect(&_stallTimer, &QTimer::timeout, this, &OperationProgress::_stall);
}

double OperationProgress::fraction() const
{
    if (_stepCount == 0) {
        return -1.0;
    }
    return static_cast<double>(qMin(_step, _stepCount)) / _stepCount;
}

QString OperationProgress::titleForCommand(uint32_t command)
{
    switch (command) {
    case COMMAND_ID_START_DETECTION:    return tr("Starting detection");
    case COMMAND_ID_STOP_DETECTION:     return tr("Stopping detection");
    case COMMAND_ID_RAW_CAPTURE:        return tr("Raw capture");
    case COMMAND_ID_SAVE_LOGS:          return tr("Saving logs");
    case COMMAND_ID_CLEAN_LOGS:         return tr("Deleting logs");
    case COMMAND_ID_START_COLLECTION:   return tr("Rotation");
    }
    return tr("Controller busy");
}

void OperationProgress::handleFrame(const OperationProgress_t& frame)
{
    // The controller's own title travels in the first frame's message; later
    // frames carry per-step detail. Post-flight analysis reuses STOP_DETECTION
    // with request_id 0, so take the title from the message there too.
    const QString message = QString::fromUtf8(frame.message, strnlen(frame.message, sizeof(frame.message)));
    const bool sameOperation = _active && frame.command == _command && frame.request_id == _requestId;

    // The controller re-sends the terminal frame for a few ticks; a repeat must
    // not restart the linger or be logged as a new operation.
    if (sameOperation && _state != OPERATION_STATE_RUNNING && frame.state == _state) {
        return;
    }

    const bool newOperation = !sameOperation || _state != OPERATION_STATE_RUNNING;
    // A larger step_count with the same step is a re-sized layout, not progress.
    const bool stepAdvanced = newOperation || frame.step > _step;

    if (newOperation) {
        _title = (frame.request_id == 0 && !message.isEmpty()) ? message : titleForCommand(frame.command);
        qCDebug(CustomPluginLog).noquote() << "OperationProgress begin title:" << _title
                                           << "command:" << TunnelProtocolText::commandName(frame.command)
                                           << "request_id:" << frame.request_id;
    }

    const bool messageChanged = !newOperation && message != _message;

    _active    = true;
    _command   = frame.command;
    _requestId = frame.request_id;
    _state     = frame.state;
    _step      = frame.step;
    _stepCount = frame.step_count;
    _message   = message;

    if (_state == OPERATION_STATE_RUNNING) {
        if (messageChanged) {
            qCDebug(CustomPluginLog).noquote() << "OperationProgress title:" << _title
                                               << "step:" << QStringLiteral("%1/%2").arg(_step).arg(_stepCount)
                                               << "message:" << _message;
        }
        _hideTimer.start(_staleMSecs);
        if (stepAdvanced) {
            // Other operations legitimately hold one step for long stretches
            if (_command == COMMAND_ID_START_COLLECTION) {
                _stallTimer.start(_stallMSecs);
            } else {
                _stallTimer.stop();
            }
        }
    } else {
        _stallTimer.stop();
        qCDebug(CustomPluginLog).noquote() << "OperationProgress end state:"
                                           << (_state == OPERATION_STATE_COMPLETE ? "OPERATION_STATE_COMPLETE" : "OPERATION_STATE_FAILED")
                                           << "title:" << _title
                                           << "message:" << _message;
        _hideTimer.start(_lingerMSecs);
    }
    emit changed();
    if (_state != OPERATION_STATE_RUNNING) {
        emit finished(_command, _state == OPERATION_STATE_COMPLETE, _message);
    }
}

void OperationProgress::reset()
{
    _hideTimer.stop();
    _stallTimer.stop();
    _hide();
}

void OperationProgress::restartStallWatch()
{
    if (_active && _state == OPERATION_STATE_RUNNING && _command == COMMAND_ID_START_COLLECTION) {
        _stallTimer.start(_stallMSecs);
    }
}

void OperationProgress::setTimeoutsForTest(int staleMSecs, int lingerMSecs, int stallMSecs)
{
    _staleMSecs = staleMSecs;
    _lingerMSecs = lingerMSecs;
    if (stallMSecs > 0) {
        _stallMSecs = stallMSecs;
    }
}

void OperationProgress::_hide()
{
    if (!_active) {
        return;
    }
    const bool wasRunning = _state == OPERATION_STATE_RUNNING;
    if (wasRunning) {
        qCWarning(CustomPluginLog) << "OperationProgress stale: no update from controller title:" << _title;
    }
    _stallTimer.stop();
    const uint32_t command = _command;
    const QString  message = _message;
    _active = false;
    _state  = 0;
    emit changed();
    if (wasRunning && command == COMMAND_ID_START_COLLECTION) {
        // Frames stopped entirely: for the rotation that is as bad as a frozen step.
        emit stalled(command, message);
    }
}

void OperationProgress::_stall()
{
    if (!_active || _state != OPERATION_STATE_RUNNING) {
        return;
    }
    qCWarning(CustomPluginLog).noquote() << "OperationProgress stalled title:" << _title
                                         << "step:" << QStringLiteral("%1/%2").arg(_step).arg(_stepCount)
                                         << "unchangedMSecs:" << _stallMSecs
                                         << "message:" << _message;
    emit stalled(_command, _message);
}
