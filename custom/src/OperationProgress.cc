#include "OperationProgress.h"
#include "CustomLoggingCategory.h"

#include <QtCore/QDebug>

#include <cstring>

using namespace TunnelProtocol;

OperationProgress::OperationProgress(QObject* parent)
    : QObject(parent)
{
    _hideTimer.setSingleShot(true);
    connect(&_hideTimer, &QTimer::timeout, this, &OperationProgress::_hide);
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

    if (newOperation) {
        _title = (frame.request_id == 0 && !message.isEmpty()) ? message : titleForCommand(frame.command);
        qCDebug(CustomPluginLog) << "OperationProgress begin" << _title << "command" << frame.command << "request_id" << frame.request_id;
    }

    _active    = true;
    _command   = frame.command;
    _requestId = frame.request_id;
    _state     = frame.state;
    _step      = frame.step;
    _stepCount = frame.step_count;
    _message   = message;

    if (_state == OPERATION_STATE_RUNNING) {
        _hideTimer.start(_staleMSecs);
    } else {
        qCDebug(CustomPluginLog) << "OperationProgress" << (_state == OPERATION_STATE_COMPLETE ? "complete" : "failed") << _title << _message;
        _hideTimer.start(_lingerMSecs);
    }
    emit changed();
}

void OperationProgress::reset()
{
    _hideTimer.stop();
    _hide();
}

void OperationProgress::setTimeoutsForTest(int staleMSecs, int lingerMSecs)
{
    _staleMSecs = staleMSecs;
    _lingerMSecs = lingerMSecs;
}

void OperationProgress::_hide()
{
    if (!_active) {
        return;
    }
    if (_state == OPERATION_STATE_RUNNING) {
        qCWarning(CustomPluginLog) << "OperationProgress stale: no update from controller for" << _title;
    }
    _active = false;
    _state  = 0;
    emit changed();
}
