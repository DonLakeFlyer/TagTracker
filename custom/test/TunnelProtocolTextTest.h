#pragma once

#include "UnitTest.h"

class TunnelProtocolTextTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _formatsAck();
    void _formatsStartIqRecording();
    void _formatsTagNames();
    void _copyClipped_data();
    void _copyClipped();
    void _payloadShorterThanHeader();
    void _payloadSizeMismatch();
    void _unknownCommandAndValues();
    void _unterminatedTextIsBounded();
};
