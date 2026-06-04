---
id: TRK-026
status: backlog
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-021"
spec: null
plan: null
blockers: []
---

## Context

CLAUDE.md §9.3 defines specific performance budgets that v0.3 must meet on Pi 5. These must be validated by automated benchmarks. The budgets:
- End-to-end latency: median ≤ 30 ms, p99 ≤ 50 ms at 60 fps.
- Frame drop rate: ≤ 0.5% over 5-minute run.
- CPU usage: ≤ 60% of one Pi 5 core.
- Memory residency: bounded and non-growing (RSS within 5% of post-startup over 30 min).
- Thermal degradation: core degrades within 1s when CPU > 80°C.

## Acceptance

- A benchmark suite (separate CMake target `tracking_core_benchmarks`) that measures all five budgets.
- Latency benchmark: measures frame_capture_timestamp → publish_timestamp across 1000+ frames, reports median/p95/p99.
- Frame drop benchmark: runs pipeline for 5 minutes, counts ring buffer overwrites, computes drop rate.
- CPU benchmark: measures per-frame CPU time using `clock_gettime(CLOCK_THREAD_CPUTIME_ID)`.
- Memory benchmark: samples RSS at intervals over 30 minutes, asserts max-min spread ≤ 5% of post-startup value.
- Thermal benchmark: artificially loads CPU to raise temperature, verifies DEGRADED state transition within 1s of threshold crossing.
- All benchmarks produce machine-readable JSON output for trend tracking.
- Benchmarks designed to run on Pi 5 hardware only (skip or warn on other platforms).
- PASS/FAIL against the documented budgets; FAIL is advisory for now (does not block commit), becomes blocking before ship.

## Plan

U1. Create `benchmarks/` directory with CMake target `tracking_core_benchmarks`.
U2. Implement latency benchmark: synthetic frame source at 60 Hz, measure full pipeline transit time.
U3. Implement frame drop benchmark: stress test with burst input, count overwrites.
U4. Implement CPU benchmark: per-frame timing via POSIX `clock_gettime`.
U5. Implement memory benchmark: fork a monitoring thread that samples `/proc/self/status` VmRSS periodically.
U6. Implement thermal benchmark: stress CPU, monitor `/sys/class/thermal/thermal_zone0/temp`, verify state transition.
U7. JSON output writer for all benchmarks (machine-readable for trend tracking).
U8. Platform guard: skip gracefully on non-Pi 5 platforms with a warning.
U9. Add to CMake: `tracking_core_benchmarks` target, not built by default (explicit opt-in `cmake --build . --target tracking_core_benchmarks`).

## Log

- 2026-05-31: created. Status: backlog. Depends on assembled pipeline (TRK-021+). Must run on Pi 5 hardware.
