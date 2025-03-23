#pragma once

#include "RotateAndCaptureStateBase.h"
#include "FunctionState.h"
#include "CustomState.h"

#include <QList>

class SmartRotateAndCaptureState;
class SliceSequenceCaptureState;

class DetermineSearchTypeState : public FunctionState
{
    Q_OBJECT

public:
    DetermineSearchTypeState(SmartRotateAndCaptureState* parentState, int rotationDivisions);
    
signals:
    void continueSearching();
    void pulseDetectionComplete();

private:
    enum {
        InitialFourPointRotate,
        FinalFourPointRotate,
        FillAroundStrongest,
    };

    void _determineNextSearchType();
    void _nextSearchTypeFromInitialFourPoint();
    void _calcStrengthInfo();

    SmartRotateAndCaptureState* _smartRotateAndCaptureState = nullptr;
    int     _maxSlice                   = -1;
    double  _maxSliceStrength           = 0;
    int     _maxQuadrantSlice           = -1;
    double  _maxQuadrantStrength        = 0;
    bool    _clockwiseSearch            = false;
    int     _rotationDivisions          = 0;
    int     _lastSearchType             = InitialFourPointRotate;
    int     _lastFourPointIndex         = 0;
    int     _lastSliceIndex             = 0;
    bool    _lastClockwiseSearch        = false;
    QList<int> _fourPointSliceStartList;
};

class SmartRotateAndCaptureState : public RotateAndCaptureStateBase
{
    Q_OBJECT

public:
    SmartRotateAndCaptureState(QState* parentState);

signals:
    void _processNextSlice();
    void _allSlicesProcessed();

private:
    CustomState* _initialFourPointRotateState();

private:
    int     _lastQuadrantSlice          = -1;
    int     _firstQuadrantSlice         = -1;
    double  _lastQuadrantStrength       = 0;
    bool    _clockwiseSearch            = false;
    SliceSequenceCaptureState* _sliceSequenceState = nullptr;

    friend class DetermineSearchTypeState;
};
