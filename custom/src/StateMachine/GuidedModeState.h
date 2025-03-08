#pragma once

#include "CustomState.h"

#include <QStringList>

class Vehicle;

// This state will signal canceled if the flight mode changes to one that is not in the allowed list

class GuidedModeState : public CustomState
{
    Q_OBJECT

public:
    GuidedModeState(const QString& stateName, QState* parentState);

private slots:
    void _flightModeChanged(const QString& flightMode);

private:
    Vehicle*    _vehicle = nullptr;
    QStringList _allowedFlightModes;
};
