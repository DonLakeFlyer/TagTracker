#pragma once

#include "UnitTest.h"

class TelemetryPairingTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _readSpanWalksMixedRecords();
    void _readSpanIgnoresTruncatedTail();
    void _pairsByContentNotName();
    void _groundTestLeavesTelemetryEmpty();
    void _twoFlightsOnOneRecording();
    void _duplicateCopiesPairWithFirstName();
    void _differentMatchesAreAmbiguous();
    void _pairAllSkipsOpenFolderAndRepeatsSafely();
};
