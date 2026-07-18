#include "ball_detector.hpp"
#include "camera_source.hpp"
#include "capture_thread.hpp"
#include "config.hpp"
#include "coordinate_mapper.hpp"
#include "homography_loader.hpp"
#include "frame_quality.hpp"
#include "export_thread.hpp"
#include "frame_ring_buffer.hpp"
#include "logging.hpp"
#include "safe_for_control.hpp"
#include "snapshot_builder.hpp"
#include "spsc_ring.hpp"
#include "tracker.hpp"
#include "tracking_snapshot.hpp"
#include "zmq_publisher.hpp"

#include <opencv2/videoio.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

// SIGINT/SIGTERM request a clean shutdown (plan R23). The handler only sets a
// lock-free atomic flag — the only async-signal-safe action available to it.
std::atomic<bool> g_stop_requested{false};
static_assert(std::atomic<bool>::is_always_lock_free,
              "signal handler requires a lock-free stop flag");

void handle_stop_signal(int) { g_stop_requested.store(true, std::memory_order_relaxed); }

// Capture-clock "now" in the same domain as FrameMetadata.capture_timestamp_ns.
std::int64_t steady_now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

}  // namespace

int main(int argc, char** argv) {
    const std::string config_path =
        (argc > 1) ? argv[1] : "config/tracking_core.yaml";
    tracking::Config config;
    try {
        config = tracking::Config::load(config_path);
        // One catch for the whole pre-logger window: ConfigError from load,
        // filesystem_error/spdlog_ex from init. The logger cannot report its
        // own bootstrap failures, so this path stays on stderr.
        tracking::logging::init(config.logging);
    } catch (const std::exception& e) {
        std::cerr << "Startup error: " << e.what() << std::endl;
        return 1;
    }

    // Post-init failures (zmq bind, OpenCV throws) must drain the logger and
    // exit 1 rather than reach std::terminate with queued messages lost.
    try {
        // TRK-017/018 fail-fast (plan KTD-7): the core refuses to start
        // without valid, paired, undistorted-fit calibration artifacts.
        const tracking::CalibrationArtifacts calibration =
            tracking::load_calibration_artifacts(config.calibration.intrinsics_path,
                                                 config.calibration.extrinsics_path,
                                                 config.coordinate);
        tracking::CoordinateMapper coordinate_mapper(calibration, config.coordinate,
                                                     config.ball.radius_m);
        LOG_INFO("calibration loaded: camera '{}' at floor position ({:.3f}, {:.3f}, "
                 "{:.3f}) m",
                 calibration.intrinsics.camera_id, coordinate_mapper.camera_centre()[0],
                 coordinate_mapper.camera_centre()[1], coordinate_mapper.camera_centre()[2]);
        // TRK-020: snapshot assembly + the ADR-007 predicate, evaluated once
        // per frame after tracking. The tracker operates in pixel space; the
        // mapper projects at snapshot build.
        tracking::SnapshotBuilder snapshot_builder(coordinate_mapper, config.coordinate);
        tracking::SafeForControlEvaluator safety_evaluator(config.safe_for_control,
                                                           config.ball.radius_m);
        bool last_published_safe = false;

        tracking::CameraSource camera(config.camera);
        if (!camera.is_open()) {
            LOG_ERROR("Unable to open camera device {}", config.camera.device_id);
            tracking::logging::shutdown();
            return 1;
        }

        // TRK-021: snapshot publisher on its own export thread, fed through a
        // freshest-wins SPSC ring (ADR-002 CONFLATE semantics end to end).
        tracking::ZmqPublisher zmq_publisher(config.zmq, config.safe_for_control,
                                             config.ball.radius_m);
        tracking::SpscRing<tracking::TrackingSnapshot> snapshot_ring(4);
        tracking::ExportThread export_thread(snapshot_ring, zmq_publisher);
        export_thread.start();

        tracking::BallDetector ball_detector(config.ball, config.camera.height,
                                             config.camera.width);
        tracking::Tracker tracker(config.track, config.gating);
        std::vector<tracking::Observation> observations;
        observations.reserve(tracking::Tracker::kMaxObservations);

        // Capture (producer) and processing (consumer) sides of the TRK-005
        // ring buffer. Frames arrive as 8-bit BGR at the configured size.
        tracking::FrameRingBuffer ring_buffer(
            static_cast<std::size_t>(config.pipeline.ring_buffer_capacity),
            config.camera.height, config.camera.width, CV_8UC3);
        tracking::CaptureThread::Options capture_options;
        capture_options.cpu_core = config.pipeline.capture_cpu_core;
        capture_options.thread_priority = config.pipeline.capture_thread_priority;
        capture_options.camera_id = 0;
        tracking::CaptureThread capture(camera, ring_buffer, capture_options);
        capture.start();

        tracking::FrameQualityAssessor quality(config.frame_quality,
                                               config.camera.height, config.camera.width);
        auto last_quality_report = std::chrono::steady_clock::now();
        std::uint64_t degraded_in_window = 0;
        std::uint64_t rejected_at_last_report = 0;
        std::uint64_t captured_at_last_report = 0;
        std::uint64_t failures_at_last_report = 0;

        std::signal(SIGINT, handle_stop_signal);
        std::signal(SIGTERM, handle_stop_signal);

        cv::Mat frame(config.camera.height, config.camera.width, CV_8UC3);
        while (!g_stop_requested.load(std::memory_order_relaxed)) {
            const auto metadata = ring_buffer.try_pop(frame);
            // Plan R4: the tracker ticks every loop iteration on the capture
            // clock, so REJECT bursts and frame gaps age tracks instead of
            // freezing them at Confirmed.
            const std::int64_t now_ns = steady_now_ns();
            if (!metadata.has_value()) {
                observations.clear();
                tracker.update(observations, now_ns);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            // TRK-008 gate: garbage frames stop here instead of feeding
            // detection. Aggregated 1 Hz reporting, never per-frame.
            const tracking::FrameQuality frame_quality = quality.assess(frame);
            if (frame_quality == tracking::FrameQuality::DEGRADED) {
                ++degraded_in_window;
            }
            const auto now = std::chrono::steady_clock::now();
            if (now - last_quality_report >= std::chrono::seconds(1)) {
                const std::uint64_t rejected = quality.rejected_count();
                if (rejected != rejected_at_last_report || degraded_in_window > 0) {
                    LOG_WARN("frame quality: {} rejected, {} degraded in the last second",
                             rejected - rejected_at_last_report, degraded_in_window);
                }
                rejected_at_last_report = rejected;
                degraded_in_window = 0;
                // Source-health check: frames flat while grab failures grow
                // means the camera died — surface it instead of spinning
                // silently (recovery policy is a follow-up decision).
                const std::uint64_t captured_now = capture.frames_captured();
                const std::uint64_t failures_now = capture.grab_failures();
                if (captured_now == captured_at_last_report &&
                    failures_now > failures_at_last_report) {
                    LOG_ERROR("camera source stalled: no frames for >=1 s, {} grab "
                              "failure(s) accumulating",
                              failures_now);
                }
                captured_at_last_report = captured_now;
                failures_at_last_report = failures_now;
                last_quality_report = now;
            }
            if (frame_quality == tracking::FrameQuality::REJECT) {
                snapshot_builder.note_frame_rejected();
                observations.clear();
                tracker.update(observations, now_ns);  // R4: REJECT still ages tracks
                continue;
            }
            snapshot_builder.mark_frame_admitted();  // INITIALISING -> RUNNING
            snapshot_builder.note_frame_processed();

            // TRK-010 ball detection feeding the TRK-014/015 tracker. This is
            // the observation-build seam (plan U4/KTD-6): detector outputs are
            // normalised to Observation here and nowhere else.
            const std::optional<tracking::BallObservation> ball =
                ball_detector.detect(frame);
            observations.clear();
            if (ball.has_value()) {
                tracking::Observation obs;
                obs.type = tracking::ObjectType::Ball;
                obs.centroid_px = ball->centroid_px;
                obs.radius_px = ball->radius_px;
                obs.capture_timestamp_ns = metadata->capture_timestamp_ns;
                observations.push_back(obs);
            }
            const auto& tracks = tracker.update(observations, now_ns);

            // TRK-020c: one snapshot + predicate evaluation per frame. The
            // snapshot feeds the TRK-021 publisher in Phase D; until then only
            // flip events surface (never per-frame logging).
            tracking::TrackingSnapshot snapshot = snapshot_builder.build(
                tracks, metadata->capture_timestamp_ns, now_ns);
            const tracking::SafetyResult safety =
                safety_evaluator.evaluate(snapshot, now_ns);
            if (safety.safe != last_published_safe) {
                LOG_INFO("safe_for_control -> {} (reasons=0x{:02x})", safety.safe,
                         safety.unsafe_reasons);
                last_published_safe = safety.safe;
            }
            snapshot_ring.try_push(snapshot);  // freshest-wins; never blocks
        }

        // Clean shutdown (plan R23): capture stops first (joins), then the
        // export thread drains its ring and joins; LINGER=0 keeps the socket
        // teardown non-blocking.
        LOG_INFO("shutdown requested; stopping capture and export threads");
        capture.stop();
        export_thread.stop();
    } catch (const std::exception& e) {
        LOG_CRITICAL("fatal error after startup: {}", e.what());
        tracking::logging::shutdown();
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    tracking::logging::shutdown();
    return 0;
}
