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
# Terminal 1 — tracking core (camera 0, publishes tcp://*:5556)
./build/src/core/tracking_core

# Terminal 2 — Python viewer
python3 src/viewer/viewer.py
```

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
