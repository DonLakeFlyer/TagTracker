#include "FactWaitForValueTarget.h"
#include "CustomLoggingCategory.h"
#include "Fact.h"

FactWaitForValueTarget::FactWaitForValueTarget(QState* parentState, Fact* fact, double targetValue, double targetVariance, int waitMsecs)
    : CustomState("FactWaitForValueTarget", parentState)
    , _fact                 (fact)
    , _targetValue          (targetValue)
    , _targetVariance       (targetVariance)
{
    connect(&_targetWaitTimer, &QTimer::timeout, this, &FactWaitForValueTarget::_waitTimeout);

    _targetWaitTimer.setSingleShot(true);
    _targetWaitTimer.setInterval(waitMsecs);

    connect(this, &QState::entered, this, [this, waitMsecs] () {
            qCDebug(CustomPluginLog) << "Waiting for fact:" << _fact->name()
                                     << "targetValue:" << _targetValue
                                     << "targetVariance:" << _targetVariance
                                     << "timeoutSecs:" << waitMsecs / 1000.0;
            connect(_fact, &Fact::rawValueChanged, this, &FactWaitForValueTarget::_rawValueChanged);
            _targetWaitTimer.start();
        });

    connect(this, &QState::exited, this, &FactWaitForValueTarget::_disconnectAll);
}

// This will advance the state machine if the value reaches the target value
void FactWaitForValueTarget::_rawValueChanged(QVariant rawValue)
{
    Fact* fact = dynamic_cast<Fact*>(sender());
    if (!fact) {
        qCCritical(CustomPluginLog) << "Fact dynamic cast failed!";
        return;
    }
    if (fact != _fact) {
        qCCritical(CustomPluginLog) << "Fact mismatch!";
        return;
    }

    if (qAbs(rawValue.toDouble() - _targetValue) <= _targetVariance) {
        // Target value reached
        qCDebug(CustomPluginLog) << "Target value reached fact:" << _fact->name()
                                 << "targetValue:" << _targetValue;
        _disconnectAll();
        emit success();
    }
}

void FactWaitForValueTarget::_waitTimeout()
{
    qCDebug(CustomPluginLog) << "Timeout waiting for fact:" << _fact->name()
                             << "targetValue:" << _targetValue
                             << "targetVariance:" << _targetVariance
                             << "currentValue:" << _fact->rawValue().toDouble();
    setError(QStringLiteral("Timeout waiting for target value"));
}

void FactWaitForValueTarget::_disconnectAll()
{
    _targetWaitTimer.stop();
    disconnect(&_targetWaitTimer, &QTimer::timeout, this, &FactWaitForValueTarget::_waitTimeout);
    disconnect(_fact, &Fact::rawValueChanged, this, &FactWaitForValueTarget::_rawValueChanged);
}
