#include "SliceSequenceCaptureState.h"
#include "CaptureAtSliceState.h"
#include "CustomPlugin.h"
#include "CustomLoggingCategory.h"

#include <QFinalState>

SliceSequenceCaptureState::SliceSequenceCaptureState(const QString& stateName, QState* parentState, int firstSlice, int skipCount, bool clockwiseDirection, int sliceCount)
    : CustomState(stateName, parentState)
{
    setup(stateName, firstSlice, skipCount, clockwiseDirection, sliceCount);
}

SliceSequenceCaptureState::SliceSequenceCaptureState(const QString& stateName, QState* parentState)
    : CustomState(stateName, parentState)
{

}

void SliceSequenceCaptureState::setup(const QString& stateName, int firstSlice, int skipCount, bool clockwiseDirection, int sliceCount)
{
    setObjectName(stateName);
    _rotationDivisions = qobject_cast<CustomPlugin*>(CustomPlugin::instance())->customSettings()->divisions()->rawValue().toInt();

    QList<CustomState*> sliceStates;

    for (int i=0; i<sliceCount; i++) {
        int sliceIndex = _clampSliceIndex(firstSlice + (i * (skipCount + 1) * (clockwiseDirection ? 1 : -1)));
        auto state = new CaptureAtSliceState(this, sliceIndex);
        sliceStates.append(state);
    }

    for (int i=0; i<sliceCount-1; i++) {
        auto currentState = sliceStates[i];
        auto nextState = sliceStates[i + 1];
        currentState->addTransition(currentState, &QState::finished, nextState);
    }
    sliceStates.last()->addTransition(sliceStates.last(), &QState::finished, new QFinalState(this));

    connect(this, &QState::entered, this, [this, firstSlice, skipCount, clockwiseDirection, sliceCount] () {
        qCDebug(CustomPluginLog) << "Four point rotate and capture at firstSlice:skipCount:clockwiseDirection:sliceCount" << firstSlice << skipCount << clockwiseDirection << sliceCount << Q_FUNC_INFO;
    });

    setInitialState(sliceStates.first());
}

int SliceSequenceCaptureState::_clampSliceIndex(int index)
{
    if (index >= _rotationDivisions) {
        return index - _rotationDivisions;
    } else if (index < 0) {
        return index + _rotationDivisions;
    }
    return index;
}
