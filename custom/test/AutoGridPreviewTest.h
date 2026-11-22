#pragma once

#include "UnitTest.h"

class AutoGridPreviewTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _startLatches();
    void _confirmMessage();
    void _adjustRegenerates();
    void _clamping_data();
    void _clamping();
    void _depthSnapsToWholeLanes();
    void _validationLimitsBlockStart();
    void _stages();
    void _movedTooFar_data();
    void _movedTooFar();
    void _clear();
    void _onlyEditorAdjusts();
};
