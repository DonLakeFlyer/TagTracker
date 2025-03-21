#pragma once

#include "RotateAndCaptureStateBase.h"
#include "FunctionState.h"
#include "CustomState.h"

#include <Qlist>

class SmartRotateAndCaptureState;

class DetermineSearchTypeState : public FunctionState
{
    Q_OBJECT

public:
    DetermineSearchTypeState(SmartRotateAndCaptureState* parentState, int rotationDivisions);
    
signals:
    void withinQuadrantSearch();
    void singleSliceDetection();
    void nextFourPointSearch();
    void pulseDetectionComplete();

private:
    enum {
        FourPointRotate,
        SingleSliceSearch,
        QuadrantSearch
    };

    void _determineNextSearchType();
    void _nextSearchTypeFromFourPoint();
    void _nextSearchTypeFromSingleSlice();
    void _calcStrengthInfo();

    SmartRotateAndCaptureState* _smartRotateAndCaptureState = nullptr;
    int     _maxSlice                   = -1;
    double  _maxSliceStrength           = 0;
    int     _maxQuadrantSlice           = -1;
    double  _maxQuadrantStrength        = 0;
    bool    _clockwiseSearch            = false;
    int     _rotationDivisions          = 0;
    int     _lastSearchType             = FourPointRotate;
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

    friend class DetermineSearchTypeState;
};
