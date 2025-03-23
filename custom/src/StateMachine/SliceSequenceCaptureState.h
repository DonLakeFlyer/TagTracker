#pragma once

#include "CustomState.h"

class SliceSequenceCaptureState : public CustomState
{
    Q_OBJECT

public:
    SliceSequenceCaptureState(const QString& stateName, QState* parentState, int firstSlice, int skipCount, bool clockwiseDirection, int sliceCount);
    SliceSequenceCaptureState(const QString& stateName, QState* parentState);

    void setup(const QString& stateName, int firstSlice, int skipCount, bool clockwiseDirection, int sliceCount);

private:
    int _clampSliceIndex(int index);

    int _rotationDivisions = 0;
};
