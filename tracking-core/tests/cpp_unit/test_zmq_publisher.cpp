// TRK-021 / plan U14: publisher schema and socket-contract tests. A local SUB
// socket round-trips one message and every schema field is checked; the
// single-part property is asserted (ZMQ_CONFLATE is incompatible with
// multipart, so a topic frame would be a contract break).

#include "zmq_publisher.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <zmq.hpp>

#include <chrono>
#include <string>
#include <thread>

#include "track.hpp"

namespace {

using nlohmann::json;

tracking::SafeForControlConfig safety_config() {
    tracking::SafeForControlConfig cfg;
    cfg.age_max_ms = 50.0;
    cfg.laser_settled_speed_m_per_s = 0.05;
    cfg.alignment_tolerance_m = 0.02;
    cfg.min_unsafe_dwell_ms = 200.0;
    return cfg;
}

tracking::ZmqConfig zmq_config(const char* endpoint) {
    tracking::ZmqConfig cfg;
    cfg.bind_address = endpoint;
    return cfg;
}

tracking::TrackingSnapshot sample_snapshot() {
    tracking::TrackingSnapshot snapshot;
    snapshot.frame_capture_timestamp_ns = 123'000'000;
    snapshot.health.tracker_state = tracking::TrackerState::Running;
    snapshot.health.frames_processed = 42;
    snapshot.health.frames_rejected = 3;
    snapshot.ball.valid = true;
    snapshot.ball.object_type = static_cast<std::uint8_t>(tracking::ObjectType::Ball);
    snapshot.ball.track_state = static_cast<std::uint8_t>(tracking::TrackState::Confirmed);
    snapshot.ball.track_id = 7;
    snapshot.ball.floor_x_m = 0.1;
    snapshot.ball.floor_y_m = 2.0;
    snapshot.ball.floor_z_m = 0.03;
    snapshot.ball.uncertainty_m = 0.004;
    snapshot.ball.age_ms = 12.5;
    snapshot.ball.speed_m_per_s = 0.2;
    snapshot.ball.speed_valid = true;
    snapshot.safety.safe = false;
    snapshot.safety.unsafe_reasons =
        tracking::kLaserNotConfirmed | tracking::kLaserInTransit;
    snapshot.safety.hysteresis_remaining_ms = 200;
    return snapshot;
}

// Receives one message with a timeout; fails the test on silence.
bool receive_json(zmq::socket_t& sub, json& out, bool& was_multipart) {
    zmq::pollitem_t item{sub.handle(), 0, ZMQ_POLLIN, 0};
    if (zmq::poll(&item, 1, std::chrono::milliseconds(2000)) == 0) {
        return false;
    }
    zmq::message_t message;
    if (!sub.recv(message, zmq::recv_flags::none).has_value()) {
        return false;
    }
    was_multipart = message.more();
    out = json::parse(
        std::string(static_cast<const char*>(message.data()), message.size()));
    return true;
}

TEST(ZmqPublisherTest, SchemaRoundTripAndSocketContract) {
    tracking::ZmqPublisher publisher(zmq_config("tcp://127.0.0.1:25547"),
                                     safety_config(), 0.03);

    const tracking::ZmqPublisher::SocketOptions options = publisher.socket_options();
    EXPECT_EQ(options.sndhwm, 1);    // ADR-002
    EXPECT_EQ(options.conflate, 1);  // ADR-002
    EXPECT_EQ(options.linger, 0);    // ADR-002

    zmq::context_t context(1);
    zmq::socket_t sub(context, zmq::socket_type::sub);
    sub.set(zmq::sockopt::subscribe, "");  // empty filter: single-part schema
    sub.connect("tcp://127.0.0.1:25547");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));  // slow joiner

    publisher.publish(sample_snapshot(), 55.5);

    json message;
    bool multipart = true;
    ASSERT_TRUE(receive_json(sub, message, multipart)) << "no message within 2 s";
    EXPECT_FALSE(multipart) << "schema is single-part; a topic frame breaks CONFLATE";

    EXPECT_EQ(message["schema_version"], 1);
    EXPECT_EQ(message["message_id"], 0);
    EXPECT_EQ(message["session_id"], publisher.session_id());
    EXPECT_GT(message["publish_timestamp_ms"].get<std::uint64_t>(), 0U);
    EXPECT_LE(message["frame_capture_timestamp_ms"].get<std::uint64_t>(),
              message["publish_timestamp_ms"].get<std::uint64_t>());

    const json& health = message["system_health"];
    EXPECT_EQ(health["tracker_state"], "RUNNING");
    EXPECT_EQ(health["calibration_status"], "VALID");
    EXPECT_DOUBLE_EQ(health["ball_radius_m"].get<double>(), 0.03);
    EXPECT_DOUBLE_EQ(health["cpu_temp_c"].get<double>(), 55.5);
    EXPECT_EQ(health["frames_processed_count"], 42);
    EXPECT_EQ(health["frames_rejected_count"], 3);

    const json& thresholds = message["thresholds"];
    EXPECT_DOUBLE_EQ(thresholds["age_max_ms"].get<double>(), 50.0);
    EXPECT_DOUBLE_EQ(thresholds["laser_settled_speed_m_per_s"].get<double>(), 0.05);
    EXPECT_DOUBLE_EQ(thresholds["alignment_tolerance_m"].get<double>(), 0.02);
    EXPECT_DOUBLE_EQ(thresholds["min_unsafe_dwell_ms"].get<double>(), 200.0);

    ASSERT_EQ(message["objects"].size(), 1U);  // invalid laser slot omitted
    const json& ball = message["objects"][0];
    EXPECT_EQ(ball["object_type"], "ball");
    EXPECT_EQ(ball["track_id"], 7);
    EXPECT_EQ(ball["track_state"], "Confirmed");
    EXPECT_EQ(ball["position"]["coordinate_space"], "Plane2D_World");
    EXPECT_DOUBLE_EQ(ball["position"]["x_m"].get<double>(), 0.1);
    EXPECT_DOUBLE_EQ(ball["position"]["z_m"].get<double>(), 0.03);
    EXPECT_TRUE(ball["speed_valid"].get<bool>());
    EXPECT_FALSE(ball["safe_for_control"].get<bool>());

    const json& safety = message["safety"];
    EXPECT_FALSE(safety["safe"].get<bool>());
    EXPECT_EQ(safety["hysteresis_remaining_ms"], 200);
    EXPECT_FALSE(safety["distance_valid"].get<bool>());
    EXPECT_FALSE(safety.contains("laser_ball_distance_m"));  // only when valid
    ASSERT_EQ(safety["unsafe_reasons"].size(), 2U);
    EXPECT_EQ(safety["unsafe_reasons"][0], "LaserNotConfirmed");
    EXPECT_EQ(safety["unsafe_reasons"][1], "LaserInTransit");
}

