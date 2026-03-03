#pragma once

#include "CustomState.h"

#include <QTimer>
#include <QSet>

class CustomPlugin;

// Waits for either a confirmed pulse or no-pulse (detection_status==3) from every
// detector in DetectorList.  Used by PythonCaptureAtSliceState to know when the
// Python detector has finished one scan cycle at a heading.
class PythonWaitForDetectionResultState : public CustomState
{
    Q_OBJECT

public:
    // timeoutMsecs: how long to wait before declaring an error (0 = no timeout)
    PythonWaitForDetectionResultState(QState* parentState, int timeoutMsecs = 30000);

signals:
    void resultsReceived();

private slots:
    void _pythonDetectorResultReceived(uint32_t tagId);

private:
    void _startListening();
    void _disconnectAll();

    CustomPlugin*   _customPlugin       = nullptr;
    QSet<uint32_t>  _pendingTagIds;
    QTimer          _timeoutTimer;
};
