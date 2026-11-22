#pragma once

#include "UnitTest.h"

class GridGeneratorTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _referenceMatch_data();
    void _referenceMatch();
    void _workedExamplePath();
    void _defaultShape_data();
    void _defaultShape();
    void _cellToCoordinate_data();
    void _cellToCoordinate();
    void _clampedLastLane();
    void _sliverLaneDropped();
    void _turnSpeedZero();
    void _planManagerItems();
    void _validate_data();
    void _validate();
};
