#include "SmartRotateAndCaptureState.h"
#include "FunctionState.h"
#include "SayState.h"
#include "DetectorList.h"
#include "CustomPlugin.h"
#include "CustomLoggingCategory.h"
#include "CaptureAtSliceState.h"
#include "SliceSequenceCaptureState.h"

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
    : FunctionState                 ("DetermineSearchTypeState", parentState, std::bind(&DetermineSearchTypeState::_determineNextSearchType, this))
    , _smartRotateAndCaptureState   (parentState)
    , _rotationDivisions            (rotationDivisions)
{
    switch (_rotationDivisions) {
    case 8:
        _fourPointSliceStartList << 0 << 1;
        break;
    case 16:
        _fourPointSliceStartList << 0 << 2 << 1 << 3;
        break;
    default:
        qCCritical(CustomPluginLog) << QStringLiteral("Unsupported rotation divisions %1").arg(_rotationDivisions) << " - " << Q_FUNC_INFO;
        for (int i=0; i<_rotationDivisions; i++) {
            _fourPointSliceStartList << 0;
        }
        break;
    }
}

void DetermineSearchTypeState::_calcStrengthInfo()
{
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
}

void DetermineSearchTypeState::_nextSearchTypeFromInitialFourPoint()
{
    // We have completed a four point search
    // Quadrant being the area between two initial adjacent slices

    _calcStrengthInfo();

    if (_maxSlice == -1) {
        // No pulses detected.  Do another four point search if we haven't exhausted all possibilities yet

        _lastFourPointIndex++;
        if (_lastFourPointIndex >= _fourPointSliceStartList.count()) {
            // We have exhausted all four point searches
            emit pulseDetectionComplete();
            return;
        }

        // Do another four point search
        _lastSearchType = FinalFourPointRotate;
        _smartRotateAndCaptureState->_sliceSequenceState->setup("Four Point Rotate", 
                                                                _fourPointSliceStartList[_lastFourPointIndex],  // firstSlice
                                                                _rotationDivisions / 4 - 1,                     // skipCount
                                                                true,                                           // clockwiseDirection
                                                                4);                                             // sliceCount
        emit continueSearching();
    } else {
        // Fill in data around the strongest slice
        _lastSearchType = FillAroundStrongest;
        _smartRotateAndCaptureState->_sliceSequenceState->setup("Fill Around Strongest", 
                                                                _clampSliceIndex(_maxSlice - 1, _rotationDivisions),    // firstSlice
                                                                1,                                                      // skipCount
                                                                true,                                                   // clockwiseDirection
                                                                2);                                                     // sliceCount
        emit continueSearching();
    }
}

void DetermineSearchTypeState::_determineNextSearchType()
{
    if (_lastSearchType == InitialFourPointRotate) {
        _nextSearchTypeFromInitialFourPoint();
    } else {
        emit pulseDetectionComplete();
    }
}

SmartRotateAndCaptureState::SmartRotateAndCaptureState(QState* parentState)
    : RotateAndCaptureStateBase("SmartRotateAndCaptureState", parentState)
{
    // States
    _sliceSequenceState = new SliceSequenceCaptureState(
                                    "Initial Four Point Rotate",
                                    this,                       // parentState
                                    0,                          // firstSlice
                                    _rotationDivisions / 4 - 1, // skipCount
                                    true,                       // clockwiseDirection
                                    4);                         // sliceCount
    auto determineSearchTypeState = new DetermineSearchTypeState(this, _rotationDivisions);
    auto finalState = new QFinalState(this);

    // Transitions
    _initRotationState->addTransition(_initRotationState, &FunctionState::functionCompleted, _sliceSequenceState);
    _sliceSequenceState->addTransition(_sliceSequenceState, &QState::finished, determineSearchTypeState);
    determineSearchTypeState->addTransition(determineSearchTypeState, &DetermineSearchTypeState::pulseDetectionComplete, _finalState);
    determineSearchTypeState->addTransition(determineSearchTypeState, &DetermineSearchTypeState::continueSearching, _sliceSequenceState);
}
