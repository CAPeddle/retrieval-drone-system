# ADR-001: Hybrid C++ Core with Python Tooling

## Status
Accepted (2026-05-08, originally documented in v0.2 design)

## Context
The Tracking & Visualisation Core must sustain 60–90 fps frame ingestion and processing on a Raspberry Pi 5, with deterministic per-frame latency, while leaving CPU headroom for adjacent subsystems (drone flight controller, laser controller, system controller). The Pi 5 has no GPU suitable for ML inference, so the bottleneck is CPU throughput and scheduling determinism.

Forces:
- 60 fps gives a 16.6 ms per-frame budget; 90 fps gives 11.1 ms.
- Python's Global Interpreter Lock and reference-counting GC introduce non-deterministic pauses that will violate sub-frame latency requirements at high frame rates.
- OpenCV, NumPy, and ZeroMQ all have first-class Python bindings, making rapid prototyping for visualisation and offline tools strongly preferable in Python.
- The team prioritises iteration speed for tooling but determinism for the tracking loop.

## Decision
We will implement the primary tracking loop — Ingestion, Frame Quality Assessment, Detection, Tracking, Coordinate Mapping, Fusion, Export — in modern C++ (C++17 or later).

We will implement Visualisation, offline calibration tooling, replay tooling, and integration test harnesses in Python.

The two layers communicate exclusively through the ZeroMQ contract defined in ADR-002. We will not use `pybind11` or in-process bindings between core and tooling in v0.3, to keep the core fully decoupled from Python's runtime.

## Consequences

**Positive:**
- The hot path is free of GC pauses, GIL contention, and Python's allocation overhead.
- Tooling, viewer, and test harnesses iterate at Python speed.
- The core can be deployed headless on Pi 5 without any Python runtime present.
- Replacing the visualiser, adding new tools, or rewriting the calibration UI does not touch the core.

**Negative:**
- Two languages, two build systems (CMake + a Python project layout), two sets of dependencies.
- Data contracts must be serialisation-explicit (JSON or MessagePack over ZeroMQ); we cannot pass C++ objects to Python directly.
- Onboarding requires comfort with both languages.

**Risks:**
- Schema drift between C++ producer and Python consumers. Mitigated by treating the ZeroMQ message schema as a versioned contract with explicit `schema_version` field.

## Alternatives Considered
- **Pure Python with NumPy/OpenCV C-extensions and disciplined buffer reuse.** Rejected: the GIL still serialises pure-Python code paths, and the Tracker's state-machine logic is not vectorisable. Latency variance was the deciding factor.
- **Pure C++ for everything including UI.** Rejected: the visualiser, calibration UI, and replay tooling change frequently and benefit from Python's iteration speed. Locking these into C++ would slow research velocity for no runtime benefit.
- **`pybind11` bindings between core and Python tooling.** Rejected for v0.3: couples Python's lifecycle to the C++ core process and complicates headless deployment. Reconsider in later phases if a Python consumer needs lower latency than ZeroMQ provides.

## Related ADRs
- ADR-002 (ZeroMQ Control Transport) — defines the inter-process boundary.
- ADR-007 (Geometric `safe_for_control` Predicate) — implemented in the C++ core.
