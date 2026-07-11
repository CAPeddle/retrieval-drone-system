# Tracking & Visualisation Core

C++ tracking pipeline for the Drone Ball Retrieval System. Detects the ball and laser, publishes state over ZeroMQ, and gates actuation via the `safe_for_control` predicate.

## Architecture

- **C++ Core (`src/core/`)**: Headless low-latency tracking pipeline (OpenCV + ZeroMQ PUB).
- **Python Viewer (`src/viewer/`)**: ZeroMQ SUB visualisation utility (OpenCV window rendering).
- **Transport**: ZeroMQ PUB/SUB only (ADR-002).

## Prerequisites

### Pi 5 (aarch64, primary target)

```bash
sudo apt update
sudo apt install -y \
    build-essential cmake git \
    libopencv-dev \
    libzmq3-dev
```

### x86_64 dev machine (Ubuntu/Debian)

```bash
sudo apt install -y \
    build-essential cmake git \
    libopencv-dev \
    libzmq3-dev
```

All other dependencies (cppzmq, nlohmann/json, yaml-cpp, spdlog, GoogleTest) are fetched automatically via CMake FetchContent.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run

```bash
# Terminal 1 — tracking core. Reads ./config/tracking_core.yaml by default
# (run from tracking-core/); pass an explicit path as the first argument.
./build/src/core/tracking_core
./build/src/core/tracking_core /etc/dbrs/tracking_core.yaml

# Terminal 2 — Python viewer
python3 src/viewer/viewer.py
```

## Configuration

Runtime configuration is a YAML file loaded once at startup (not hot-reloaded —
restart to apply changes). A template with defaults lives at
[`config/tracking_core.yaml`](config/tracking_core.yaml). Pass an explicit path
as the first CLI argument; otherwise `config/tracking_core.yaml` (relative to the
working directory) is used. A missing required field or a type mismatch aborts
startup with an error naming the offending field. All fields are required in v0.3.

| Field | Type | Meaning |
|-------|------|---------|
| `camera.device_id` | int | V4L2 device index (0 = /dev/video0) |
| `camera.target_fps` | int | Capture rate (v0.3 targets ≥60 fps) |
| `laser.modulation_frequency_hz` | double | ADR-005 modulation frequency (15 Hz) |
| `laser.modulation_duty_cycle` | double | ADR-005 duty cycle (0.0–1.0) |
| `safe_for_control.age_max_ms` | double | Max snapshot age before the predicate goes false |
| `safe_for_control.laser_settled_speed_m_per_s` | double | "Settled" laser speed threshold |
| `safe_for_control.alignment_tolerance_m` | double | `ALIGNMENT_TOLERANCE_M` (2 cm default) |
| `ball.radius_m` | double | Ball radius in metres |
| `zmq.bind_address` | string | Core BIND address (ADR-002) |
| `calibration.intrinsics_path` | string | Per-camera intrinsics JSON (ADR-004) |
| `calibration.extrinsics_path` | string | Floor-plane extrinsics JSON (ADR-006) |

## Test

```bash
# C++ unit tests
cd build && ctest --output-on-failure

# Python integration tests (from tracking-core/ root)
pytest tests/python_integration/
```

## Python dependencies

```bash
pip install -r requirements.txt
```

## CMake targets

| Target | Type | Purpose |
|--------|------|---------|
| `tracking_core_lib` | Static library | All pipeline sources (detector, tracker, etc.) |
| `tracking_core` | Executable | Main application (links tracking_core_lib) |
| `tracking_core_tests` | Executable | GTest unit tests (links tracking_core_lib) |
