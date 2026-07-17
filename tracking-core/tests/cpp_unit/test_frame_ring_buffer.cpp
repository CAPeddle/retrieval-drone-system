// Unit tests for the TRK-005 SPSC frame ring buffer. No hardware required.
#include "frame_metadata.hpp"
#include "frame_ring_buffer.hpp"

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

constexpr int kRows = 48;
constexpr int kCols = 64;
constexpr int kType = CV_8UC1;

// A solid-valued frame whose pixels encode the sequence number, so a torn
// (half-overwritten) copy is detectable as a non-uniform image.
cv::Mat solid_frame(std::uint64_t seq) {
    return cv::Mat(kRows, kCols, kType, cv::Scalar(static_cast<double>(seq % 251)));
}

tracking::FrameMetadata meta(std::uint64_t seq) {
    tracking::FrameMetadata m;
    m.capture_timestamp_ns = static_cast<std::int64_t>(seq) * 1000;
    m.sequence_number = seq;
    m.capture_duration_us = 42;
    m.camera_id = 1;
    return m;
}

bool is_uniform(const cv::Mat& m, double* value_out) {
    double lo = 0.0, hi = 0.0;
    cv::minMaxLoc(m, &lo, &hi);
    if (value_out != nullptr) {
        *value_out = lo;
    }
    return lo == hi;
}

TEST(FrameRingBufferTest, PushPopRoundTrip) {
    tracking::FrameRingBuffer buffer(4, kRows, kCols, kType);
    EXPECT_FALSE(buffer.try_push(solid_frame(7), meta(7)));

    cv::Mat out;
    const auto popped = buffer.try_pop(out);
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(popped->sequence_number, 7u);
    EXPECT_EQ(popped->capture_timestamp_ns, 7000);
    EXPECT_EQ(popped->capture_duration_us, 42u);
    EXPECT_EQ(popped->camera_id, 1);
    double value = -1.0;
    EXPECT_TRUE(is_uniform(out, &value));
    EXPECT_EQ(value, 7.0);
}

TEST(FrameRingBufferTest, EmptyPopReturnsNullopt) {
    tracking::FrameRingBuffer buffer(4, kRows, kCols, kType);
    cv::Mat out;
    EXPECT_FALSE(buffer.try_pop(out).has_value());
}

TEST(FrameRingBufferTest, PopsInFifoOrder) {
    tracking::FrameRingBuffer buffer(4, kRows, kCols, kType);
    for (std::uint64_t seq = 0; seq < 3; ++seq) {
        buffer.try_push(solid_frame(seq), meta(seq));
    }
    cv::Mat out;
    for (std::uint64_t seq = 0; seq < 3; ++seq) {
        const auto popped = buffer.try_pop(out);
        ASSERT_TRUE(popped.has_value());
        EXPECT_EQ(popped->sequence_number, seq);
    }
    EXPECT_FALSE(buffer.try_pop(out).has_value());
}

TEST(FrameRingBufferTest, OverflowDropsOldest) {
    tracking::FrameRingBuffer buffer(4, kRows, kCols, kType);
    int drops_reported = 0;
    for (std::uint64_t seq = 0; seq < 6; ++seq) {
        if (buffer.try_push(solid_frame(seq), meta(seq))) {
            ++drops_reported;
        }
    }
    EXPECT_EQ(drops_reported, 2);
    EXPECT_EQ(buffer.drops(), 2u);

    // The consumer sees the freshest window: 2..5, oldest first.
    cv::Mat out;
    for (std::uint64_t seq = 2; seq < 6; ++seq) {
        const auto popped = buffer.try_pop(out);
        ASSERT_TRUE(popped.has_value());
        EXPECT_EQ(popped->sequence_number, seq);
    }
    EXPECT_FALSE(buffer.try_pop(out).has_value());
}

TEST(FrameRingBufferTest, MismatchedFrameIsRejectedNotPushed) {
    // A dims/type mismatch must be refused: silently reallocating the slot
    // would both allocate on the hot path and race the consumer's copy.
    tracking::FrameRingBuffer buffer(4, kRows, kCols, kType);
    const cv::Mat wrong_size(kRows * 2, kCols, kType, cv::Scalar(1));
    const cv::Mat wrong_type(kRows, kCols, CV_8UC3, cv::Scalar(1, 1, 1));
    EXPECT_FALSE(buffer.try_push(wrong_size, meta(0)));
    EXPECT_FALSE(buffer.try_push(wrong_type, meta(1)));
    EXPECT_EQ(buffer.rejected(), 2u);

    cv::Mat out;
    EXPECT_FALSE(buffer.try_pop(out).has_value());  // nothing was pushed
    EXPECT_FALSE(buffer.try_push(solid_frame(2), meta(2)));  // matching frame still works
    EXPECT_TRUE(buffer.try_pop(out).has_value());
}

TEST(FrameRingBufferTest, PopCopiesIntoPreallocatedWithoutReshape) {
    tracking::FrameRingBuffer buffer(2, kRows, kCols, kType);
    buffer.try_push(solid_frame(3), meta(3));
    cv::Mat out(kRows, kCols, kType);
    const void* data_before = out.data;
    ASSERT_TRUE(buffer.try_pop(out).has_value());
    EXPECT_EQ(out.data, data_before);  // no reallocation on the consumer side
}

// Producer at full tilt, consumer deliberately slower: sequence numbers must
// be strictly increasing, every popped frame uniform (no torn copies), and
// pushed == popped + dropped + retained.
TEST(FrameRingBufferTest, ConcurrentStressSlowConsumer) {
    constexpr std::uint64_t kFrames = 2000;
    tracking::FrameRingBuffer buffer(8, kRows, kCols, kType);
    std::atomic<bool> producer_done{false};

    std::thread producer([&] {
        for (std::uint64_t seq = 0; seq < kFrames; ++seq) {
            buffer.try_push(solid_frame(seq), meta(seq));
            if (seq % 16 == 0) {
                std::this_thread::yield();
            }
        }
        producer_done.store(true);
    });

    std::uint64_t popped_count = 0;
    std::uint64_t last_seq = 0;
    bool first = true;
    bool torn = false;
    cv::Mat out;
    while (true) {
        const auto popped = buffer.try_pop(out);
        if (!popped.has_value()) {
            if (producer_done.load()) {
                break;
            }
            std::this_thread::yield();
            continue;
        }
        double value = -1.0;
        if (!is_uniform(out, &value) ||
            value != static_cast<double>(popped->sequence_number % 251)) {
            torn = true;
        }
        if (!first) {
            EXPECT_GT(popped->sequence_number, last_seq);
        }
        last_seq = popped->sequence_number;
        first = false;
        ++popped_count;
        // Slow consumer: force the overwrite path to be exercised.
        if (popped_count % 8 == 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    }
    producer.join();

    EXPECT_FALSE(torn) << "popped a torn (half-overwritten) frame";
    EXPECT_GT(buffer.drops(), 0u) << "slow consumer never forced the overwrite path";
    EXPECT_LE(popped_count + buffer.drops(), kFrames);
    EXPECT_GT(popped_count, 0u);
}

}  // namespace
