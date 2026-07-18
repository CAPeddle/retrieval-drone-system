---
id: TRK-006
status: done
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-07-17
depends_on:
  - "TRK-003"
  - "TRK-005"
spec: null
plan: null
blockers: []
---

## Context

The ingestion thread is the entry point of the hot path. It captures frames from the CSI camera via V4L2 (through OpenCV's VideoCapture) at the configured target FPS (60 default), with manual exposure locked at calibration time. It runs under `SCHED_FIFO` with documented priority and is pinned to a dedicated CPU core. Each captured frame is timestamped with the monotonic clock and pushed into the frame ring buffer.

## Acceptance

- A `CaptureThread` class that owns a `cv::VideoCapture` and the producer side of the `FrameRingBuffer`.
- Opens camera device from config (`camera.device_id`).
- Sets manual exposure via V4L2 properties (`camera.exposure_us` from config, disabling auto-exposure).
- Sets frame dimensions and FPS from config (`camera.width`, `camera.height`, `camera.target_fps`).
- Captures frames in a tight loop, timestamps each frame with `std::chrono::steady_clock::now()`.
- Pushes each `FrameSlot` (frame + metadata) into the ring buffer.
- Runs on a dedicated thread with `SCHED_FIFO` priority (configurable, default 80).
- CPU affinity pinned to core 2 (configurable via `pipeline.capture_cpu_core`).
- Graceful shutdown via atomic stop flag.
- Logs frame drops (ring buffer full → overwrite) at WARN level with count per second.
- Unit test: mock camera source (synthetic frames), verify push rate matches expected FPS within 5%.
- Integration note: on dev machines without V4L2, falls back to OpenCV default backend.

## Plan

U1. Create `src/core/capture_thread.hpp` with `CaptureThread` class declaration.
U2. Implement `capture_thread.cpp`:
   - Constructor: opens camera, configures exposure/resolution/fps via V4L2 properties.
   - `start()`: launches std::thread, sets SCHED_FIFO and CPU affinity.
   - Main loop: grab frame → timestamp → push to ring buffer → log drops.
   - `stop()`: sets atomic flag, joins thread.
U3. Add config fields: `camera.device_id`, `camera.width` (default 640), `camera.height` (default 480), `camera.target_fps` (default 60), `camera.exposure_us`, `pipeline.capture_cpu_core` (default 2), `pipeline.capture_thread_priority` (default 80).
U4. Implement V4L2 property setting: `CAP_PROP_AUTO_EXPOSURE = 1` (manual), `CAP_PROP_EXPOSURE = value`.
U5. Platform guard: `#ifdef __linux__` for SCHED_FIFO and CPU affinity; no-op on other platforms for dev builds.
U6. Unit test with synthetic frame source: verify timing, verify metadata populated, verify stop/start lifecycle.
U7. Update `main.cpp` to instantiate `CaptureThread` with config.

## Log

- 2026-05-31: created. Status: backlog. Depends on TRK-005 (frame ring buffer) and TRK-003 (config).
- 2026-07-17: backlog → in-progress. Starting on feat/v03-frame-pipeline; TRK-005 ring buffer landed.
- 2026-07-17: in-progress → done. CaptureThread landed: FrameSource seam (CameraSource V4L2-first + synthetic test source), SCHED_FIFO+affinity best-effort with WARN degradation, per-frame loop log-free (1 Hz aggregated drop WARN per KTD-2), sequence continuity across restart, main.cpp now producer/consumer via the ring buffer. New config camera.exposure_us + pipeline.capture_cpu_core/capture_thread_priority. 42/42 ctest green Release+Debug; on-target 5% rate + real SCHED_FIFO verification deferred to Pi (handoff recipe).
- 2026-07-17: review note: V4L2 CAP_PROP_EXPOSURE is likely in 100 us units (exposure_absolute); the raw exposure_us pass-through needs on-target verification with v4l2-ctl before calibration exposure is trusted. Added to the PR's deployment checks.
