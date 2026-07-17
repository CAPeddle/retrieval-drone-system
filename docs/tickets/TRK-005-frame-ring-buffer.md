---
id: TRK-005
status: done
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-07-17
depends_on:
  - "TRK-002"
spec: null
plan: null
blockers: []
---

## Context

Per CLAUDE.md §7.1, pipeline stages communicate via lock-free single-producer/single-consumer queues. The frame ring buffer sits between the capture thread (producer) and the processing thread (consumer). Pre-allocated at startup, it holds N frames worth of `cv::Mat` data without any heap allocations on the hot path. If the consumer falls behind, the oldest frame is overwritten (the consumer always processes the freshest available frame, consistent with ADR-002's CONFLATE semantics).

## Acceptance

- A `FrameRingBuffer` class with configurable capacity (default 4 frames from config).
- Pre-allocates all `cv::Mat` storage at construction based on configured frame dimensions and type.
- `try_push()` returns immediately (non-blocking); overwrites oldest if full.
- `try_pop()` returns immediately; yields `std::nullopt` if empty.
- No heap allocation, no mutex, no lock on push or pop.
- Thread-safe for exactly one producer and one consumer (SPSC contract documented in class header).
- Unit tests: single-threaded push/pop, overflow overwrites oldest, empty pop returns nullopt, multi-threaded stress test (producer at 60 Hz, consumer at varying rates).
- Frame metadata (timestamp, sequence number) travels with the frame — not just the pixel data.

## Plan

U1. Create `src/core/frame_ring_buffer.hpp` with `FrameRingBuffer` class.
U2. Use atomic head/tail indices for lock-free SPSC. Implementation based on Lamport's classic ring buffer or `moodycamel::ReaderWriterQueue` pattern.
U3. Storage: `std::vector<FrameSlot>` where `FrameSlot` contains a pre-allocated `cv::Mat` and a `FrameMetadata` struct (timestamp, sequence_number, capture_duration_us).
U4. `try_push`: copies frame data into the next slot, advances head. If full, advances tail (drops oldest).
U5. `try_pop`: copies frame data out from tail slot, advances tail.
U6. Add config field: `pipeline.ring_buffer_capacity` (default 4).
U7. Unit tests in `test_frame_ring_buffer.cpp`: basic operations, overflow semantics, concurrent stress.
U8. Document the SPSC contract and overflow policy in the header comment.

## Log

- 2026-05-31: created. Status: backlog. Depends on TRK-003 (config for capacity) and TRK-002 (build system).
- 2026-07-17: backlog → in-progress. Starting v0.3 frame-pipeline goal run (TRK-005..008 on feat/v03-frame-pipeline).
- 2026-07-17: in-progress → done. FrameRingBuffer + FrameMetadata landed on feat/v03-frame-pipeline: SPSC ring with CAS-reclaim overwrite-oldest (torn reads discarded by failed pop CAS), pre-allocated slots from camera.width/height, pipeline.ring_buffer_capacity config (1-64). 6 unit tests incl. slow-consumer concurrent stress; 37/37 ctest green Release+Debug, zero warnings.
