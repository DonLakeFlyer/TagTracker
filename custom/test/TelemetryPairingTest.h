#pragma once

#include "UnitTest.h"

class TelemetryPairingTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _readSpanWalksMixedRecords();
    void _readSpanIgnoresTruncatedTail();
    void _readSpanAcceptsLongRecordings();
    void _readSpanNeedsOneCompleteRecord();
    void _pairsByContentNotName();
    void _groundTestLeavesTelemetryEmpty();
    void _twoFlightsOnOneRecording();
    void _duplicateCopiesPairWithShortestName();
    void _differentMatchesAreAmbiguous();
    void _sameSpanDifferentBytesIsAmbiguous();
    void _replacesWrongCopyInFolder();
    void _pairAllSkipsOpenFolderAndRepeatsSafely();
    void _pairAllIgnoresCompanionLogFolders();
    void _emptyPathsDoNotScanWorkingDirectory();
};
