#pragma once

#include "RotateAndCaptureStateBase.h"
#include "FunctionState.h"
#include "CustomState.h"

class QSignalTransition;
class DetermineSearchTypeState;
class WithinQuadrantSearchState;
class SmartRotateAndCaptureState;

class DetermineSearchTypeState : public FunctionState
{
    Q_OBJECT

public:
    DetermineSearchTypeState(SmartRotateAndCaptureState* parentState, int rotationDivisions);
    
signals:
    void withinQuadrantSearch();
    void singleSliceDetection();

private:
    void _determineSearchType();

    SmartRotateAndCaptureState* _smartRotateAndCaptureState = nullptr;
    int     _maxSlice               = -1;
    double  _maxSliceStrength       = 0;
    int     _maxQuadrantSlice       = -1;
    double  _maxQuadrantStrength    = 0;
    bool    _clockwiseSearch        = false;
    int     _rotationDivisions      = 0;
};

class WithinQuadrantSearchState : public CustomState
{
    Q_OBJECT

public:
    WithinQuadrantSearchState(SmartRotateAndCaptureState* parentState, int rotationDivisions, int sliceIndex, bool clockwiseSearch);

private:
    SmartRotateAndCaptureState* _smartRotateAndCaptureState = nullptr;
    int     _rotationDivisions  = -1;
    int     _sliceIndex         = -1;
    bool    _clockwiseSearch    = false;
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
    enum {
        InitialSearchNorth,
        InitialSearchSouth,
        InitialSearchEast,
        InitialSearchWest,
        InitialSearchComplete,
        SearchingForStrongQuadrant,
        WithinQuadrantSearch,    
    };

    CustomState* _initialFourPointRotateState();

private:
    int     _rotateMode                 = InitialSearchNorth;
    int     _lastQuadrantSlice          = -1;
    int     _firstQuadrantSlice         = -1;
    double  _lastQuadrantStrength       = 0;
    bool    _clockwiseSearch            = false;

    friend class WithinQuadrantSearchState;
    friend class DetermineSearchTypeState;
};
