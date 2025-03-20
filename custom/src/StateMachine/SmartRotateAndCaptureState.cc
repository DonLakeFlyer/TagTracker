#include "SmartRotateAndCaptureState.h"
#include "FunctionState.h"
#include "SayState.h"
#include "DetectorList.h"
#include "CustomPlugin.h"
#include "CustomLoggingCategory.h"

#include <QSignalTransition>
#include <QFinalState>

static int _clampSliceIndex(int index, int rotationDivisions)
{
    if (index >= rotationDivisions) {
        return index - rotationDivisions;
    } else if (index < 0) {
        return index + rotationDivisions;
    }
    return index;
}

DetermineSearchTypeState::DetermineSearchTypeState(SmartRotateAndCaptureState* parentState, int rotationDivisions)
    : FunctionState                 ("DetermineSearchTypeState", parentState, std::bind(&DetermineSearchTypeState::_determineSearchType, this))
    , _smartRotateAndCaptureState   (parentState)
    , _rotationDivisions            (rotationDivisions)
{

}

void DetermineSearchTypeState::_determineSearchType()
{
    // We have completed the initial search and are now looking for the strongest slice or quadrant
    // Quadrant being the area between two initial adjacent slices

    // Find the strongest single slice and quadrant

    auto& rgAngleStrengths = qobject_cast<CustomPlugin*>(CustomPlugin::instance())->rgAngleStrengths().last();
    int sliceIncrement = _rotationDivisions / 4;

    for (int i=0; i < _rotationDivisions; i += sliceIncrement) {
        // Determine strongest slice
        double sliceStrength = rgAngleStrengths[i];
        if (sliceStrength > _maxSliceStrength) {
            _maxSliceStrength = sliceStrength;
            _maxSlice = i;
        }

        if (i + sliceIncrement >= _rotationDivisions) {
            continue;
        }

        // Determine strongest quadrant between two slices
        double leftSliceStrength = rgAngleStrengths[i];
        double rightSliceStrength = rgAngleStrengths[_clampSliceIndex(i + sliceIncrement, _rotationDivisions)];

        if (!(leftSliceStrength > 0.0) || !(rightSliceStrength > 0.0)) {
            // We need both slices to have strength to consider this quadrant
            continue;
        }

        double quadrantStrength = (leftSliceStrength + rightSliceStrength) / 2;
        if (quadrantStrength > _maxQuadrantStrength) {
            _clockwiseSearch = leftSliceStrength > rightSliceStrength   ;
            _maxQuadrantStrength = quadrantStrength;
            _maxQuadrantSlice = i;
        }
    }

    qCDebug(CustomPluginLog) << QStringLiteral("Max strength %1 at slice %2").arg(_maxSliceStrength).arg(_maxSlice) << " - " << Q_FUNC_INFO;
    qCDebug(CustomPluginLog) << QStringLiteral("Max quadrant strength %1 at slice %2, clockwise %3").arg(_maxQuadrantStrength).arg(_maxQuadrantSlice).arg(_clockwiseSearch) << " - " << Q_FUNC_INFO;

    if (_maxQuadrantSlice == -1) {
        // This means we only have a single strong slice
        emit singleSliceDetection();
    } else {
        // We found a strong quadrant. Continue searching within the quadrant
        auto withinQuadrantSearchState = new WithinQuadrantSearchState(_smartRotateAndCaptureState, _rotationDivisions, _maxQuadrantSlice, _clockwiseSearch);
        addTransition(this, &DetermineSearchTypeState::withinQuadrantSearch, withinQuadrantSearchState);
        withinQuadrantSearchState->addTransition(withinQuadrantSearchState, &QState::finished, _smartRotateAndCaptureState->_announceRotateCompleteState);
        emit withinQuadrantSearch();
    }
}

WithinQuadrantSearchState::WithinQuadrantSearchState(SmartRotateAndCaptureState* parentState, int rotationDivisions, int sliceIndex, bool clockwiseSearch)
    : CustomState                   ("WithinQuadrantSearchState", parentState)
    , _smartRotateAndCaptureState   (parentState)
    , _rotationDivisions            (rotationDivisions)
    , _sliceIndex                   (sliceIndex)
    , _clockwiseSearch              (clockwiseSearch)
{
    int quadrantSliceCount = _rotationDivisions / 4 - 1;

    auto& rotationStates = _smartRotateAndCaptureState->_rotationStates;
    auto finalState = new QFinalState(this);

    int currentSlice = _sliceIndex;
    QList<CustomState*> quadrantStates;
    for (int i=0; i<quadrantSliceCount; i++) {
        currentSlice = _clampSliceIndex(currentSlice + (_clockwiseSearch ? 1 : -1), _rotationDivisions);
        quadrantStates.append(_smartRotateAndCaptureState->_rotateAndCaptureAtHeadingState(this, currentSlice));
    }

    for (int i=0; i<quadrantSliceCount; i++) {
        auto currentState = quadrantStates[i];
        
        if (i == quadrantSliceCount - 1) {
            // This is the final slice in the quadrant
            currentState->addTransition(currentState, &QState::finished, finalState);
        } else {
            // Move to the next slice
            auto nextState = quadrantStates[i + 1];
            currentState->addTransition(currentState, &QState::finished, nextState);
        }
    }

    setInitialState(quadrantStates.first());
}

SmartRotateAndCaptureState::SmartRotateAndCaptureState(QState* parentState)
    : RotateAndCaptureStateBase("SmartRotateAndCaptureState", parentState)
{
    // States
    auto initialFourPointRotateState = _initialFourPointRotateState();
    auto determineSearchTypeState = new DetermineSearchTypeState(this, _rotationDivisions);

    // Transitions
    _initRotationState->addTransition(_initRotationState, &FunctionState::functionCompleted, initialFourPointRotateState);
    initialFourPointRotateState->addTransition(initialFourPointRotateState, &QState::finished, determineSearchTypeState);
    determineSearchTypeState->addTransition(determineSearchTypeState, &DetermineSearchTypeState::singleSliceDetection, _finalState);
}

CustomState* SmartRotateAndCaptureState::_initialFourPointRotateState()
{
    auto initialFourPointRotateState = new CustomState("InitialFourPointRotateState", this);

    auto northState = _rotateAndCaptureAtHeadingState(initialFourPointRotateState, 0);
    auto eastState  = _rotateAndCaptureAtHeadingState(initialFourPointRotateState, _rotationDivisions / 4);
    auto southState = _rotateAndCaptureAtHeadingState(initialFourPointRotateState, _rotationDivisions / 2);
    auto westState  = _rotateAndCaptureAtHeadingState(initialFourPointRotateState, _rotationDivisions / 2 + _rotationDivisions / 4);
    auto finalState = new QFinalState(initialFourPointRotateState);

    auto northTransition    = northState->addTransition(northState, &QState::finished, eastState);
    auto eastTransition     = eastState->addTransition(eastState, &QState::finished, southState);
    auto southTransition    = southState->addTransition(southState, &QState::finished, westState);
    auto westTransition     = westState->addTransition(westState, &QState::finished, finalState);

    initialFourPointRotateState->setInitialState(northState);

    return initialFourPointRotateState;
}
