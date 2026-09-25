#pragma once

#include "CustomState.h"

class CustomPlugin;

// Waits for the controller to report COLLECTION_STATUS_SLICE_COMPLETE for one
// collection slice.  Used by PythonCaptureAtSliceState to know when the Python
// detectors have finished one scan cycle at a heading. There is no fixed
// timeout: the controller's rotation OPERATION_PROGRESS step advances every
// second of the dwell, so a step that stops moving (OperationProgress::stalled)
// or the operation ending early (OperationProgress::finished) is the failure signal.
class PythonWaitForDetectionResultState : public CustomState
{
    Q_OBJECT

public:
    PythonWaitForDetectionResultState(QState* parentState, uint32_t collectionId, uint32_t sliceId);

signals:
    void resultsReceived();

private slots:
    void _collectionStatusReceived(uint32_t collectionId, uint32_t sliceId, uint32_t status, uint32_t errorCode);
    void _progressStalled(uint32_t command, const QString& lastMessage);
    void _progressFinished(uint32_t command, bool success, const QString& message);

private:
    void _startListening();
    void _disconnectAll();

    CustomPlugin*   _customPlugin       = nullptr;
    uint32_t        _collectionId = 0;
    uint32_t        _sliceId = 0;
    bool            _listening = false;
};
