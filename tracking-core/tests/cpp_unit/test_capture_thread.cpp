// Unit tests for the TRK-006 capture thread. No camera, no privileges: a
// synthetic FrameSource paces itself; SCHED_FIFO/affinity failures degrade
// gracefully (WARN) and are not asserted here.
#include "capture_thread.hpp"
#include "frame_ring_buffer.hpp"

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

namespace {

constexpr int kRows = 48;
constexpr int kCols = 64;

// Self-pacing synthetic camera: emits `max_frames` solid frames at `fps`,
// then reports failure (as a real camera does on disconnect). Absolute-time
// pacing via sleep_until keeps cumulative drift low.
class SyntheticSource final : public tracking::FrameSource {
public:
    SyntheticSource(double fps, int max_frames) : period_(1.0 / fps), max_frames_(max_frames) {}

    bool grab(cv::Mat& out) override {
        if (emitted_ >= max_frames_) {
            return false;
        }
        if (emitted_ == 0) {
            start_ = std::chrono::steady_clock::now();
            next_ = start_;
        }
        std::this_thread::sleep_until(next_);
        next_ += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(period_));
        out.create(kRows, kCols, CV_8UC1);
        out.setTo(cv::Scalar(static_cast<double>(emitted_ % 251)));
        last_ = std::chrono::steady_clock::now();
        ++emitted_;
        return true;
    }

    double measured_fps() const {
        const double seconds = std::chrono::duration<double>(last_ - start_).count();
        return seconds > 0.0 ? (emitted_ - 1) / seconds : 0.0;
    }

    int emitted() const { return emitted_; }

private:
    double period_;
    int max_frames_;
    int emitted_ = 0;
    std::chrono::steady_clock::time_point start_{};
    std::chrono::steady_clock::time_point next_{};
    std::chrono::steady_clock::time_point last_{};
};

tracking::CaptureThread::Options test_options() {
    tracking::CaptureThread::Options options;
    options.cpu_core = 0;
    options.thread_priority = 80;  // Will fail unprivileged -> WARN + continue.
    options.camera_id = 3;
    return options;
}

void drain_until_source_done(tracking::CaptureThread& capture, const SyntheticSource& source,
                             int max_frames) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (source.emitted() < max_frames && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    capture.stop();
}

TEST(CaptureThreadTest, PushRateTracksSourceFps) {
    constexpr double kFps = 100.0;
    constexpr int kFrames = 50;
    SyntheticSource source(kFps, kFrames);
    tracking::FrameRingBuffer buffer(8, kRows, kCols, CV_8UC1);
    tracking::CaptureThread capture(source, buffer, test_options());

    // Consume concurrently so nothing is dropped.
    std::atomic<bool> done{false};
    std::uint64_t popped = 0;
    std::thread consumer([&] {
        cv::Mat out;
        while (!done.load()) {
            if (buffer.try_pop(out).has_value()) {
                ++popped;
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(500));
            }
        }
        while (buffer.try_pop(out).has_value()) {
            ++popped;
        }
    });

    capture.start();
    drain_until_source_done(capture, source, kFrames);
    done.store(true);
    consumer.join();

    EXPECT_EQ(capture.frames_captured(), static_cast<std::uint64_t>(kFrames));
    EXPECT_EQ(popped + buffer.drops(), capture.frames_captured());
    // Push rate == source pacing. 10% bound on a shared dev machine; the 5%
    // acceptance figure is verified on-target (handoff recipe: local pass
    // separates logic errors, Pi run owns the timing claim).
    const double fps = source.measured_fps();
    EXPECT_GT(fps, kFps * 0.90);
    EXPECT_LT(fps, kFps * 1.10);
}

TEST(CaptureThreadTest, MetadataPopulatedAndMonotonic) {
    SyntheticSource source(200.0, 20);
    tracking::FrameRingBuffer buffer(32, kRows, kCols, CV_8UC1);
    tracking::CaptureThread capture(source, buffer, test_options());
    capture.start();
    drain_until_source_done(capture, source, 20);

    cv::Mat out;
    std::uint64_t expected_seq = 0;
    std::int64_t last_ts = 0;
    while (const auto metadata = buffer.try_pop(out)) {
        EXPECT_EQ(metadata->sequence_number, expected_seq++);  // gap-free
        EXPECT_GT(metadata->capture_timestamp_ns, last_ts);    // monotonic
        last_ts = metadata->capture_timestamp_ns;
        EXPECT_EQ(metadata->camera_id, 3);
        EXPECT_LT(metadata->capture_duration_us, 1000000u);
    }
    EXPECT_EQ(expected_seq, 20u);
}

TEST(CaptureThreadTest, StopStartLifecycleContinuesSequence) {
    SyntheticSource source(500.0, 1000);
    tracking::FrameRingBuffer buffer(64, kRows, kCols, CV_8UC1);
    tracking::CaptureThread capture(source, buffer, test_options());

    capture.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    capture.stop();
    const std::uint64_t first_batch = capture.frames_captured();
    EXPECT_GT(first_batch, 0u);

    capture.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    capture.stop();
    EXPECT_GT(capture.frames_captured(), first_batch);

    // Sequence numbers must continue across restart, not reset: drain and
    // verify strict increase with no duplicates.
    cv::Mat out;
    std::uint64_t last_seq = 0;
    bool first = true;
    while (const auto metadata = buffer.try_pop(out)) {
        if (!first) {
            EXPECT_GT(metadata->sequence_number, last_seq);
        }
        last_seq = metadata->sequence_number;
        first = false;
    }
}

TEST(CaptureThreadTest, DropsCountedWhenConsumerAbsent) {
    SyntheticSource source(1000.0, 100);
    tracking::FrameRingBuffer buffer(4, kRows, kCols, CV_8UC1);
    tracking::CaptureThread capture(source, buffer, test_options());
    capture.start();
    drain_until_source_done(capture, source, 100);

    EXPECT_EQ(capture.frames_captured(), 100u);
    EXPECT_GT(capture.frames_dropped(), 0u);
    EXPECT_EQ(capture.frames_dropped(), buffer.drops());
}

}  // namespace
