---
id: TRK-032
status: backlog
subsystem: tracking-core
tier: design
created: 2026-07-26
updated: 2026-07-26
depends_on: ["TRK-034"]
spec: null
plan: null
blockers: []
---
## Context

The tracking core cannot ingest a live frame on the machine it runs on. `CameraSource` (`tracking-core/src/core/camera_source.cpp:8-12`) uses `cv::VideoCapture` with `CAP_V4L2`, and on 64-bit Raspberry Pi OS a Bayer CSI sensor exposes no processed BGR `/dev/video0` — only the raw sensor stream, with no auto-exposure, black-level or lens-shading correction applied. ADR-011 D3 decides the resolution: a new `LibcameraSource` implementing the existing `FrameSource` interface, selected by config, with libcamera as an optional build dependency so the development-machine build stays green. This is a hot-path module with its own buffer lifecycle and failure semantics, which is why it is tier `design`.

## Acceptance

- `LibcameraSource` implements `FrameSource::grab(cv::Mat&)` unchanged; `CameraSource` and `ReplaySource` are untouched by this ticket beyond the format contract in TRK-033.
- Source selection is by config (`camera.source_type`), not by `#ifdef`, so a single binary can drive libcamera, V4L2/UVC, or replay.
- The build succeeds **with and without** libcamera present. Both configs green under `tools/pi5-remote-test.sh`.
- The 1536×864 sensor mode is selected explicitly and frames are delivered at 60 fps (ADR-011 D4). The applied mode and frame duration are logged at startup.
- Manual exposure and manual analogue gain applied from config at startup and never changed thereafter (ADR-004).
- `AfMode=Manual` with a fixed `LensPosition` applied at startup; the **applied** position is read back, logged, and surfaced for `system_health` (R-05 mitigation — applying it is not enough, it must be verified).
- Sustained ≥60 fps with frame drop ≤0.5 %, median end-to-end latency ≤30 ms, p99 ≤50 ms, core CPU ≤60 % of one core, measured on the Pi 5 (ADR-011 gate G3, CLAUDE.md §5).
- Camera disconnect or a libcamera error surfaces as `grab()` returning false, not a crash or a stall — the same contract `CameraSource` already honours.
- No allocation in `grab()` after startup (the `cpp` rule's hot-path discipline).

## Plan

Tier `design` — full plan to be written to a plan doc before implementation and linked from `plan:`. Open questions to settle there:

1. **Buffer ownership.** libcamera hands back a dmabuf-mapped request. Does `grab()` copy the Y plane into the caller's `cv::Mat`, or can the ring-buffer slot wrap the mapped buffer and defer the copy? The second is faster but ties frame lifetime to request recycling, which interacts with `FrameRingBuffer`'s freshest-wins overflow policy.
2. **Threading.** libcamera is callback-driven; `CaptureThread` is a blocking-producer design pinned to core 2 at SCHED_FIFO 80. Bridge with a condition variable inside `LibcameraSource` so the existing `CaptureThread` is unchanged, or restructure? Prefer the former — `CaptureThread`'s timestamping and priority handling is already Pi-5-verified.
3. **Timestamping.** libcamera supplies a sensor timestamp per request. Prefer it over `CaptureThread`'s `steady_clock` stamp if the clock bases can be reconciled — it removes scheduling jitter from the latency measurement. Confirm the clock base before relying on it.
4. **Optional dependency mechanism.** pkg-config `libcamera` with a CMake feature flag, and a stub that fails cleanly at config-load time when `source_type: libcamera` is requested in a build without it.

### Sub-tickets (create when the plan doc lands)

| Child | Title | Scope |
|-------|-------|-------|
| TRK-032a | libcamera build integration | Optional pkg-config dependency; both build configs green |
| TRK-032b | LibcameraSource capture + buffer lifecycle | Mode selection, request loop, Y-plane delivery |
| TRK-032c | Control application + readback | Exposure, gain, focus; startup logging and health surfacing |
| TRK-032d | On-target performance validation | ADR-011 gate G3 |

## Log

- 2026-07-26: created. Status: backlog. Tier design. Implements ADR-011 D3/D4; discharges ADR-011 gate G3. Depends on TRK-034 so the sensor modes and thresholds are measured before code is written against them.
