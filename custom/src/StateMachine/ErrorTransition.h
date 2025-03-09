#pragma once

#include <QEvent>
#include <QAbstractTransition>

class ErrorEvent : public QEvent
{
public:
    ErrorEvent(const QString& errorString)
        : QEvent        (type())
        , _errorString  (errorString)
    {}

    static Type type() { return QEvent::Type(QEvent::User + 2); }

private:
    QString _errorString;
};

class ErrorTransition : public QAbstractTransition
{
    Q_OBJECT

public:
    ErrorTransition(QState* parentState)
        : QAbstractTransition(parentState)
    {}

protected:
    // QAbstractTransition overrides
    bool eventTest(QEvent *e) override { return e->type() == ErrorEvent::type(); }
    void onTransition(QEvent *) override { }

private:
    QString m_value;
};