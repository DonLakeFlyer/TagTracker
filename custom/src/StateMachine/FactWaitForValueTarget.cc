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
            qCDebug(CustomPluginLog) << QStringLiteral("Waiting for target value %1 with variance %2 on %3. Timeout %4 secs").arg(_targetValue).arg(_targetVariance).arg(_fact->name()).arg(waitMsecs / 1000.0) << " - " << Q_FUNC_INFO;
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
        qCCritical(CustomPluginLog) << Q_FUNC_INFO << "Fact dynamic cast failed!";
        return;
    }
    if (fact != _fact) {
        qCCritical(CustomPluginLog) << Q_FUNC_INFO << "Fact mismatch!";
        return;
    }

    if (qAbs(rawValue.toDouble() - _targetValue) <= _targetVariance) {
        // Target value reached
        qCDebug(CustomPluginLog) << QStringLiteral("Target value %1 reached for %2").arg(_targetValue).arg(_fact->name()) << " - " << Q_FUNC_INFO;
        _disconnectAll();
        emit success();
    }
}

void FactWaitForValueTarget::_waitTimeout()
{
    qCDebug(CustomPluginLog) << QStringLiteral("Timeout waiting for target value %1 with variance %2 on %3. Current value %4.").arg(_targetValue).arg(_targetVariance).arg(_fact->name()).arg(_fact->rawValue().toDouble()) << " - " << Q_FUNC_INFO;
    setError(QStringLiteral("Timeout waiting for target value"));
}

void FactWaitForValueTarget::_disconnectAll()
{
    _targetWaitTimer.stop();
    disconnect(&_targetWaitTimer, &QTimer::timeout, this, &FactWaitForValueTarget::_waitTimeout);
    disconnect(_fact, &Fact::rawValueChanged, this, &FactWaitForValueTarget::_rawValueChanged);
}