---
description: Tracking-core C++ rules — hot-path discipline, general conventions, module boundaries, and coordinate/Z-compensation. Applies when editing tracking-core C++.
paths:
  - "tracking-core/**/*.cpp"
  - "tracking-core/**/*.hpp"
  - "tracking-core/**/*.h"
---

# Tracking-Core C++ Rules

Everything a C++ edit in the tracking core needs. Formatting below the level of
these rules is delegated to `clang-format`.

## What "the hot path" means

The hot path is the per-frame execution path that runs at frame rate (60–90 Hz):
ingestion → frame-quality assessment → detection → tracking → coordinate mapping
→ fusion → state export. It has a determinism contract (ADR-001). Startup,
calibration, config-reload, shutdown, and ZMQ control-message handling are NOT
hot-path and are held to looser rules. When unsure, treat as hot-path and ask.

## Hot-path discipline (§7.1)

**Memory**
- Pre-allocate everything reusable at startup (frame buffers, observation
  vectors, detector working buffers, ZMQ message buffers). No allocations in the
  hot path.
- `std::vector` only with `reserve()` at construction; no unbounded `push_back`.
  Genuinely unbounded → pre-allocated ring buffer.
- `std::string` forbidden in the hot path — use `std::string_view` for read-only,
  `std::array<char, N>` for owned data.
- `std::shared_ptr` forbidden in the hot path — single-owner via `std::unique_ptr`
  or raw pointers with documented lifetime.
- No `new`/`delete` in the hot path (pre-allocated pools only). No RTTI /
  `dynamic_cast` — use templates or `std::variant` / tagged unions.

**Threading**
- Pipeline stages communicate via lock-free SPSC queues (e.g.
  `boost::lockfree::spsc_queue`, `moodycamel::ReaderWriterQueue`). No mutexes on
  the hot path.
- Mutexes are permitted for config/calibration and other infrequently-updated
  shared state read by the hot path. Use `std::shared_mutex` if read-mostly; the
  hot path takes the shared lock.
- Ingestion and tracker threads run under `SCHED_FIFO` with documented
  priorities; document the priority-inversion analysis at the thread entry point.
- CPU affinity: ingestion and tracker pinned to two dedicated cores; export,
  logging, heartbeat on remaining cores. Core numbers configurable; the partition
  is fixed at startup.

**Error handling**
- No exceptions on the hot path. Use `std::expected` (C++23) or a project
  `Result<T, Error>`. Startup/config code may throw; hot-path code returns errors.
- Error types are enums or small POD structs — no string messages built in the
  hot path. Log templates are pre-formatted at startup with placeholders.

**Logging**
- Async, ring-buffered, lock-free (e.g. `spdlog` async sink). No `std::cout`,
  `printf`, or synchronous file I/O on the hot path. Never write the SD card on
  the hot path — async ring buffer to tmpfs / dedicated mount only.
- Hot-path log level `WARN`+ by default; `DEBUG`/`TRACE` compile-time gated for
  release builds.

## General C++ conventions (§7.2)

- C++17 minimum, C++20 preferred where the Pi 5 toolchain supports it (verify
  against the deployed compiler before using C++20 features).
- No raw owning pointers — `unique_ptr`/`shared_ptr` for ownership, raw
  pointers/references only for non-owning access (hot-path exception above).
- Header/implementation split; templates and inline functions in headers.
- `constexpr` and `noexcept` where they genuinely apply, not as decoration.
- Namespaces under `dbrs::` with sub-namespaces matching the directory
  (`dbrs::tracking::`, `dbrs::detection::`, …).
- Includes: angle-brackets for third-party, quotes for project headers. Order:
  project, third-party, standard library; each group alphabetised.

## When to deviate (§7.4)

Deviation is permitted only when: the user explicitly asks; an ADR requires it;
or a measured benchmark shows the rule is worse AND the deviation is documented
in a comment pointing at the benchmark. NOT permitted for "it's just easier" /
"prototype code" / "clean up later". Hot-path code violating discipline is
presumed wrong until the deviation is justified. Library selection within an open
category is a legitimate benchmark spike — document the result in a comment or,
if load-bearing, a new ADR.

## Module boundaries (§5)

The tracking core is one Linux process on the Pi 5. Its job: ingest frames,
detect objects, maintain tracks, project to FloorPlane2D (ADR-006) with
object-class Z compensation (ADR-010), fuse multi-camera (Phase 3+; N=1 in v0.3),
evaluate `safe_for_control` (ADR-007), publish the ZMQ stream (ADR-002) + a ≥1 Hz
heartbeat, and run a calibration health check (ADR-004 Phase 2).

**The core never:**
- Speaks MAVLink — wrap it in an adapter (Phase 3+).
- Commands the laser — the LaserController owns that (ADR-008).
- Commands the drone — the flight controller owns that.
- Auto-updates calibration without explicit operator action (until ADR-009 is
  promoted from Proposed).
- Writes the SD card on the hot path.
- Assumes a camera count beyond what the current calibration supports.

Adapters are always separate processes communicating over ZMQ — never bolt a
Python helper onto the core via pybind11 or shared memory (ADR-001). ZMQ contract
(ADR-002): PUB/SUB, core BINDs, consumers CONNECT, `ZMQ_SNDHWM=1`,
`ZMQ_CONFLATE=1`, `ZMQ_LINGER=0`. Consumers must treat heartbeat silence — not a
data-plane gap — as "process dead."

**Historical seam (do not revive).** The v0.2 Gemini ArUco/UDP/`solvePnP`
drone-positioning pipeline is **superseded**. Drone position comes from the
core's ZMQ stream via the MAVLink adapter (Phase 3+), not a parallel CV pipeline.
Do not propose hybrid designs that combine the ZMQ stream with the legacy
ArUco/UDP path.

## Coordinate mapping & Z compensation (§7 + ADR-003/006/010)

- Three coordinate tiers (ADR-003): `Full3D_World`, `Plane2D_World`,
  `ImagePixels`. v0.3 ships only `Plane2D_World`.
- FloorPlane2D (ADR-006): Z=0 is the floor; origin at the primary Charuco corner,
  X along the long edge, right-handed, Z up.
- Z=0 is NOT universally true. Projecting image pixels onto Z=0 is correct only
  for objects whose centroid lies on the floor — the ball and drone do not. Always
  apply ADR-010 object-class Z compensation before computing floor-plane
  distances. A 6 cm ball at the floor projects ~3 cm off under typical oblique
  geometry, exceeding the 2 cm alignment tolerance → systematic false negatives.
- Route all projection through `calibration_status`; respect the
  `INVALID`/`DEGRADED`/`VALID` triple. Never assume a calibration value is correct
  indefinitely.

## Code documentation (§8.6, brief)

Comments explain what this code does and why, not the system's architectural
position. Cite ADRs by ID where relevant (e.g. `// Per ADR-002, HWM=1 drops stale
messages on consumer lag.`) without restating the ADR's rationale. **Never cite
ADR-009** in code comments, design docs, or new ADRs — it is Proposed, not
authoritative; if active calibration becomes relevant, raise it with the user as
an open question. Doxygen-style comments encouraged on public headers, not
required for v0.3.
