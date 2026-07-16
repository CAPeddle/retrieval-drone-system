---
id: TRK-004
status: in-progress
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-07-16
spec: null
plan: null
blockers: []
---

## Context

Per CLAUDE.md §7.1, hot-path logging must be async, ring-buffered, and lock-free. No `std::cout`, no `printf`, no synchronous file I/O on the hot path. spdlog with its async sink satisfies this. The logger must write to tmpfs or a dedicated mount — never to the SD card on the hot path. Log levels in the hot path are WARN+ by default; DEBUG/TRACE are compile-time gated for release builds.

## Acceptance

- spdlog initialised with async sink and ring-buffer (pre-allocated at startup).
- Log output directed to `/tmp/tracking_core/` (tmpfs on Pi OS) by default, configurable via YAML.
- Hot-path log macros (`LOG_WARN`, `LOG_ERROR`) resolve to spdlog async calls.
- `LOG_DEBUG` and `LOG_TRACE` are `#ifdef`-gated: compiled away in Release builds.
- Console output (stdout sink) available for development builds only, disabled in release.
- No allocations in the log call path — format strings are pre-registered or use compile-time format.
- Unit test: logging at various levels does not block the calling thread (timing assertion with async sink).
- Logger setup is configurable from `tracking_core.yaml`: `logging.level`, `logging.output_dir`, `logging.max_file_size_mb`.

## Plan

U1. Add `src/core/logging.hpp` — project-wide log macros wrapping spdlog, with compile-time gating.
U2. Add `src/core/logging.cpp` — initialisation function called once from `main()`, creates async logger with ring buffer (8192 slots), file rotating sink to configured directory.
U3. Add config fields: `logging.level` (default: `warn`), `logging.output_dir` (default: `/tmp/tracking_core/`), `logging.max_file_size_mb` (default: 10).
U4. In Release CMake config: define `NDEBUG` and `TRACKING_RELEASE` to gate out DEBUG/TRACE macros.
U5. Replace any existing `std::cerr`/`std::cout` in `main.cpp` with log calls.
U6. Unit test: `test_logging.cpp` — verify async behaviour (enqueue doesn't block), verify release build exclusion of debug logs.

## Log

- 2026-05-31: created. Status: backlog. Depends on TRK-003 (config fields for logging).
- 2026-07-16: backlog → in-progress. Implementation complete on feat/trk-004-async-logger per docs/plans/2026-07-15-001-feat-trk-004-async-logger-plan.md (supersedes the inline U1-U6 sketch): logging config section, spdlog async logger with LOG_* macros, main.cpp wiring, 26/26 tests green in Release and Debug. Front-matter depends_on corrected to include TRK-003. Awaiting review/merge.
