---
id: TRK-002
status: blocked
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-06-04
spec: null
plan: null
blockers:
  - "Local Windows environment lacks OpenCV/libzmq/cv2 for build + test validation"
---

## Context

The current `tracking-core/CMakeLists.txt` started as a minimal scaffold that compiled `main.cpp` with OpenCV and ZMQ. v0.3 requires a production build system that manages all dependencies (OpenCV, ZeroMQ/cppzmq, a JSON library, a YAML library, a test framework, an async logger), supports native build on Pi 5 (aarch64), and enforces C++17 minimum. This is the foundation every subsequent ticket builds on.

## Acceptance

- Top-level `CMakeLists.txt` under `tracking-core/` (post TRK-001 rename) configures and builds cleanly on:
  - Pi 5 (aarch64, GCC 12+) natively.
  - x86_64 dev machine (GCC 12+ or Clang 15+) for rapid iteration.
- Dependencies declared and findable via `find_package` or `FetchContent`:
  - OpenCV 4.x (`core`, `imgproc`, `videoio`, `calib3d`, `aruco`)
  - cppzmq (header-only ZMQ C++ bindings)
  - nlohmann/json (or equivalent single-header JSON)
  - yaml-cpp (YAML parsing)
  - spdlog (async logging)
  - GoogleTest (unit testing)
- C++17 enforced as minimum standard (`CMAKE_CXX_STANDARD 17`).
- Separate CMake targets for: `tracking_core` (main executable), `tracking_core_lib` (static library for unit tests to link against), `tracking_core_tests` (test binary).
- `cmake --build` produces no warnings at `-Wall -Wextra -Wpedantic`.
- A `README.md` documents build prerequisites for Pi 5 and dev machine.

## Plan

U1. Rewrite `tracking-core/CMakeLists.txt` with project metadata, C++17 enforcement, warning flags.
U2. Add `FetchContent` blocks for: cppzmq, nlohmann/json, yaml-cpp, spdlog, GoogleTest.
U3. Keep OpenCV as a system `find_package` (too large for FetchContent; installed via `apt` on Pi).
U4. Define `tracking_core_lib` as a static library target (all pipeline sources minus `main.cpp`).
U5. Define `tracking_core` executable linking `tracking_core_lib`.
U6. Define `tracking_core_tests` executable under `tests/cpp_unit/`, linking `tracking_core_lib` + GTest.
U7. Add a `src/core/CMakeLists.txt` for the library sources (currently `detector.cpp`, `tracker.cpp`).
U8. Verify build on dev machine: `cmake -S tracking-core -B build && cmake --build build`.
U9. Update `tracking-core/README.md` with build prerequisites (`apt install` list for Pi 5).
U10. Run `tracking_core_tests` to confirm GTest scaffold passes.

## Log

- 2026-05-31: created. Status: backlog. First v0.3 implementation ticket; depends on TRK-001 completing the rename.
- 2026-06-01: backlog → in-progress. Starting build system overhaul.
- 2026-06-04: in-progress → blocked. Implementation complete; validation blocked on missing OpenCV/libzmq/cv2 in local Windows environment. Requires Pi 5 or Linux dev machine.
