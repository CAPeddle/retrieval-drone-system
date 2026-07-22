#pragma once

// TRK-007: per-frame metadata carried alongside the pixel data through every
// pipeline stage (ring buffer -> quality gate -> detection -> export). Defined
// once so timestamps are never re-derived downstream. The ZMQ message's
// publish/frame_capture timestamps derive from capture_timestamp_ns (ADR-002).

#include <cstdint>
#include <type_traits>

namespace tracking {

struct FrameMetadata {
    std::int64_t capture_timestamp_ns = 0;   // std::chrono::steady_clock, at grab completion
    std::uint64_t sequence_number = 0;       // monotonic, gap-free per source
    std::uint32_t capture_duration_us = 0;   // grab start -> grab complete
    std::uint8_t camera_id = 0;              // multi-camera scaffolding (N=1 in v0.3)
    // TRK-009 U5 (R4): set on the first frame after a replay-clip loop restart so
    // the modulation detector flushes its window at the discontinuity. Frame-
    // aligned by construction (stamped by the capture thread, carried per frame)
    // — a polled accessor would not be, letting a window span the wrap.
    bool wrapped = false;
};

// Hot-path contract: copied by value into ring-buffer slots at frame rate.
static_assert(std::is_trivially_copyable_v<FrameMetadata>,
              "FrameMetadata must be trivially copyable (copied per frame on the hot path)");
static_assert(sizeof(FrameMetadata) <= 32,
              "FrameMetadata must stay within half a cache line");

}  // namespace tracking
