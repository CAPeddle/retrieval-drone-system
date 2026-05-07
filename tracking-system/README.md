# Tracking & Visualisation Core

Foundational monorepo scaffold for the Drone Ball Retrieval System tracking subsystem.

## Architecture
- **C++ Core (`src/core/`)**: Headless low-latency tracking pipeline (OpenCV + ZeroMQ PUB).
- **Python Viewer (`src/viewer/`)**: ZeroMQ SUB visualisation utility (OpenCV window rendering).
- **Transport**: ZeroMQ PUB/SUB only.

## Build (C++)
```bash
cmake -S . -B build
cmake --build build
```

## Run
```bash
# Terminal 1
./build/src/core/tracking_core

# Terminal 2
python3 src/viewer/viewer.py
```

## Python dependencies
```bash
pip install -r requirements.txt
```
