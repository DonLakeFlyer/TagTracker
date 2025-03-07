#include "DelayState.h"
#include "CustomLoggingCategory.h"

DelayState::DelayState(QState* parentState, int delayMsecs)
    : CustomState(parentState)
{
    _delayTimer.setSingleShot(true);
    _delayTimer.setInterval(delayMsecs);

    connect(this, &QState::entered, this, [this, delayMsecs] () 
        { 
            qCDebug(CustomPluginLog) << QStringLiteral("Starting delay for %1 secs").arg(delayMsecs / 1000.0) << " - " << Q_FUNC_INFO;
            connect(&_delayTimer, &QTimer::timeout, this, &DelayState::delayComplete);
            _delayTimer.start();
        });
}
