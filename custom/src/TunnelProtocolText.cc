#include "TunnelProtocolText.h"

#include <cstring>

#include "TunnelProtocol.h"

using namespace TunnelProtocol;

// Stringizes the protocol constant itself so log text always matches TunnelProtocol.h
#define TUNNEL_NAME_CASE(constant) \
    case constant:                 \
        return QStringLiteral(#constant)
#define TUNNEL_COMMAND_CASE(name) \
    case COMMAND_ID_##name:       \
        return QStringLiteral(#name)

namespace {

QString _unknownName(const char* prefix, uint32_t value)
{
    return QStringLiteral("%1UNKNOWN(%2)").arg(QLatin1String(prefix)).arg(value);
}

QString _collectionFinishName(uint32_t disposition)
{
    switch (disposition) {
        TUNNEL_NAME_CASE(COLLECTION_FINISH_FINALIZE);
        TUNNEL_NAME_CASE(COLLECTION_FINISH_CANCEL);
    }
    return _unknownName("COLLECTION_FINISH_", disposition);
}

QString _operationStateName(uint32_t state)
{
    switch (state) {
        TUNNEL_NAME_CASE(OPERATION_STATE_RUNNING);
        TUNNEL_NAME_CASE(OPERATION_STATE_COMPLETE);
        TUNNEL_NAME_CASE(OPERATION_STATE_FAILED);
    }
    return _unknownName("OPERATION_STATE_", state);
}

QString _commandResultName(uint32_t result)
{
    switch (result) {
        TUNNEL_NAME_CASE(COMMAND_RESULT_SUCCESS);
        TUNNEL_NAME_CASE(COMMAND_RESULT_FAILURE);
    }
    return _unknownName("COMMAND_RESULT_", result);
}

QString _detectionModeName(uint32_t mode)
{
    switch (mode) {
        TUNNEL_NAME_CASE(DETECTION_MODE_UAVRT);
        TUNNEL_NAME_CASE(DETECTION_MODE_PYTHON);
    }
    return _unknownName("DETECTION_MODE_", mode);
}

QString _logLevelName(uint32_t level)
{
    switch (level) {
        TUNNEL_NAME_CASE(LOG_LEVEL_DEBUG);
        TUNNEL_NAME_CASE(LOG_LEVEL_VERBOSE);
    }
    return _unknownName("LOG_LEVEL_", level);
}

QString _antennaIdName(uint32_t antennaId)
{
    switch (antennaId) {
        TUNNEL_NAME_CASE(ANTENNA_ID_RA2A);
        TUNNEL_NAME_CASE(ANTENNA_ID_RA23K);
    }
    return _unknownName("ANTENNA_ID_", antennaId);
}

QString _heartbeatSystemIdName(uint32_t systemId)
{
    switch (systemId) {
        TUNNEL_NAME_CASE(HEARTBEAT_SYSTEM_ID_MAVLINKCONTROLLER);
        TUNNEL_NAME_CASE(HEARTBEAT_SYSTEM_ID_CHANNELIZER);
    }
    return _unknownName("HEARTBEAT_SYSTEM_ID_", systemId);
}

QString _heartbeatStatusName(uint32_t status)
{
    switch (status) {
        TUNNEL_NAME_CASE(HEARTBEAT_STATUS_IDLE);
        TUNNEL_NAME_CASE(HEARTBEAT_STATUS_RECEIVING_TAGS);
        TUNNEL_NAME_CASE(HEARTBEAT_STATUS_HAS_TAGS);
        TUNNEL_NAME_CASE(HEARTBEAT_STATUS_DETECTING);
        TUNNEL_NAME_CASE(HEARTBEAT_STATUS_CAPTURE);
    }
    return _unknownName("HEARTBEAT_STATUS_", status);
}

QString _detectionStatusName(uint8_t status)
{
    switch (status) {
        TUNNEL_NAME_CASE(kSubthresholdDetectionStatus);
        TUNNEL_NAME_CASE(kSuperthresholdDetectionStatus);
        TUNNEL_NAME_CASE(kConfirmedDetectionStatus);
        TUNNEL_NAME_CASE(kNoPulseDetectionStatus);
    }
    return _unknownName("DetectionStatus_", status);
}

QString _rateStateName(uint8_t rateState)
{
    switch (rateState) {
        TUNNEL_NAME_CASE(kRateStateA);
        TUNNEL_NAME_CASE(kRateStateB);
        TUNNEL_NAME_CASE(kRateStateAToB);
        TUNNEL_NAME_CASE(kRateStateBToA);
    }
    return _unknownName("RateState_", rateState);
}

class FieldWriter
{
public:
    explicit FieldWriter(const QString& name)
        : _text(name)
    {}

    FieldWriter& add(const char* key, const QString& value)
    {
        _text += QStringLiteral(" %1: %2").arg(QLatin1String(key), value);
        return *this;
    }

    FieldWriter& add(const char* key, uint32_t value) { return add(key, QString::number(value)); }

    FieldWriter& add(const char* key, uint16_t value) { return add(key, QString::number(value)); }

    FieldWriter& add(const char* key, uint8_t value) { return add(key, QString::number(value)); }

    FieldWriter& add(const char* key, double value) { return add(key, QString::number(value, 'g', 10)); }

    FieldWriter& add(const char* key, float value) { return add(key, QString::number(value, 'g', 7)); }

    FieldWriter& addText(const char* key, const char* text, size_t maxLength)
    {
        const QString value = QString::fromUtf8(text, static_cast<qsizetype>(strnlen(text, maxLength)));
        if (!value.isEmpty()) {
            add(key, QStringLiteral("\"%1\"").arg(value));
        }
        return *this;
    }

    const QString& text() const { return _text; }

private:
    QString _text;
};

template <typename T>
bool _decode(const uint8_t* payload, size_t length, T& message)
{
    if (length != sizeof(T)) {
        return false;
    }
    memcpy(&message, payload, sizeof(T));
    return true;
}

}  // namespace

namespace TunnelProtocolText {

QString commandName(uint32_t command)
{
    switch (command) {
        TUNNEL_COMMAND_CASE(ACK);
        TUNNEL_COMMAND_CASE(START_TAGS);
        TUNNEL_COMMAND_CASE(END_TAGS);
        TUNNEL_COMMAND_CASE(TAG);
        TUNNEL_COMMAND_CASE(START_DETECTION);
        TUNNEL_COMMAND_CASE(STOP_DETECTION);
        TUNNEL_COMMAND_CASE(PULSE);
        TUNNEL_COMMAND_CASE(RAW_CAPTURE);
        TUNNEL_COMMAND_CASE(HEARTBEAT);
        TUNNEL_COMMAND_CASE(START_ROTATION);
        TUNNEL_COMMAND_CASE(STOP_ROTATION);
        TUNNEL_COMMAND_CASE(SAVE_LOGS);
        TUNNEL_COMMAND_CASE(CLEAN_LOGS);
        TUNNEL_COMMAND_CASE(AIRSPY_STATUS);
        TUNNEL_COMMAND_CASE(START_COLLECTION);
        TUNNEL_COMMAND_CASE(START_COLLECTION_SLICE);
        TUNNEL_COMMAND_CASE(FINISH_COLLECTION);
        TUNNEL_COMMAND_CASE(BEARING_RESULT);
        TUNNEL_COMMAND_CASE(COLLECTION_STATUS);
        TUNNEL_COMMAND_CASE(PYTHON_PULSE);
        TUNNEL_COMMAND_CASE(OPERATION_PROGRESS);
        TUNNEL_COMMAND_CASE(SET_LOG_LEVEL);
        TUNNEL_COMMAND_CASE(DETECTOR_HEARTBEAT);
    }
    return _unknownName("COMMAND_ID_", command);
}

QString collectionStatusName(uint32_t status)
{
    switch (status) {
        TUNNEL_NAME_CASE(COLLECTION_STATUS_SLICE_ARMED);
        TUNNEL_NAME_CASE(COLLECTION_STATUS_SLICE_COMPLETE);
        TUNNEL_NAME_CASE(COLLECTION_STATUS_FAILED);
        TUNNEL_NAME_CASE(COLLECTION_STATUS_STOPPED);
        TUNNEL_NAME_CASE(COLLECTION_STATUS_REVISIT_REQUESTED);
    }
    return _unknownName("COLLECTION_STATUS_", status);
}

QString collectionErrorName(uint32_t errorCode)
{
    switch (errorCode) {
        TUNNEL_NAME_CASE(COLLECTION_ERROR_PROCESS_FAILED);
        TUNNEL_NAME_CASE(COLLECTION_ERROR_UNEXPECTED_EXCEPTION);
        TUNNEL_NAME_CASE(COLLECTION_ERROR_REPORT_SEND_FAILED);
        TUNNEL_NAME_CASE(COLLECTION_ERROR_LOG_OPEN_FAILED);
    }
    return _unknownName("COLLECTION_ERROR_", errorCode);
}

QString formatMessage(Direction direction, const uint8_t* payload, size_t length)
{
    const QLatin1String directionText(direction == Direction::Received ? "received" : "sent");

    HeaderInfo_t header{};
    if (length < sizeof(header)) {
        return QStringLiteral("Tunnel message %1: payload_length: %2 expected at least: %3")
            .arg(directionText)
            .arg(length)
            .arg(sizeof(header));
    }
    memcpy(&header, payload, sizeof(header));

    FieldWriter writer(QStringLiteral("%1 %2:").arg(commandName(header.command), directionText));
    // Controller-originated messages carry no request_id
    if (header.request_id != 0) {
        writer.add("request_id", header.request_id);
    }

    const auto sizeMismatch = [&writer, length](size_t expected) {
        return writer.add("payload_length", static_cast<uint32_t>(length))
            .add("expected", static_cast<uint32_t>(expected))
            .text();
    };

    switch (header.command) {
        case COMMAND_ID_ACK: {
            AckInfo_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            writer.add("command", commandName(m.command))
                .add("request_id", m.request_id)
                .add("result", _commandResultName(m.result))
                .addText("message", m.message, sizeof(m.message));
            break;
        }
        case COMMAND_ID_START_TAGS: {
            StartTagsInfo_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            writer.add("upload_id", m.upload_id).add("tag_count", m.tag_count);
            break;
        }
        case COMMAND_ID_END_TAGS: {
            EndTagsInfo_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            writer.add("upload_id", m.upload_id).add("tag_count", m.tag_count);
            break;
        }
        case COMMAND_ID_TAG: {
            TagInfo_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            writer.add("upload_id", m.upload_id)
                .add("tag_index", m.tag_index)
                .add("id", m.id)
                .add("frequency_hz", m.frequency_hz)
                .add("pulse_width_msecs", m.pulse_width_msecs)
                .add("intra_pulse1_msecs", m.intra_pulse1_msecs)
                .add("intra_pulse2_msecs", m.intra_pulse2_msecs)
                .add("intra_pulse_uncertainty_msecs", m.intra_pulse_uncertainty_msecs)
                .add("intra_pulse_jitter_msecs", m.intra_pulse_jitter_msecs)
                .add("k", m.k)
                .add("false_alarm_probability", m.false_alarm_probability)
                .add("channelizer_channel_number", m.channelizer_channel_number)
                .add("channelizer_channel_center_frequency_hz", m.channelizer_channel_center_frequency_hz)
                .add("ip1_mu", m.ip1_mu)
                .add("ip1_sigma", m.ip1_sigma)
                .add("ip2_mu", m.ip2_mu)
                .add("ip2_sigma", m.ip2_sigma);
            break;
        }
        case COMMAND_ID_START_DETECTION: {
            StartDetectionInfo_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            writer.add("radio_center_frequency_hz", m.radio_center_frequency_hz)
                .add("gain", m.gain)
                .add("detection_mode", _detectionModeName(m.detection_mode))
                .add("detection_margin", m.detection_margin)
                .add("confidence_ratio", m.confidence_ratio)
                .add("debug_detector", m.debug_detector)
                .add("dump_spectrogram", m.dump_spectrogram);
            break;
        }
        case COMMAND_ID_STOP_DETECTION: {
            StopDetectionInfo_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            break;
        }
        case COMMAND_ID_SAVE_LOGS: {
            SaveLogsInfo_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            break;
        }
        case COMMAND_ID_CLEAN_LOGS: {
            CleanLogsInfo_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            break;
        }
        case COMMAND_ID_AIRSPY_STATUS: {
            AirspyStatusInfo_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            break;
        }
        case COMMAND_ID_RAW_CAPTURE: {
            RawCaptureInfo_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            writer.add("gain", m.gain).add("frequency_hz", m.frequency_hz);
            break;
        }
        case COMMAND_ID_HEARTBEAT: {
            Heartbeat_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            writer.add("protocol_version", m.protocol_version)
                .add("system_id", _heartbeatSystemIdName(m.system_id))
                .add("status", _heartbeatStatusName(m.status))
                .add("cpu_temp_c", m.cpu_temp_c);
            break;
        }
        case COMMAND_ID_START_COLLECTION: {
            StartCollection_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            writer.add("collection_id", m.collection_id)
                .add("radio_center_frequency_hz", m.radio_center_frequency_hz)
                .add("n_slices", m.n_slices)
                .add("detection_margin", m.detection_margin)
                .add("confidence_ratio", m.confidence_ratio)
                .add("debug_detector", m.debug_detector)
                .add("dump_spectrogram", m.dump_spectrogram)
                .add("antenna_id", _antennaIdName(m.antenna_id));
            break;
        }
        case COMMAND_ID_START_COLLECTION_SLICE: {
            StartCollectionSlice_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            writer.add("collection_id", m.collection_id).add("slice_id", m.slice_id).add("heading_deg", m.heading_deg);
            break;
        }
        case COMMAND_ID_FINISH_COLLECTION: {
            FinishCollection_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            writer.add("collection_id", m.collection_id).add("disposition", _collectionFinishName(m.disposition));
            break;
        }
        case COMMAND_ID_COLLECTION_STATUS: {
            CollectionStatus_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            writer.add("collection_id", m.collection_id)
                .add("slice_id", m.slice_id)
                .add("status", collectionStatusName(m.status))
                .add("expected_detectors", m.expected_detectors)
                .add("completed_detectors", m.completed_detectors);
            if (m.status == COLLECTION_STATUS_FAILED) {
                writer.add("error_code", collectionErrorName(m.error_code));
            } else if (m.status == COLLECTION_STATUS_REVISIT_REQUESTED) {
                writer.add("revisit_heading_deg", m.revisit_heading_deg);
            }
            break;
        }
        case COMMAND_ID_BEARING_RESULT: {
            BearingResult_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            writer.add("collection_id", m.collection_id)
                .add("tag_id", m.tag_id)
                .add("bearing_deg", m.bearing_deg)
                .add("r_squared", m.r_squared)
                .add("n_valid_slices", m.n_valid_slices)
                .add("best_snr", m.best_snr)
                .add("confirmed", m.confirmed);
            break;
        }
        case COMMAND_ID_PULSE: {
            PulseInfo_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            writer.add("tag_id", m.tag_id)
                .add("frequency_hz", m.frequency_hz)
                .add("start_time_seconds", m.start_time_seconds)
                .add("predict_next_start_seconds", m.predict_next_start_seconds)
                .add("snr", m.snr)
                .add("stft_score", m.stft_score)
                .add("group_seq_counter", m.group_seq_counter)
                .add("group_ind", m.group_ind)
                .add("group_snr", m.group_snr)
                .add("noise_psd", m.noise_psd)
                .add("detection_status", _detectionStatusName(m.detection_status))
                .add("confirmed_status", m.confirmed_status)
                .add("latitude", m.latitude)
                .add("longitude", m.longitude)
                .add("altitude_rel", m.altitude_rel)
                .add("roll_deg", m.roll_deg)
                .add("pitch_deg", m.pitch_deg)
                .add("yaw_deg", m.yaw_deg);
            break;
        }
        case COMMAND_ID_PYTHON_PULSE: {
            PythonPulseInfo_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            writer.add("collection_id", m.collection_id)
                .add("slice_id", m.slice_id)
                .add("tag_id", m.tag_id)
                .add("frequency_hz", m.frequency_hz)
                .add("cycle_counter", m.cycle_counter)
                .add("start_time_seconds", m.start_time_seconds)
                .add("predict_next_start_seconds", m.predict_next_start_seconds)
                .add("snr", m.snr)
                .add("score_ratio", m.score_ratio)
                .add("signal_psd", m.signal_psd)
                .add("noise_psd", m.noise_psd)
                .add("detection_status", _detectionStatusName(m.detection_status))
                .add("confirmed_status", m.confirmed_status)
                .add("rate_state", _rateStateName(m.rate_state))
                .add("candidate_id", m.candidate_id)
                .add("latitude", m.latitude)
                .add("longitude", m.longitude)
                .add("altitude_rel", m.altitude_rel)
                .add("roll_deg", m.roll_deg)
                .add("pitch_deg", m.pitch_deg)
                .add("yaw_deg", m.yaw_deg);
            break;
        }
        case COMMAND_ID_OPERATION_PROGRESS: {
            OperationProgress_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            writer.add("command", commandName(m.command))
                .add("request_id", m.request_id)
                .add("state", _operationStateName(m.state))
                .add("step", m.step)
                .add("step_count", m.step_count)
                .addText("message", m.message, sizeof(m.message));
            break;
        }
        case COMMAND_ID_SET_LOG_LEVEL: {
            SetLogLevel_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            writer.add("level", _logLevelName(m.level));
            break;
        }
        case COMMAND_ID_DETECTOR_HEARTBEAT: {
            DetectorHeartbeat_t m;
            if (!_decode(payload, length, m)) {
                return sizeMismatch(sizeof(m));
            }
            writer.add("tag_id", m.tag_id).add("detection_mode", _detectionModeName(m.detection_mode));
            break;
        }
        default:
            writer.add("payload_length", static_cast<uint32_t>(length));
            break;
    }

    return writer.text();
}

}  // namespace TunnelProtocolText

#undef TUNNEL_NAME_CASE
#undef TUNNEL_COMMAND_CASE