TEST(ZmqPublisherTest, SerialisationStaysWithinBudget) {
    // TRK-021 / plan R16: <= 0.5 ms per snapshot. Asserted in Release only
    // (the budget is a Release-on-Pi contract; Debug records the number).
    tracking::ZmqPublisher publisher(zmq_config("tcp://127.0.0.1:25549"),
                                     safety_config(), 0.03);
    tracking::TrackingSnapshot snapshot = sample_snapshot();
    snapshot.laser = snapshot.ball;  // worst case: both slots serialised
    snapshot.laser.object_type = static_cast<std::uint8_t>(tracking::ObjectType::Laser);
    publisher.publish(snapshot, 55.5);  // warm-up outside the measurement

    constexpr int kIterations = 200;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kIterations; ++i) {
        publisher.publish(snapshot, 55.5);
    }
    const double avg_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                  start)
            .count() /
        kIterations;
    ::testing::Test::RecordProperty("avg_publish_ms_x1000",
                                    static_cast<int>(avg_ms * 1000));
#ifdef NDEBUG
    EXPECT_LT(avg_ms, 0.5) << "serialisation budget exceeded (R16)";
#endif
}

TEST(ZmqPublisherTest, MessageIdsAreMonotonic) {
    tracking::ZmqPublisher publisher(zmq_config("tcp://127.0.0.1:25548"),
                                     safety_config(), 0.03);
    const tracking::TrackingSnapshot snapshot = sample_snapshot();
    publisher.publish(snapshot, -1.0);
    publisher.publish(snapshot, -1.0);
    publisher.publish(snapshot, -1.0);
    EXPECT_EQ(publisher.messages_published(), 3U);
}

}  // namespace
