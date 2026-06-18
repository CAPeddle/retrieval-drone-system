# Retrieval Drone System — Workspace Instructions

## Project Identity

An autonomous ball-retrieval drone system: fixed-camera tracking core, steerable laser pointer, and drone. The **Tracking & Visualisation Core** is the current focus — a C++ pipeline that detects the ball and laser, publishes state over ZeroMQ, and gates actuation via the `safe_for_control` predicate.

### Current Phase

**Phase 1: Camera Streaming** — stream video from Pi 3B → Pi 5. Keep implementations minimal and Pi-compatible.

**v0.3 target (tracking core):** one camera, one laser, one ball, single-camera floor-plane tracking at ≥ 60 fps on Pi 5.

## Work Tracking

Work flows through [BOARD.md](../BOARD.md), a flat kanban. Each ticket has a story file at `docs/tickets/<ID>-<slug>.md` holding its plan, status, and history.

- **BOARD.md** is a thin index. One-line changes only. Rules in [`instructions/board.instructions.md`](instructions/board.instructions.md).
- **Story files** hold all ticket detail. Schema in [`instructions/tickets.instructions.md`](instructions/tickets.instructions.md).
- **Three tiers:** `mechanical` (script + commit), `small` (plan inline, hand-execute), `design` (spec + plan files, then implement).
- **Helpers:** [`tools/board/`](../tools/board/) provides Python scripts (`ticket_new.py`, `ticket_move.py`, `board_check.py`, `ticket_archive.py`) that perform ticket scaffolding, column moves, linting, and archival atomically. Prefer scripts over hand-editing BOARD.md and story files; see [`tools/board/README.md`](../tools/board/README.md).

Before starting a ticket: read its story file. Before changing status: update the story file's `status:` front-matter, append a `## Log` entry, and move the line in BOARD.md — all in one commit. The `tools/board/ticket_move.py` script does all of this atomically.

## Repository Layout

```
.github/                        # Workspace & file-scoped instructions
  instructions/                 # On-demand instruction files (coding standards, review, ADR governance)
docs/adr/                       # ADRs — authoritative architecture decisions
tracking-core/                  # C++ core + Python viewer scaffold
  src/core/                     # C++17 tracking pipeline (OpenCV + ZeroMQ PUB)
  src/viewer/                   # Python ZeroMQ SUB viewer
  tests/cpp_unit/               # GTest unit tests
  tests/python_integration/     # Python integration tests
  tools/                        # Utility scripts (e.g. record_camera.py)
*.md (repo root)                # Design research docs — reference, not specification
CLAUDE.md                       # Full agent operating contract (deep reference)
```

## Architecture Governance

**Source of truth:** accepted ADRs in `docs/adr/`. Read before proposing changes.

| Rule | Detail |
|------|--------|
| ADRs are binding | Accepted ADRs are not reopened without explicit user invitation. |
| Cite by ID | Reference decisions as "ADR-007 clause 3", not paraphrases. |
| State assumptions | If a recommendation depends on a premise, name it explicitly. |
| Contradiction protocol | If a request contradicts an accepted ADR, surface the conflict and update the ADR/docs *before* implementing. |
| New decisions | Architecturally significant choices get a new ADR (next free number, never reused). See `.github/instructions/adr-governance.instructions.md`. |

### Locked Decisions (do not relitigate)

| ADR | Locks |
|-----|-------|
| 001 | C++ core, Python tooling, ZMQ bridge. No pybind11 in runtime. |
| 002 | ZMQ PUB/SUB. Core BINDs, consumers CONNECT. `SNDHWM=1`, `CONFLATE=1`, `LINGER=0`. |
| 003 | Three-tier coordinate degradation. v0.3 ships `Plane2D_World` only. |
| 004 | Three-phase calibration lifecycle. |
| 005 | 15 Hz laser modulation, 4-frame PSD correlation. IR preferred. |
| 006 | FloorPlane2D world frame. Origin at primary Charuco corner. |
| 007 | `safe_for_control` — 8 clauses, single-snapshot, asymmetric hysteresis. |
| 008 | LaserController is a separate Python process with failsafe-off. |
| 010 | Per-class Z compensation in CoordinateMapper. |

ADR-009 (active calibration refinement) is **Proposed only** — do not reference in code or design docs.

## Core Boundary Invariants

- The tracking core **never** speaks MAVLink — wrap in an adapter.
- The tracking core **never** commands the laser — the LaserController adapter owns that.
- Every external system is a **separate process** communicating via ZMQ (ADR-001). No pybind11/shared-memory shortcuts.
- The core publishes a **heartbeat ≥ 1 Hz** even when the data plane is silent. ZMQ silence = process dead.
- No SD-card writes on the hot path. Logging is async ring-buffer only.

## Build & Test

```bash
# C++ core
cmake -S tracking-core -B tracking-core/build
cmake --build tracking-core/build

# Run tracking core (camera 0, publishes tcp://*:5556)
./tracking-core/build/src/core/tracking_core

# Python viewer
pip install -r tracking-core/requirements.txt
python3 tracking-core/src/viewer/viewer.py
```

Tests: `tracking-core/tests/cpp_unit/` (GTest), `tracking-core/tests/python_integration/` (pytest). Add tests alongside new code.

## Hardware

- **Pi 3B** — camera node (streaming source; possibly pan-tilt controller, TBD).
- **Pi 5** — primary compute (tracking pipeline, viewer). 8 GB, 64-bit Pi OS. ~60-70% CPU budget for tracking.
- **Camera** — 5 MP NoIR CSI (OV5647). Rolling shutter. Focus ring must be physically locked.
- **Laser** — IR preferred, modulated by MCU per ADR-005/008.
- No internet dependency. No cloud calls.

## Conventions

- C++17 minimum. No exceptions without an ADR.
- ZMQ messages always carry `schema_version`.
- ADR naming: `ADR-NNN-short-title-with-hyphens.md`. Numbers never reused.
- Root `.md` files are design research — reference, not specification.
- Coding standards: see `.github/instructions/cpp-hot-path.instructions.md` and `.github/instructions/python-tooling.instructions.md`.

## Deeper Context

For the full operating contract (persona, physical environment detail, system boundaries, testing gates, risk register, glossary): see `CLAUDE.md`.
