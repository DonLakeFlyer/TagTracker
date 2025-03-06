#include "CustomStateMachine.h"
#include "CustomLoggingCategory.h"

#include "QGCApplication.h"
#include "AudioOutput.h"

#include <QFinalState>

CustomStateMachine::CustomStateMachine(QObject* parent)
    : QStateMachine (parent)
    , _errorState   (new CustomState(this))
    , _finalState   (new QFinalState(this))
{
    connect(_errorState, &QState::entered, this, &CustomStateMachine::displayError);
    _errorState->addTransition(_errorState, &QState::entered, _finalState);
    connect(_finalState, &QState::entered, this, [this] () { this->deleteLater(); });
}

void CustomStateMachine::setErrorState(CustomState* errorState)
{
    if (_errorState) {
        disconnect(_errorState, &QState::entered, this, &CustomStateMachine::displayError);
    }
    _errorState = errorState;
    connect(_errorState, &QState::entered, this, &CustomStateMachine::displayError);
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
