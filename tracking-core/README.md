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
| `camera.width` | int | Capture width in pixels |
| `camera.height` | int | Capture height in pixels |
| `camera.exposure_us` | int | Manual exposure in µs, locked at calibration time (ADR-004) |
| `pipeline.ring_buffer_capacity` | int | Pre-allocated frame slots between capture and processing (1–64) |
| `pipeline.capture_cpu_core` | int | Core the ingestion thread pins to (0–63) |
| `pipeline.capture_thread_priority` | int | SCHED_FIFO priority (1–99); degrades with a WARN when unprivileged |
| `frame_quality.underexposed_threshold` | double | Histogram mean below this → frame rejected |
| `frame_quality.overexposed_threshold` | double | Histogram mean above this → frame rejected |
| `frame_quality.blur_threshold` | double | Laplacian variance below this → frame rejected |
| `laser.modulation_frequency_hz` | double | ADR-005 modulation frequency (15 Hz) |
| `laser.modulation_duty_cycle` | double | ADR-005 duty cycle (0.0–1.0) |
| `safe_for_control.age_max_ms` | double | Max snapshot age before the predicate goes false |
| `safe_for_control.laser_settled_speed_m_per_s` | double | "Settled" laser speed threshold |
| `safe_for_control.alignment_tolerance_m` | double | `ALIGNMENT_TOLERANCE_M` (2 cm default) |
| `ball.radius_m` | double | Ball radius in metres |
| `ball.expected_radius_px_min` | int | TRK-010 size gate lower bound (provisional) |
| `ball.expected_radius_px_max` | int | TRK-010 size gate upper bound (provisional) |
| `ball.min_circularity` | double | Minimum 4πA/P² for a ball candidate (0–1, provisional) |
| `ball.detection_blur_kernel` | int | Odd Gaussian kernel size, 3–15 (provisional) |
| `ball.brightness_threshold` | int | Fixed brightness prior 1–254; NoIR ball is bright (provisional) |
| `zmq.bind_address` | string | Core BIND address (ADR-002) |
| `calibration.intrinsics_path` | string | Per-camera intrinsics JSON (ADR-004) |
| `calibration.extrinsics_path` | string | Floor-plane extrinsics JSON (ADR-006) |
| `calibration.aruco_dictionary` | string | TRK-011 predefined dictionary (`4X4_50`/`4X4_100`/`5X5_50`) |
| `calibration.marker_ids` | int list | Static health-monitoring marker IDs (ADR-004 Phase 2), non-empty |
| `calibration.charuco.squares_x/y` | int | Board dimensions, both odd (KTD-4 future-proofing) |
| `calibration.charuco.square_length_m` | double | Physical square edge (provisional) |
| `calibration.charuco.marker_length_m` | double | Physical marker edge, < square (provisional) |
| `logging.level` | string | Runtime log floor: `trace`\|`debug`\|`info`\|`warn`\|`error`\|`critical` (Release builds compile out `debug`/`trace`) |
| `logging.output_dir` | string | Log directory — must be tmpfs, never the SD card (§7.1); init warns if not |
| `logging.max_file_size_mb` | int | Rotating-sink file size limit (3 rotated files kept) |

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
