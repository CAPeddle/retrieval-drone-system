---
id: TRK-031
status: done
subsystem: tracking-core
tier: small
created: 2026-07-17
updated: 2026-07-17
spec: null
plan: null
blockers: []
---
## Context

The Pi 5 (deployment target) has no camera attached and the operator is
remote; the only sensor is the ov5647 on the Pi 3B. Recorded real-sensor clips
replayed through a `FrameSource` give deterministic, hardware-realistic tests
with no live-hardware dependency per run — the seed of the TRK-025 replay
harness the ADR-007 gate requires. Chosen over a live network source (Stage 2)
because replay runs survive either Pi being down.

## Acceptance

- A `ReplaySource : FrameSource` reading a video file via OpenCV, with
  optional fps pacing and loop mode; end-of-file reports grab failure like a
  camera disconnect.
- Hermetic unit tests (self-generated clip via cv::VideoWriter): frame count,
  order, end-of-stream, loop, pacing.
- Integration test gated on `TRACKING_REPLAY_DIR`: recorded ov5647 clips
  (normal / underexposed / overexposed) flow through `FrameQualityAssessor`
  with the expected verdicts; skips cleanly where recordings are absent.
- Scenario clips recorded from the Pi 3B and stored on the Pi 5; capture
  procedure scripted (`tools/record-scenarios-pi3b.sh`).
- `tools/pi5-remote-test.sh` exports `TRACKING_REPLAY_DIR` when recordings
  are present, so on-target runs include the replay integration test.

## Plan

U1. `src/core/include/replay_source.hpp` + `replay_source.cpp` (ReplaySource).
U2. `tests/cpp_unit/test_replay_source.cpp` — hermetic tests via VideoWriter.
U3. `tests/cpp_unit/test_replay_integration.cpp` — env-gated real-clip test.
U4. `tools/record-scenarios-pi3b.sh` — rpicam-vid capture + transcode + store.
U5. Record the three scenarios; wire TRACKING_REPLAY_DIR into pi5-remote-test.

## Log

- 2026-07-17: created. Status: backlog.
- 2026-07-17: backlog → in-progress. Stage 3 of the remote-testing evaluation: replay-based hardware-realistic tests.
- 2026-07-17: in-progress → done. ReplaySource + scenario library live: hermetic unit tests (VideoWriter-generated clips) + env-gated integration tests running real ov5647 recordings through the shipped config on the Pi 5 — 65/65 green both configs on-target. First real-world catch: the guessed blur_threshold 100 rejected every real frame; recalibrated to 12 from measured data (sharp ~33, sigma-3 blur ~2.5). Recording procedure scripted (record-scenarios-pi3b.sh); pi5-remote-test.sh auto-arms TRACKING_REPLAY_DIR.
