// TRK-007: FrameMetadata contract tests. The load-bearing checks are the
// static_asserts in frame_metadata.hpp itself (trivially copyable, <= 32
// bytes); this TU proves they compile in both build configs and pins the
// runtime copy semantics the ring buffer relies on.
#include "frame_metadata.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <type_traits>

namespace {

TEST(FrameMetadataTest, CompileTimeContract) {
    static_assert(std::is_trivially_copyable_v<tracking::FrameMetadata>);
    static_assert(sizeof(tracking::FrameMetadata) <= 32);
    static_assert(std::is_nothrow_default_constructible_v<tracking::FrameMetadata>);
    SUCCEED();
}

TEST(FrameMetadataTest, DefaultConstructionZeroes) {
    const tracking::FrameMetadata m;
    EXPECT_EQ(m.capture_timestamp_ns, 0);
    EXPECT_EQ(m.sequence_number, 0u);
    EXPECT_EQ(m.capture_duration_us, 0u);
    EXPECT_EQ(m.camera_id, 0);
}

TEST(FrameMetadataTest, CopyPreservesAllFields) {
    tracking::FrameMetadata a;
    a.capture_timestamp_ns = 123456789012345;
    a.sequence_number = 987654321;
    a.capture_duration_us = 16667;
    a.camera_id = 2;

    const tracking::FrameMetadata b = a;  // trivial copy — what slots do per frame
    EXPECT_EQ(b.capture_timestamp_ns, a.capture_timestamp_ns);
    EXPECT_EQ(b.sequence_number, a.sequence_number);
    EXPECT_EQ(b.capture_duration_us, a.capture_duration_us);
    EXPECT_EQ(b.camera_id, a.camera_id);

    // memcpy-equivalence is what trivial copyability buys on the hot path.
    tracking::FrameMetadata c;
    std::memcpy(&c, &a, sizeof(a));
    EXPECT_EQ(c.sequence_number, a.sequence_number);
    EXPECT_EQ(c.capture_timestamp_ns, a.capture_timestamp_ns);
}

}  // namespace
