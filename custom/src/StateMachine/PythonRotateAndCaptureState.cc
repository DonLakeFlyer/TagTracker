#include "PythonRotateAndCaptureState.h"
#include "PythonCaptureAtSliceState.h"
#include "PythonWaitForFinishOutcomeState.h"
#include "SendTunnelCommandState.h"
#include "FunctionState.h"
#include "SayState.h"
#include "CustomPlugin.h"
#include "CustomSettings.h"
#include "CustomLoggingCategory.h"
#include "DetectorList.h"
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
    , _rotationDivisions(_customSettings->divisions()->rawValue().toInt())
{
    const int rotationDivisions     = _rotationDivisions;
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
    startCollection.antenna_id                = _customSettings->antennaModel()->rawValue().toUInt();

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
    auto finishOutcomeState             = new PythonWaitForFinishOutcomeState(this, _collectionId, /*allowRevisit*/ true);
    // Revisit branch: the slice itself is built once the controller names the heading
    _revisitState                       = new QState(this);
    _revisitDone                        = new QFinalState(_revisitState);
    auto announceRevisitState           = new SayState("Announce Revisit", this, "Confirming bearing");
    auto finishAfterRevisitState        = new SendTunnelCommandState("FinishCollection after revisit", this, reinterpret_cast<uint8_t*>(&finishCollection), sizeof(finishCollection), kFinishCollectionAckTimeoutMs);
    auto finishAfterRevisitOutcomeState = new PythonWaitForFinishOutcomeState(this, _collectionId, /*allowRevisit*/ false);
    auto rotationEndState               = new FunctionState("Rotation End",   this, std::bind(&PythonRotateAndCaptureState::_rotationEnd, this));
    auto announceRotateCompleteState    = new SayState("Announce Rotate Complete", this, "Rotation detection complete");
    auto finalState                     = new QFinalState(this);

    // Build the per-slice states
    QList<PythonCaptureAtSliceState*> sliceStates;
    const double priorBearingDeg = _customPlugin->hasPriorBearing() ? _customPlugin->priorBearingDeg() : qQNaN();
    const QList<int> sliceOrder = sliceVisitOrder(rotationDivisions, priorBearingDeg, _customSettings->antennaOffset()->rawValue().toDouble());
    for (int sequenceIndex = 0; sequenceIndex < sliceOrder.count(); ++sequenceIndex) {
        sliceStates.append(new PythonCaptureAtSliceState(
            this, sliceOrder[sequenceIndex], sequenceIndex, _collectionId));
    }

    // Transitions: rotationBegin → startCollection → slice[0] → ... → slice[N-1] → finishCollection
    //   → finishOutcome → (bearing) rotationEnd
    //                   → (revisit) announce → revisit slice → finishCollection again → finishOutcome → rotationEnd
    rotationBeginState->addTransition(rotationBeginState, &FunctionState::advance, startCollectionState);
    startCollectionState->addTransition(startCollectionState, &SendTunnelCommandState::commandSucceeded, sliceStates.first());

    for (int i = 0; i < sliceStates.count() - 1; i++) {
        sliceStates[i]->addTransition(sliceStates[i], &QState::finished, sliceStates[i + 1]);
    }
    sliceStates.last()->addTransition(sliceStates.last(), &QState::finished, finishCollectionState);

    finishCollectionState->addTransition(finishCollectionState, &SendTunnelCommandState::commandSucceeded, finishOutcomeState);
    // Direct connection: the revisit slice must exist before the machine processes the transition into it.
    connect(finishOutcomeState, &PythonWaitForFinishOutcomeState::revisitRequested,
            this, &PythonRotateAndCaptureState::_buildRevisitSlice, Qt::DirectConnection);
    finishOutcomeState->addTransition(finishOutcomeState, &PythonWaitForFinishOutcomeState::bearingReceived, rotationEndState);
    finishOutcomeState->addTransition(finishOutcomeState, &PythonWaitForFinishOutcomeState::revisitRequested, announceRevisitState);
    announceRevisitState->addTransition(announceRevisitState, &SayState::advance, _revisitState);
    connect(_revisitState, &QState::entered, this, [this] () {
        if (!_revisitState->initialState()) {
            setError(QStringLiteral("Revisit slice was never built for collection %1").arg(_collectionId));
        }
    });
    _revisitState->addTransition(_revisitState, &QState::finished, finishAfterRevisitState);
    finishAfterRevisitState->addTransition(finishAfterRevisitState, &SendTunnelCommandState::commandSucceeded, finishAfterRevisitOutcomeState);
    finishAfterRevisitOutcomeState->addTransition(finishAfterRevisitOutcomeState, &PythonWaitForFinishOutcomeState::bearingReceived, rotationEndState);
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

QList<int> PythonRotateAndCaptureState::sliceVisitOrder(int rotationDivisions, double priorBearingDeg, double antennaOffsetDeg)
{
    QList<int> sliceOrder;
    if (rotationDivisions <= 0) {
        return sliceOrder;
    }

    if (std::isfinite(priorBearingDeg)) {
        const double degreesPerSlice = 360.0 / rotationDivisions;
        double bearingDeg = std::fmod(priorBearingDeg + antennaOffsetDeg, 360.0);
        if (bearingDeg < 0) {
            bearingDeg += 360.0;
        }
        const int firstSlice = static_cast<int>(std::lround(bearingDeg / degreesPerSlice)) % rotationDivisions;
        sliceOrder.append(firstSlice);
        for (int distance = 1; sliceOrder.count() < rotationDivisions; ++distance) {
            const int clockwise = (firstSlice + distance) % rotationDivisions;
            const int counterClockwise = (firstSlice - distance + rotationDivisions) % rotationDivisions;
            if (!sliceOrder.contains(clockwise)) {
                sliceOrder.append(clockwise);
            }
            if (sliceOrder.count() < rotationDivisions && !sliceOrder.contains(counterClockwise)) {
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
    return sliceOrder;
}

void PythonRotateAndCaptureState::_collectionStatusReceived(uint32_t collectionId, uint32_t sliceId, uint32_t status, uint32_t errorCode)
{
    if (collectionId != _collectionId || status != COLLECTION_STATUS_FAILED) {
        return;
    }

    QString errorDescription;
    switch (errorCode) {
    case COLLECTION_ERROR_PROCESS_FAILED:
        errorDescription = QStringLiteral("detector process exited");
        break;
    case COLLECTION_ERROR_UNEXPECTED_EXCEPTION:
        errorDescription = QStringLiteral("unexpected exception; see the py_detector log");
        break;
    case COLLECTION_ERROR_REPORT_SEND_FAILED:
        errorDescription = QStringLiteral("failed to send a detector report");
        break;
    case COLLECTION_ERROR_LOG_OPEN_FAILED:
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

void PythonRotateAndCaptureState::_buildRevisitSlice(float headingDeg)
{
    if (_revisitState->initialState() != nullptr) {
        return;
    }
    // slice_id continues the visit sequence so the controller keys it like any other heading
    auto revisitSlice = new PythonCaptureAtSliceState(
        _revisitState, headingDeg, static_cast<uint32_t>(_rotationDivisions + 1), _collectionId,
        PythonCaptureAtSliceState::ExplicitYaw {});
    revisitSlice->addTransition(revisitSlice, &QState::finished, _revisitDone);
    _revisitState->setInitialState(revisitSlice);
    qCDebug(CustomStateMachineLog) << "Python: revisit slice built for heading" << headingDeg;
}

void PythonRotateAndCaptureState::_rotationBegin()
{
    _customPlugin->rotationIsStarting(_collectionId);
    DetectorList::instance()->startHeartbeatWatchdogs();
    qCDebug(CustomStateMachineLog) << "Python rotation begin" << " - " << Q_FUNC_INFO;
}

void PythonRotateAndCaptureState::_rotationEnd()
{
    _customPlugin->rotationIsEnding();
}
