---
id: TRK-007
status: backlog
subsystem: tracking-core
tier: mechanical
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-006"
spec: null
plan: null
blockers: []
---

## Context

Each frame passing through the pipeline needs associated metadata: capture timestamp (monotonic), sequence number (monotonic, gap-free), capture duration (time from grab start to grab complete), and source camera ID (for multi-camera scaffolding). This struct is defined once and carried through every pipeline stage. Keeping it separate from `cv::Mat` avoids re-deriving timestamps or injecting per-stage cost.

## Acceptance

- `FrameMetadata` struct defined in a shared header: `capture_timestamp_ns` (int64, steady_clock), `sequence_number` (uint64), `capture_duration_us` (uint32), `camera_id` (uint8).
- Struct is trivially copyable (POD-like) — no heap members, no virtual functions.
- Used by `FrameRingBuffer` and propagated through all pipeline stages to the export layer.
- `publish_timestamp_ms` and `frame_capture_timestamp_ms` in the ZMQ message derive from this struct.
- Unit test: sizeof check (must be ≤ 32 bytes to fit in cache line alongside pointer), trivial-copy static_assert.

## Plan

U1. Create `src/core/frame_metadata.hpp` with the struct definition.
U2. Add `static_assert(std::is_trivially_copyable_v<FrameMetadata>)`.
U3. Add `static_assert(sizeof(FrameMetadata) <= 32)`.
U4. Update `FrameRingBuffer`'s `FrameSlot` to include `FrameMetadata` (if not already from TRK-005).
U5. Unit test: compile-time assertions pass, runtime construction and copy behave correctly.

## Log

- 2026-05-31: created. Status: backlog. Depends on TRK-005 (ring buffer uses this).
