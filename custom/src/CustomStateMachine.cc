#include "CustomStateMachine.h"
#include "CustomLoggingCategory.h"

#include "QGCApplication.h"
#include "AudioOutput.h"

#include <QFinalState>

void CustomState::setError(const QString& errorString) 
{
    qWarning(CustomPluginLog) << "CustomState::setError: errorString" << errorString;
    _errorString = errorString; 

    // Bubble the error up to the top level state machine
    auto customStateMachine = qobject_cast<CustomStateMachine*>(machine());
    if (customStateMachine) {
        customStateMachine->_errorString = errorString;
    }

    emit error(); 
}

FunctionState::FunctionState(std::function<void()> function, QState* parent)
    : CustomState   (parent)
    , _function     (function)
{
    connect(this, &QState::entered, this, [this] () { _function(); });
}

CustomStateMachine::CustomStateMachine(QObject* parent)
    : QStateMachine (parent)
    , _errorState   (new FunctionState(std::bind(&CustomStateMachine::displayError, this), this))
    , _finalState   (new QFinalState(this))
{
    _errorState->addTransition(_errorState, &QState::entered, _finalState);
    connect(_finalState, &QState::entered, this, [this] () { this->deleteLater(); });
}

void CustomStateMachine::setError(const QString& errorString)
{
    _errorString = errorString;
    emit error();
}

void CustomStateMachine::displayError()
{
    qDebug() << "CustomStateMachine::displayError: errorString" << _errorString;
    AudioOutput::instance()->say(_errorString);
    qgcApp()->showAppMessage(_errorString);
    qCWarning(CustomPluginLog) << _errorString;
    _errorString.clear();
}

