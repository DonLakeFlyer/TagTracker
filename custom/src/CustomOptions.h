#pragma once

#include "QGCOptions.h"

class CustomOptions : public QGCOptions
{
public:
    CustomOptions(QObject* parent = nullptr);

    // QGCOptions overrides
    virtual bool guidedBarShowOrbit () const final { return false; }
    virtual bool guidedBarShowROI   () const final { return false; }
};
