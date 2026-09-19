#pragma once

#include "UnitTest.h"

class OperationProgressTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _fractionIndeterminateWhenNoStepCount();
    void _fractionClampsToOne();
    void _titleFromCommand();
    void _titleFromMessageForControllerInitiated();
    void _titleKeptAcrossRunningUpdates();
    void _completeAndFailedStates();
    void _sameIdsAfterCompleteStartNewOperation();
    void _duplicateTerminalFrameIsIgnored();
    void _resetHidesOnce();
    void _staleRunningHides();
    void _completeLingersThenHides();
};
