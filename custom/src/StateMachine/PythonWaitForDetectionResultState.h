#pragma once

#include "CustomState.h"

#include <QTimer>

class CustomPlugin;

// Waits for the controller to report COLLECTION_STATUS_SLICE_COMPLETE for one
// collection slice.  Used by PythonCaptureAtSliceState to know when the Python
// detectors have finished one scan cycle at a heading.
class PythonWaitForDetectionResultState : public CustomState
{
    Q_OBJECT

public:
    // timeoutMsecs: how long to wait before declaring an error (0 = no timeout)
    PythonWaitForDetectionResultState(QState* parentState, uint32_t collectionId, uint32_t sliceId, int timeoutMsecs = 30000);

signals:
    void resultsReceived();

private slots:
    void _collectionStatusReceived(uint32_t collectionId, uint32_t sliceId, uint32_t status, uint32_t errorCode);

private:
    void _startListening();
    void _disconnectAll();

    CustomPlugin*   _customPlugin       = nullptr;
    uint32_t        _collectionId = 0;
    uint32_t        _sliceId = 0;
    QTimer          _timeoutTimer;
};
