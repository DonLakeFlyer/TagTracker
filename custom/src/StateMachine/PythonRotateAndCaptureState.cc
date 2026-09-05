#include "PythonRotateAndCaptureState.h"
#include "PythonCaptureAtSliceState.h"
#include "SendTunnelCommandState.h"
#include "FunctionState.h"
#include "SayState.h"
#include "CustomPlugin.h"
#include "CustomSettings.h"
#include "CustomLoggingCategory.h"
#include "TagDatabase.h"
#include "TunnelProtocol.h"

#include <QFinalState>
#include <QRandomGenerator>
#include <cmath>

using namespace TunnelProtocol;

PythonRotateAndCaptureState::PythonRotateAndCaptureState(QState* parentState)
    : CustomState       ("PythonRotateAndCaptureState", parentState)
    , _customPlugin     (qobject_cast<CustomPlugin*>(CustomPlugin::instance()))
    , _customSettings   (_customPlugin->customSettings())
{
    const int rotationDivisions     = _customSettings->divisions()->rawValue().toInt();
    do {
        _collectionId = QRandomGenerator::global()->generate();
    } while (_collectionId == 0);

    StartCollection_t startCollection {};
    startCollection.header.command            = COMMAND_ID_START_COLLECTION;
    startCollection.collection_id              = _collectionId;
    startCollection.radio_center_frequency_hz = TagDatabase::instance()->channelizerTuner();
    startCollection.n_slices                  = rotationDivisions;
    startCollection.detection_margin          = _customSettings->detectionMargin()->rawValue().toDouble();
    startCollection.confidence_ratio          = _customSettings->confidenceRatio()->rawValue().toDouble();
    startCollection.debug_detector            = _customSettings->debugDetector()->rawValue().toBool() ? 1 : 0;

    FinishCollection_t finishCollection {};
    finishCollection.header.command = COMMAND_ID_FINISH_COLLECTION;
    finishCollection.collection_id = _collectionId;
    finishCollection.disposition = COLLECTION_FINISH_FINALIZE;

    // Controller blocks up to 30 s waiting for detectors to start before acking START_COLLECTION
    constexpr int kStartCollectionAckTimeoutMs = 35000;
    // Controller tears down detector processes (up to 15 s) before acking FINISH_COLLECTION
    constexpr int kFinishCollectionAckTimeoutMs = 20000;

    auto rotationBeginState             = new FunctionState("Rotation Begin", this, std::bind(&PythonRotateAndCaptureState::_rotationBegin, this));
    auto startCollectionState           = new SendTunnelCommandState("StartCollection", this, reinterpret_cast<uint8_t*>(&startCollection), sizeof(startCollection), kStartCollectionAckTimeoutMs);
    auto finishCollectionState          = new SendTunnelCommandState("FinishCollection", this, reinterpret_cast<uint8_t*>(&finishCollection), sizeof(finishCollection), kFinishCollectionAckTimeoutMs);
    auto rotationEndState               = new FunctionState("Rotation End",   this, std::bind(&PythonRotateAndCaptureState::_rotationEnd, this));
    auto announceRotateCompleteState    = new SayState("Announce Rotate Complete", this, "Rotation detection complete");
    auto finalState                     = new QFinalState(this);

    // Build the per-slice states
    QList<PythonCaptureAtSliceState*> sliceStates;
    QList<int> sliceOrder;
    if (_customPlugin->hasPriorBearing()) {
        const double degreesPerSlice = 360.0 / rotationDivisions;
        const double antennaOffset = _customSettings->antennaOffset()->rawValue().toDouble();
        const int firstSlice = static_cast<int>(std::lround(
            (_customPlugin->priorBearingDeg() + antennaOffset) / degreesPerSlice))
            % rotationDivisions;
        sliceOrder.append(firstSlice);
        for (int distance = 1; sliceOrder.count() < rotationDivisions; ++distance) {
            const int clockwise = (firstSlice + distance) % rotationDivisions;
            const int counterClockwise =
                (firstSlice - distance + rotationDivisions) % rotationDivisions;
            if (!sliceOrder.contains(clockwise)) {
                sliceOrder.append(clockwise);
            }
            if (sliceOrder.count() < rotationDivisions
                && !sliceOrder.contains(counterClockwise)) {
                sliceOrder.append(counterClockwise);
            }
        }
    } else if (rotationDivisions == 8) {
        sliceOrder = {0, 2, 4, 6, 1, 3, 5, 7};
    } else {
        for (int i = 0; i < rotationDivisions; ++i) {
            sliceOrder.append(i);
        }
    }
    for (int sequenceIndex = 0; sequenceIndex < sliceOrder.count(); ++sequenceIndex) {
        sliceStates.append(new PythonCaptureAtSliceState(
            this, sliceOrder[sequenceIndex], sequenceIndex, _collectionId));
    }

    // Transitions: rotationBegin → startRotationDetection → slice[0] → ... → slice[N-1] → stopRotationDetection → rotationEnd
    rotationBeginState->addTransition(rotationBeginState, &FunctionState::advance, startCollectionState);
    startCollectionState->addTransition(startCollectionState, &SendTunnelCommandState::commandSucceeded, sliceStates.first());

    for (int i = 0; i < sliceStates.count() - 1; i++) {
        sliceStates[i]->addTransition(sliceStates[i], &QState::finished, sliceStates[i + 1]);
    }
    sliceStates.last()->addTransition(sliceStates.last(), &QState::finished, finishCollectionState);

    finishCollectionState->addTransition(finishCollectionState, &SendTunnelCommandState::commandSucceeded, rotationEndState);
    rotationEndState->addTransition(rotationEndState, &FunctionState::advance, announceRotateCompleteState);
    announceRotateCompleteState->addTransition(announceRotateCompleteState, &SayState::advance, finalState);

    // Detector failures can arrive between slices (slice_id 0), when no slice state is listening
    connect(this, &QState::entered, this, [this] () {
        connect(_customPlugin, &CustomPlugin::collectionStatusReceived,
                this, &PythonRotateAndCaptureState::_collectionStatusReceived);
    });
    connect(this, &QState::exited, this, [this] () {
        disconnect(_customPlugin, &CustomPlugin::collectionStatusReceived,
                   this, &PythonRotateAndCaptureState::_collectionStatusReceived);
    });

    setInitialState(rotationBeginState);
}

void PythonRotateAndCaptureState::_collectionStatusReceived(uint32_t collectionId, uint32_t sliceId, uint32_t status, uint32_t errorCode)
{
    if (collectionId != _collectionId || status != COLLECTION_STATUS_FAILED) {
        return;
    }

    QString errorDescription;
    switch (errorCode) {
    case 1:
        errorDescription = QStringLiteral("detector process exited");
        break;
    case 2:
        errorDescription = QStringLiteral("unexpected exception; see the py_detector log");
        break;
    case 3:
        errorDescription = QStringLiteral("failed to send a detector report");
        break;
    case 4:
        errorDescription = QStringLiteral("failed to open the detector log");
        break;
    default:
        errorDescription = QStringLiteral("unknown error");
        break;
    }
    setError(QStringLiteral("Python detector failed at slice %1: %2 (error %3)")
                 .arg(sliceId)
                 .arg(errorDescription)
                 .arg(errorCode));
}

void PythonRotateAndCaptureState::_rotationBegin()
{
    _customPlugin->rotationIsStarting(_collectionId);
    qCDebug(CustomStateMachineLog) << "Python rotation begin" << " - " << Q_FUNC_INFO;
}

void PythonRotateAndCaptureState::_rotationEnd()
{
    _customPlugin->rotationIsEnding();
}
