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

void DetermineSearchTypeState::_nextSearchTypeFromFourPoint()
{
    // We have completed a four point search
    // Quadrant being the area between two initial adjacent slices

    _calcStrengthInfo();

    if (_maxSlice == -1) {
        // No pulses detected.  Do another four point search if we haven't exhausted all possibilities yet

        // FIXME: NYI
    } else if (_maxQuadrantSlice == -1) {
        // This means we only have a single strong slice. We look to the left and right to determine quadrant direction
        _lastSearchType = SingleSliceSearch;
        emit singleSliceDetection();
    } else {
        // We found a strong quadrant. Continue searching within the quadrant

        _lastSearchType = QuadrantSearch;
        _lastSliceIndex = _maxQuadrantSlice + (_clockwiseSearch ? 1 : -1);
        _lastClockwiseSearch = _clockwiseSearch;

        int quadrantSliceCount = _rotationDivisions / 4 - 1;
    
        auto quadrantSearchState = new SliceSequenceCaptureState(
                                            _smartRotateAndCaptureState,    // parentState
                                            _lastSliceIndex,                // firstSlice
                                            0,                              // skipCount
                                            _lastClockwiseSearch,           // clockwiseDirection
                                            quadrantSliceCount);            // sliceCount

        addTransition(this, &DetermineSearchTypeState::withinQuadrantSearch, quadrantSearchState);
        quadrantSearchState->addTransition(quadrantSearchState, &QState::finished, _smartRotateAndCaptureState->_announceRotateCompleteState);

        emit withinQuadrantSearch();
    }
}

void DetermineSearchTypeState::_nextSearchTypeFromSingleSlice()
{

}

void DetermineSearchTypeState::_determineNextSearchType()
{
    switch (_lastSearchType) {
    case FourPointRotate:
        _nextSearchTypeFromFourPoint();
        break;
    case SingleSliceSearch:
        _nextSearchTypeFromSingleSlice();
        break;
    default:
        qCCritical(CustomPluginLog) << QStringLiteral("Unsupported search type%1").arg(_lastSearchType) << " - " << Q_FUNC_INFO;
        break;
    }
}

SmartRotateAndCaptureState::SmartRotateAndCaptureState(QState* parentState)
    : RotateAndCaptureStateBase("SmartRotateAndCaptureState", parentState)
{
    // States
    auto fourPointState = new SliceSequenceCaptureState(
                                    this,                       // parentState
                                    0,                          // firstSlice
                                    _rotationDivisions / 4 - 1, // skipCount
                                    true,                       // clockwiseDirection
                                    4);                         // sliceCount
    auto determineSearchTypeState = new DetermineSearchTypeState(this, _rotationDivisions);

    // Transitions
    _initRotationState->addTransition(_initRotationState, &FunctionState::functionCompleted, fourPointState);
    fourPointState->addTransition(fourPointState, &QState::finished, determineSearchTypeState);
}
