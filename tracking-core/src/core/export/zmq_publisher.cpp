// TRK-021: ZMQ publisher implementation. Schema authority lives in the header
// comment; keep the two in lockstep with src/viewer/schema.py.

#include "zmq_publisher.hpp"

#include <nlohmann/json.hpp>
#include <zmq.hpp>

#include <chrono>
#include <cmath>
#include <string>

#include "track.hpp"

namespace tracking {

namespace {

using nlohmann::json;

std::uint64_t wall_epoch_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

std::int64_t steady_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

const char* tracker_state_name(TrackerState state) {
    return state == TrackerState::Running ? "RUNNING" : "INITIALISING";
}

const char* track_state_name(std::uint8_t state) {
    switch (static_cast<TrackState>(state)) {
        case TrackState::Provisional: return "Provisional";
        case TrackState::Confirmed: return "Confirmed";
        case TrackState::Predicted: return "Predicted";
        case TrackState::Occluded: return "Occluded";
        case TrackState::Lost: return "Lost";
        case TrackState::Retired: return "Retired";
    }
    return "Unknown";
}

json reasons_json(std::uint16_t mask) {
    static constexpr const char* kNames[] = {
        "TrackerNotRunning",  "CalibrationInvalid", "LaserNotConfirmed",
        "BallNotConfirmed",   "AgeExceedsThreshold", "LaserInTransit",
        "LaserBallMisaligned"};
    json out = json::array();
    for (int bit = 0; bit < 7; ++bit) {
        if (mask & (1U << bit)) {
            out.push_back(kNames[bit]);
        }
    }
    return out;
}

// Defensive finite guard at the serialisation boundary: upstream sanitisation
// (plan R8) already invalidates non-finite slots, but a NaN reaching nlohmann
// would serialise as `null` and fail every pydantic consumer silently.
double finite_or_zero(double v) { return std::isfinite(v) ? v : 0.0; }

json object_json(const ObjectSlot& slot, const SafetyResult& safety) {
    return {
        {"object_type",
         static_cast<ObjectType>(slot.object_type) == ObjectType::Laser ? "laser"
                                                                        : "ball"},
        {"track_id", slot.track_id},
        {"track_state", track_state_name(slot.track_state)},
        {"position",
         {{"coordinate_space", "Plane2D_World"},
          {"x_m", finite_or_zero(slot.floor_x_m)},
          {"y_m", finite_or_zero(slot.floor_y_m)},
          {"z_m", finite_or_zero(slot.floor_z_m)},
          {"uncertainty_m", finite_or_zero(slot.uncertainty_m)}}},
        {"pixel",
         {{"x_px", finite_or_zero(static_cast<double>(slot.pixel_x_px))},
          {"y_px", finite_or_zero(static_cast<double>(slot.pixel_y_px))}}},
        {"age_ms", finite_or_zero(slot.age_ms)},
        {"speed_m_per_s", finite_or_zero(slot.speed_m_per_s)},
        {"speed_valid", slot.speed_valid},
        {"safe_for_control", safety.safe},  // global predicate (ADR-002)
        {"unsafe_reasons", reasons_json(safety.unsafe_reasons)},
    };
}

}  // namespace

struct ZmqPublisher::Impl {
    zmq::context_t context{1};
    zmq::socket_t socket{context, zmq::socket_type::pub};
    SafeForControlConfig safety_config;
    double ball_radius_m = 0.0;
    std::uint64_t session_id = 0;
    std::uint64_t message_id = 0;
    std::string buffer;  // pre-sized serialisation buffer (TRK-021)
};

ZmqPublisher::ZmqPublisher(const ZmqConfig& zmq_config,
                           const SafeForControlConfig& safety_config,
                           double ball_radius_m)
    : impl_(std::make_unique<Impl>()) {
    impl_->safety_config = safety_config;
    impl_->ball_radius_m = ball_radius_m;
    impl_->session_id = wall_epoch_ms();
    impl_->buffer.reserve(4096);  // comfortably above the worst-case message
    impl_->socket.set(zmq::sockopt::sndhwm, 1);    // ADR-002
    impl_->socket.set(zmq::sockopt::conflate, 1);  // ADR-002: last message wins
    impl_->socket.set(zmq::sockopt::linger, 0);    // ADR-002: no shutdown block
    impl_->socket.bind(zmq_config.bind_address);
}

ZmqPublisher::~ZmqPublisher() = default;

void ZmqPublisher::publish(const TrackingSnapshot& snapshot, double cpu_temp_c) {
    // Per-message steady->wall offset (plan KTD-5): both wire timestamps come
    // from one sampling so they share a single mapping.
    const std::uint64_t wall_now_ms = wall_epoch_ms();
    const std::int64_t steady_now = steady_ns();
    const std::int64_t capture_offset_ms =
        (steady_now - snapshot.frame_capture_timestamp_ns) / 1'000'000;
    const std::uint64_t frame_capture_ms =
        capture_offset_ms >= 0 && wall_now_ms >= static_cast<std::uint64_t>(capture_offset_ms)
            ? wall_now_ms - static_cast<std::uint64_t>(capture_offset_ms)
            : wall_now_ms;

    json message = {
        {"schema_version", 1},
        {"message_id", impl_->message_id++},
        {"session_id", impl_->session_id},
        {"publish_timestamp_ms", wall_now_ms},
        {"frame_capture_timestamp_ms", frame_capture_ms},
        {"system_health",
         {{"tracker_state", tracker_state_name(snapshot.health.tracker_state)},
          {"calibration_status", "VALID"},
          {"ball_radius_m", impl_->ball_radius_m},
          {"cpu_temp_c", finite_or_zero(cpu_temp_c)},
          {"frames_processed_count", snapshot.health.frames_processed},
          {"frames_rejected_count", snapshot.health.frames_rejected}}},
        {"thresholds",
         {{"age_max_ms", impl_->safety_config.age_max_ms},
          {"laser_settled_speed_m_per_s", impl_->safety_config.laser_settled_speed_m_per_s},
          {"alignment_tolerance_m", impl_->safety_config.alignment_tolerance_m},
          {"min_unsafe_dwell_ms", impl_->safety_config.min_unsafe_dwell_ms}}},
        {"objects", json::array()},
        {"safety",
         {{"safe", snapshot.safety.safe},
          {"unsafe_reasons", reasons_json(snapshot.safety.unsafe_reasons)},
          {"hysteresis_remaining_ms", snapshot.safety.hysteresis_remaining_ms},
          {"distance_valid", snapshot.safety.distance_valid}}},
    };
    if (snapshot.safety.distance_valid) {
        message["safety"]["laser_ball_distance_m"] =
            finite_or_zero(snapshot.safety.laser_ball_distance_m);
    }
    if (snapshot.laser.valid) {
        message["objects"].push_back(object_json(snapshot.laser, snapshot.safety));
    }
    if (snapshot.ball.valid) {
        message["objects"].push_back(object_json(snapshot.ball, snapshot.safety));
    }

    impl_->buffer.clear();
    impl_->buffer = message.dump();
    // Single-part send — see the schema block in the header for why a topic
    // frame is forbidden (ZMQ_CONFLATE is incompatible with multipart).
    impl_->socket.send(zmq::buffer(impl_->buffer), zmq::send_flags::dontwait);
}

std::uint64_t ZmqPublisher::messages_published() const noexcept {
    return impl_->message_id;
}

std::uint64_t ZmqPublisher::session_id() const noexcept { return impl_->session_id; }

ZmqPublisher::SocketOptions ZmqPublisher::socket_options() const {
    SocketOptions options;
    options.sndhwm = impl_->socket.get(zmq::sockopt::sndhwm);
    options.conflate = impl_->socket.get(zmq::sockopt::conflate);
    options.linger = impl_->socket.get(zmq::sockopt::linger);
    return options;
}

}  // namespace tracking
