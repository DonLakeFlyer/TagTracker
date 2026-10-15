#include "ScheduledTask.h"

#include <utility>

#include "QGCLoggingCategory.h"

QGC_LOGGING_CATEGORY(ScheduledTaskLog, "Utilities.Timing.ScheduledTask")

ScheduledTask::ScheduledTask(RuntimeScheduler* scheduler, QObject* context)
    : _scheduler(scheduler), _context(context), _state(std::make_shared<State>())
{
    qCDebug(ScheduledTaskLog) << this;
}

ScheduledTask::~ScheduledTask()
{
    qCDebug(ScheduledTaskLog) << this;
    cancel();
}

bool ScheduledTask::schedule(std::chrono::microseconds delay, RuntimeScheduler::Callback callback)
{
    cancel();
    if (!_scheduler || !_context || !callback) {
        return false;
    }
    const auto generation = _state->generation;
    const std::weak_ptr<State> state = _state;
    _state->id = _scheduler->schedule(_context, delay, [state, generation, callback = std::move(callback)]() {
        const auto current = state.lock();
        if (!current || current->generation != generation) {
            return;
        }
        current->id = 0;
        callback();
    });
    return _state->id != 0;
}

bool ScheduledTask::scheduleRepeating(std::chrono::microseconds interval, RuntimeScheduler::Callback callback)
{
    if (!callback) {
        cancel();
        return false;
    }
    return _scheduleRepeating(interval, std::make_shared<RuntimeScheduler::Callback>(std::move(callback)));
}

bool ScheduledTask::_scheduleRepeating(std::chrono::microseconds interval,
                                       std::shared_ptr<RuntimeScheduler::Callback> callback)
{
    return schedule(interval, [this, interval, callback = std::move(callback)]() {
        // Keep the callable alive even if it cancels or replaces this task.
        const auto run = callback;
        (void) _scheduleRepeating(interval, run);
        (*run)();
    });
}

void ScheduledTask::cancel()
{
    ++_state->generation;
    const auto id = std::exchange(_state->id, 0);
    if (_scheduler && id) {
        _scheduler->cancel(id);
    }
}

bool ScheduledTask::active() const
{
    return _scheduler && _context && _state->id != 0;
}
