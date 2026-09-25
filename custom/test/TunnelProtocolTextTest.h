#pragma once

#include "UnitTest.h"

class TunnelProtocolTextTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _formatsAck();
    void _payloadShorterThanHeader();
    void _payloadSizeMismatch();
    void _unknownCommandAndValues();
    void _unterminatedTextIsBounded();
};
