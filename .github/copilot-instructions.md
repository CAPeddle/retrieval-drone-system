# Retrieval Drone System — Workspace Instructions

## Project Status
This project is **actively being formulated**. Technical direction and way-of-work are still being established. Prefer to surface design trade-offs and ask clarifying questions rather than making unilateral architectural choices.

## What This Project Is
An autonomous ball-retrieval drone system. Development is iterative, starting from the simplest working slice and expanding scope progressively.

### Current Phase (Phase 1): Camera Streaming
**Goal:** Stream camera video from a Raspberry Pi 3B to a Raspberry Pi 5.

Key open questions still being resolved:
- The **pan-tilt head** (for laser aiming) may need to be mounted on the Pi 3B rather than the Pi 5 — physical mounting and I/O constraints are undecided.
- Overall system topology (which compute node owns what) is still being designed.

### Longer-Term Components (design artefacts exist, not yet built)
- **Tracking & Visualisation Core** (`tracking-system/`) — C++ tracking pipeline + Python viewer; ZeroMQ PUB/SUB transport. See `tracking-system/README.md`.
- **Laser controller** — pan-tilt servo assembly for precise spot aiming. See `CopilotServoCOntrol.md`.
- **Drone flight controller** — out of scope for current phase.

## Repository Layout
```
.github/                        # Workspace instructions (this file)
Claude Synthesised/             # ADRs — authoritative "why" for tracking subsystem decisions
tracking-system/                # Scaffold: C++ core + Python viewer (not yet deployed)
  src/core/                     # C++17 tracking pipeline (OpenCV + ZeroMQ PUB)
  src/viewer/                   # Python ZeroMQ SUB viewer
  tests/cpp_unit/               # GTest unit tests
  tests/python_integration/     # Python integration tests
  tools/                        # Utility scripts (e.g. record_camera.py)
*.md                            # Design research docs (ChatGPT, Gemini, Copilot sessions)
```

## Architecture Decisions
All accepted decisions are in `Claude Synthesised/` as ADRs. Read these before proposing architectural changes. Key decisions:
- **ADR-001**: Hybrid C++17 core / Python tooling — C++ for the hot path, Python for viewers/tools/tests.
- **ADR-002**: ZeroMQ PUB/SUB is the only inter-process transport. No in-process bindings.
- **ADR-003**: Graceful coordinate degradation — system must work even without full calibration.
- **ADR-006**: Floor-plane 2D world frame.
- **ADR-007**: `safe_for_control` geometric predicate gates actuation.

New architectural proposals should be written as ADRs following the conventions in `Claude Synthesised/README.md`.

## Build & Test (`tracking-system/`)

**C++ core:**
```bash
cmake -S tracking-system -B tracking-system/build
cmake --build tracking-system/build
```

**Run:**
```bash
# Terminal 1 — tracking core (camera 0, publishes on tcp://*:5556)
./tracking-system/build/src/core/tracking_core

# Terminal 2 — Python viewer
python3 tracking-system/src/viewer/viewer.py
```

**Python dependencies:**
```bash
pip install -r tracking-system/requirements.txt
```
Dependencies: `opencv-python`, `pyzmq`, `numpy`.

## Hardware Context
- **Raspberry Pi 3B** — camera node (currently: streaming source; possibly also pan-tilt controller).
- **Raspberry Pi 5** — primary compute node (tracking pipeline, viewer).
- **Pan-tilt head** — dual-axis servo assembly for laser aiming. Mounting node TBD.
- Camera interface: undecided for Phase 1 (options include MJPEG over HTTP, GStreamer, ZeroMQ).

## Conventions
- C++ standard: **C++17**. No exceptions from this without an ADR.
- ZeroMQ message frames always carry a `schema_version` field to prevent silent schema drift.
- ADR file naming: `ADR-NNN-short-title-with-hyphens.md`. No apostrophes, no spaces. Numbers never reused.
- Design research conversations (ChatGPT, Gemini, Copilot sessions) are kept as `.md` files in the repo root — treat them as reference, not specification.
- Specs and binding decisions go in ADRs.

## Agent Guidance
- **Phase 1 work** (camera streaming) is the immediate priority. Keep implementations minimal and Pi-compatible.
- When the pan-tilt mount node is unknown, surface the constraint rather than assuming an answer.
- Prefer `pybind11`-free solutions (see ADR-001); ZeroMQ is the correct inter-process bridge.
- Tests live in `tests/cpp_unit/` (GTest) and `tests/python_integration/`. Add tests alongside new code.
- Do not invent new transport mechanisms without an ADR.
