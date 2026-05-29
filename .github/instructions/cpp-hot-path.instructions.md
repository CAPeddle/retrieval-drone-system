---
applyTo: "tracking-system/src/core/**"
description: "C++ hot-path discipline and general conventions for the tracking core pipeline. Use when writing or reviewing C++ code in the tracking core."
---
# C++ Tracking Core — Coding Standards

## Hot-Path Definition

The "hot path" is the per-frame execution path (60–90 Hz): ingestion → frame quality → detection → tracking → coordinate mapping → fusion → state export. Code that runs at frame rate is hot-path. Startup, calibration, config reload, and ZMQ control messages are NOT hot-path.

When in doubt, treat it as hot-path.

## Memory (hot-path only)

- Pre-allocate everything reusable at startup. No allocations in the hot path.
- `std::vector` only with `reserve()` at construction. No unbounded `push_back`.
- `std::string` forbidden — use `std::string_view` or fixed `std::array<char, N>`.
- `std::shared_ptr` forbidden — single-owner via `std::unique_ptr` or raw non-owning pointers.
- No `new` / `delete`. Pre-allocated pools only.
- No RTTI / `dynamic_cast`. Use templates or `std::variant`.

## Threading (hot-path only)

- Pipeline stages communicate via **lock-free SPSC queues** (e.g. `moodycamel::ReaderWriterQueue`). No mutexes on the hot path.
- `std::shared_mutex` is permitted for infrequently-updated config/calibration state; hot path takes the shared lock.
- Ingestion and tracker threads: `SCHED_FIFO`, documented priorities.
- CPU affinity: ingestion + tracker pinned to dedicated cores; export/logging/heartbeat on remaining cores.

## Error Handling (hot-path only)

- No exceptions. Use `std::expected` (C++23) or a project `Result<T, Error>` type.
- Error types are enums or small POD structs. No string construction.

## Logging (hot-path only)

- Async, ring-buffered, lock-free (e.g. `spdlog` async sink).
- No `std::cout`, no `printf`, no synchronous file I/O.
- Hot-path levels: `WARN`+ by default. `DEBUG`/`TRACE` compile-time gated in release.

## General C++ Conventions (all code)

- **C++17 minimum.** C++20 where Pi 5 toolchain supports it.
- No raw owning pointers — `unique_ptr` or `shared_ptr` for ownership.
- Header/implementation split. Public interfaces in headers.
- `constexpr` and `noexcept` where genuinely applicable — not as decoration.
- Namespace: `dbrs::` with sub-namespaces matching directory (`dbrs::tracking::`, `dbrs::detection::` etc.).
- Include order: project headers → third-party (angle brackets) → standard library. Each group alphabetised.

## Deviation Rules

Deviation from the above is permitted only when:
1. The user explicitly asks for it.
2. An ADR requires it.
3. A measured benchmark justifies it AND a code comment references that benchmark.

"It's just easier" or "it's prototype code" is not justification.
