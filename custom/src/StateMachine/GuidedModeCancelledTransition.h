#pragma once

#include <QEvent>
#include <QAbstractTransition>

class GuidedModeCancelledEvent : public QEvent
{
public:
    GuidedModeCancelledEvent()
        : QEvent(type())
    {}

    static Type type() { return QEvent::Type(QEvent::User + 1); }
};

class GuidedModeCancelledTransition : public QAbstractTransition
{
    Q_OBJECT

public:
    GuidedModeCancelledTransition(QState* parentState)
        : QAbstractTransition(parentState)
    {}

protected:
    // QAbstractTransition overrides
    bool eventTest(QEvent *e) override { return e->type() == GuidedModeCancelledEvent::type(); }
    void onTransition(QEvent *) override { }
};