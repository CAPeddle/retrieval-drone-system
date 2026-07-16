---
title: spdlog async hot-path logging — compile-time gating and the v1.14 promise gotcha
captured: 2026-07-16
applies_to: [tracking-core]
tags: [spdlog, logging, hot-path, compile-time-gating, cmake]
problem_type: pattern
source: internal
---

# spdlog async hot-path logging — compile-time gating and the v1.14 promise gotcha

## Summary

How TRK-004 wired spdlog into the §7.1 hot-path discipline: spdlog-native
compile-time level gating instead of hand-rolled `#ifdef`, how to *prove* the
gate in both build configs, and two library realities that falsified plan
claims until caught by review — a per-message heap allocation in spdlog v1.14
and the mutex-based (not lock-free) async queue. Reach for this when adding
logging to a latency-budgeted module, when bumping/downgrading spdlog, or when
writing tests for anything compile-time gated.

## Context

TRK-004 (PR #9) built `tracking::logging` — the async ring-buffered logger the
cpp rule §7.1 mandates. The multi-agent review plus upstream source reads
falsified three plausible-sounding claims the plan originally carried.

## Pattern: SPDLOG_ACTIVE_LEVEL gating

`tracking-core/src/core/include/logging.hpp` defines the floor **before**
including any spdlog header, guarded so a TU can override:

```cpp
#ifndef SPDLOG_ACTIVE_LEVEL
#ifdef NDEBUG
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO   // Release: DEBUG/TRACE compiled out
#else
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE  // Debug: everything compiled in
#endif
#endif
#include <spdlog/spdlog.h>
```

- `LOG_*` macros wrap `SPDLOG_LOGGER_*`, which expand to nothing (arguments
  **unevaluated**) below the floor — no hand-rolled `#ifdef`, no
  `TRACKING_RELEASE` define needed; `NDEBUG` (automatic in Release) carries it.
- Release floor is `INFO`, not `WARN`: compile out exactly what §7.1 names
  (DEBUG/TRACE) so `logging.level` stays runtime-meaningful on the deployed
  binary. A floor above the config surface silently turns valid config values
  into no-ops.
- **Include-order trap:** a TU that includes a raw spdlog header before
  `logging.hpp` silently gets spdlog's own default floor (`INFO`), losing the
  Debug-build TRACE floor. Convention: include `logging.hpp`, never spdlog
  directly. Unenforced — grep for `#include <spdlog/` outside `logging.*` when
  debugging "my LOG_DEBUG does nothing".

## Pattern: proving the gate in tests

Two TUs, two jobs (`tracking-core/tests/cpp_unit/`):

- `test_logging.cpp` pins `#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_WARN`
  before the include (the `#ifndef` makes that the supported override) so
  argument-elision can be asserted identically in both configs: a counter
  passed to `LOG_DEBUG` must stay 0.
- `test_logging_default_level.cpp` sets **no** override and `static_assert`s
  the NDEBUG mapping — the override TU can never exercise the header's own
  default branch, so without this file a Release-maps-to-TRACE typo passes
  every other test.
- Test-isolation gotcha: the rotating sink **appends**, and both build
  configs' test binaries share `::testing::TempDir()` paths keyed on test
  name. Clean the directory before use and in TearDown, or a stale log from
  the other config leaks into content assertions (bit us: a Release-run line
  failed a Debug assertion).

## Gotcha: spdlog v1.14 heap-allocates a std::promise per message

In v1.14.x, `async_msg` (thread_pool.h) carries a `std::promise<void>
flush_promise` **member**, default-constructed for every log-type message —
with libstdc++ that is two heap allocations per `LOG_*` call, regardless of
message size. This violates any "no allocation in the log call path" contract
and no configuration avoids it. **Fixed upstream in v1.15** (the promise left
`async_msg`). The build pins `GIT_TAG v1.15.3` with a do-not-downgrade comment
in `tracking-core/CMakeLists.txt`. A wall-clock timing test does not catch
this class of regression — ~200 ns mallocs pass a 250 ms/1000-call bound
trivially; only an allocation-count test or a source read does.

## Gotcha: "async" is not "lock-free"

spdlog's async queue (`mpmc_blocking_q.h`) takes a `std::mutex` on every
enqueue, shared with the worker's dequeue. With
`async_overflow_policy::overrun_oldest` the producer never *waits for queue
space* (full queue drops the eldest — ADR-002 CONFLATE spirit), but the
critical section is real. Consequence recorded in the plan's KTD-2: a
SCHED_FIFO thread (TRK-006's capture thread) logging through a plain mutex
risks unbounded priority inversion — decide policy before hot-loop adoption.

## Trade-offs / secondary pitfalls

- Emit init-time safety diagnostics **before** `logger->set_level(level)`: a
  fresh logger admits WARN, so warnings survive a strict configured level
  (`error`/`critical`). Setting the level first silently swallowed the
  not-tmpfs SD-card warning (confirmed empirically before the fix).
- `spdlog::shutdown()` nulls the default logger; `SPDLOG_LOGGER_*` has no null
  check, so a stray post-shutdown `LOG_*` is a null deref. Our `shutdown()`
  installs a synchronous stderr fallback default afterwards.
- `init()` starts with `spdlog::shutdown()` (idempotent-by-replacement) so
  test fixtures can re-init per case without the duplicate-registration throw.

## Source links

- [`tracking-core/src/core/include/logging.hpp`](../../../tracking-core/src/core/include/logging.hpp) — the header pattern
- [`tracking-core/tests/cpp_unit/test_logging_default_level.cpp`](../../../tracking-core/tests/cpp_unit/test_logging_default_level.cpp) — the no-override static_assert TU
- [PR #9](https://github.com/CAPeddle/retrieval-drone-system/pull/9) — review findings and fixes
- [`docs/plans/2026-07-15-001-feat-trk-004-async-logger-plan.md`](../../plans/2026-07-15-001-feat-trk-004-async-logger-plan.md) — KTD-1/KTD-2 rationale
